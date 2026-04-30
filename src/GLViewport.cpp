#include "GLViewport.h"

#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>

#include <QtMath>

namespace {
float niceGridStep(float fullWidth, int targetDivisions = 14)
{
    if (fullWidth <= 1e-6f) {
        return 0.1f;
    }
    const float raw = fullWidth / float(targetDivisions);
    const float exp = std::floor(std::log10(raw));
    const float base = std::pow(10.0f, exp);
    const float mant = raw / base;
    float nice = 10.0f;
    if (mant <= 1.0f) {
        nice = 1.0f;
    } else if (mant <= 2.0f) {
        nice = 2.0f;
    } else if (mant <= 5.0f) {
        nice = 5.0f;
    }
    return nice * base;
}
} // namespace

GLViewport::GLViewport(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
}

void GLViewport::setMesh(const MeshObject& mesh)
{
    m_mesh = mesh;
    refreshGridFromMesh();
    fitView();
    update();
}

void GLViewport::clearMesh()
{
    m_mesh.clear();
    refreshGridFromMesh();
    fitView();
    update();
}

void GLViewport::setRenderMode(RenderMode mode)
{
    m_renderMode = mode;
    update();
}

void GLViewport::setCuttingPlaneConfig(int countX, int countY, int countZ, Axis adjustedAxis, int adjustedIndex)
{
    m_cutX = qMax(1, countX);
    m_cutY = qMax(1, countY);
    m_cutZ = qMax(1, countZ);
    m_adjustedAxis = adjustedAxis;
    m_adjustedIndex = qMax(0, adjustedIndex);
    update();
}

void GLViewport::fitView()
{
    if (m_mesh.isEmpty()) {
        m_distance = 5.0f;
        m_pan = QVector3D(0, 0, 0);
        return;
    }
    const float r = qMax(0.01f, m_mesh.radius());
    m_distance = r * 2.6f;
    m_pan = -m_mesh.center();
}

void GLViewport::refreshGridFromMesh()
{
    if (m_mesh.isEmpty()) {
        m_gridExtent = 2.0f;
        m_gridStep = 0.4f;
        m_axesLength = 1.0f;
        return;
    }
    const QVector3D mn = m_mesh.minBounds();
    const QVector3D mx = m_mesh.maxBounds();
    const float halfW = qMax(qAbs(mn.x()), qAbs(mx.x()));
    const float halfD = qMax(qAbs(mn.z()), qAbs(mx.z()));
    const float half = qMax(qMax(halfW, halfD), 0.5f * (mx.y() - mn.y()));
    m_gridExtent = qMax(half * 1.4f, 1.0f);
    const float full = m_gridExtent * 2.0f;
    m_gridStep = niceGridStep(full);
    if (m_gridStep < full / 200.0f) {
        m_gridStep = full / 40.0f;
    }
    m_axesLength = qBound(0.08f * m_gridExtent, m_mesh.radius() * 0.3f, m_gridExtent * 0.35f);
}

void GLViewport::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
}

void GLViewport::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void GLViewport::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    applyCamera();

    drawGrid(m_gridExtent, m_gridStep);
    drawAxes(m_axesLength);

    switch (m_renderMode) {
    case RenderMode::Wireframe:
        drawMesh(true, false);
        break;
    case RenderMode::Solid:
        drawMesh(false, false);
        break;
    case RenderMode::SolidWire:
        drawMesh(false, true);
        break;
    }

    drawCuttingPlanes();
}

void GLViewport::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePos = event->pos();
}

void GLViewport::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();

    if (event->buttons() & Qt::LeftButton) {
        m_yawDeg += delta.x() * 0.5f;
        m_pitchDeg += delta.y() * 0.5f;
        m_pitchDeg = qBound(-89.0f, m_pitchDeg, 89.0f);
        update();
    } else if (event->buttons() & Qt::MiddleButton) {
        const float scale = m_distance * 0.0025f;
        m_pan += QVector3D(delta.x() * scale, -delta.y() * scale, 0.0f);
        update();
    }
}

void GLViewport::wheelEvent(QWheelEvent* event)
{
    const float notches = event->angleDelta().y() / 120.0f;
    m_distance *= qPow(0.86f, notches);
    m_distance = qMax(0.05f, m_distance);
    update();
}

void GLViewport::applyCamera()
{
    const float aspect = float(width()) / float(qMax(1, height()));
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const float fovy = 45.0f;
    const float zNear = 0.01f;
    const float r = m_mesh.isEmpty() ? 10.0f : m_mesh.radius();
    const float zFar = qMax(5000.0f, r * 80.0f);
    const float top = zNear * qTan(qDegreesToRadians(fovy * 0.5f));
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, zNear, zFar);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -m_distance);
    glRotatef(m_pitchDeg, 1.0f, 0.0f, 0.0f);
    glRotatef(m_yawDeg, 0.0f, 1.0f, 0.0f);
    glTranslatef(m_pan.x(), m_pan.y(), m_pan.z());
}

void GLViewport::drawGrid(float extent, float step)
{
    glDisable(GL_LIGHTING);
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(1, 0x3333);
    glColor3f(0.25f, 0.27f, 0.30f);
    glBegin(GL_LINES);
    for (float i = -extent; i <= extent; i += step) {
        glVertex3f(-extent, 0.0f, i);
        glVertex3f(extent, 0.0f, i);
        glVertex3f(i, 0.0f, -extent);
        glVertex3f(i, 0.0f, extent);
    }
    glEnd();
    glDisable(GL_LINE_STIPPLE);
}

void GLViewport::drawAxes(float length)
{
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor3f(0.9f, 0.2f, 0.2f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(length, 0.0f, 0.0f);

    glColor3f(0.2f, 0.9f, 0.2f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, length, 0.0f);

    glColor3f(0.2f, 0.4f, 0.95f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, length);
    glEnd();
    glLineWidth(1.0f);
}

void GLViewport::drawMesh(bool wireframeOnly, bool overlayWire)
{
    if (m_mesh.isEmpty()) {
        return;
    }

    const auto& verts = m_mesh.vertices();
    const auto& tris = m_mesh.triangles();
    const auto& norms = m_mesh.faceNormals();

    if (!wireframeOnly) {
        glEnable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glColor3f(0.75f, 0.76f, 0.82f);
        glBegin(GL_TRIANGLES);
        for (int i = 0; i < tris.size(); ++i) {
            const MeshObject::Triangle& tri = tris[i];
            const QVector3D& n = norms[i];
            glNormal3f(n.x(), n.y(), n.z());

            const QVector3D& a = verts[tri.v0];
            const QVector3D& b = verts[tri.v1];
            const QVector3D& c = verts[tri.v2];
            glVertex3f(a.x(), a.y(), a.z());
            glVertex3f(b.x(), b.y(), b.z());
            glVertex3f(c.x(), c.y(), c.z());
        }
        glEnd();
    }

    if (wireframeOnly || overlayWire) {
        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glColor3f(0.05f, 0.05f, 0.08f);
        glLineWidth(1.2f);
        glBegin(GL_TRIANGLES);
        for (const MeshObject::Triangle& tri : tris) {
            const QVector3D& a = verts[tri.v0];
            const QVector3D& b = verts[tri.v1];
            const QVector3D& c = verts[tri.v2];
            glVertex3f(a.x(), a.y(), a.z());
            glVertex3f(b.x(), b.y(), b.z());
            glVertex3f(c.x(), c.y(), c.z());
        }
        glEnd();
        glLineWidth(1.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

void GLViewport::drawCuttingPlanes()
{
    if (m_mesh.isEmpty()) {
        return;
    }

    const QVector3D mn = m_mesh.minBounds();
    const QVector3D mx = m_mesh.maxBounds();
    const float margin = qMax((mx - mn).length() * 0.03f, 0.001f);
    const QVector3D a = mn - QVector3D(margin, margin, margin);
    const QVector3D b = mx + QVector3D(margin, margin, margin);

    auto drawX = [&](float x, bool adjusted) {
        glColor4f(adjusted ? 0.90f : 0.05f, adjusted ? 0.20f : 0.80f, adjusted ? 0.20f : 0.85f, 0.45f);
        glBegin(GL_QUADS);
        glVertex3f(x, a.y(), a.z());
        glVertex3f(x, b.y(), a.z());
        glVertex3f(x, b.y(), b.z());
        glVertex3f(x, a.y(), b.z());
        glEnd();
    };
    auto drawY = [&](float y, bool adjusted) {
        glColor4f(adjusted ? 0.90f : 0.05f, adjusted ? 0.20f : 0.80f, adjusted ? 0.20f : 0.85f, 0.45f);
        glBegin(GL_QUADS);
        glVertex3f(a.x(), y, a.z());
        glVertex3f(b.x(), y, a.z());
        glVertex3f(b.x(), y, b.z());
        glVertex3f(a.x(), y, b.z());
        glEnd();
    };
    auto drawZ = [&](float z, bool adjusted) {
        glColor4f(adjusted ? 0.90f : 0.05f, adjusted ? 0.20f : 0.80f, adjusted ? 0.20f : 0.85f, 0.45f);
        glBegin(GL_QUADS);
        glVertex3f(a.x(), a.y(), z);
        glVertex3f(b.x(), a.y(), z);
        glVertex3f(b.x(), b.y(), z);
        glVertex3f(a.x(), b.y(), z);
        glEnd();
    };

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto drawAxisPlanes = [&](Axis axis, int count) {
        const float minV = axis == Axis::X ? a.x() : (axis == Axis::Y ? a.y() : a.z());
        const float maxV = axis == Axis::X ? b.x() : (axis == Axis::Y ? b.y() : b.z());
        if (count <= 1) {
            const bool adjusted = (axis == m_adjustedAxis) && (m_adjustedIndex == 0);
            if (axis == Axis::X) {
                drawX(0.5f * (minV + maxV), adjusted);
            } else if (axis == Axis::Y) {
                drawY(0.5f * (minV + maxV), adjusted);
            } else {
                drawZ(0.5f * (minV + maxV), adjusted);
            }
            return;
        }
        for (int i = 0; i < count; ++i) {
            const float t = float(i + 1) / float(count + 1);
            const float p = minV + t * (maxV - minV);
            const bool adjusted = (axis == m_adjustedAxis) && (i == m_adjustedIndex);
            if (axis == Axis::X) {
                drawX(p, adjusted);
            } else if (axis == Axis::Y) {
                drawY(p, adjusted);
            } else {
                drawZ(p, adjusted);
            }
        }
    };

    drawAxisPlanes(Axis::X, m_cutX);
    drawAxisPlanes(Axis::Y, m_cutY);
    drawAxisPlanes(Axis::Z, m_cutZ);

    glDisable(GL_BLEND);
}
