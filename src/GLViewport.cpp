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

void GLViewport::setCutMethod(CutMethod method)
{
    m_cutMethod = method;
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
        glColor3f(0.97f, 0.93f, 0.82f);
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
        glColor3f(0.22f, 0.24f, 0.30f);
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
    if (m_mesh.isEmpty())
        return;

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    switch (m_cutMethod) {
    case CutMethod::Planar:  drawPlanarCutViz();  break;
    case CutMethod::Conic:   drawConicCutViz();   break;
    case CutMethod::Flexi:   drawFlexiCutViz();   break;
    case CutMethod::Grid:    drawGridCutViz();     break;
    case CutMethod::Radial:  drawRadialCutViz();  break;
    case CutMethod::Modular: drawModularCutViz(); break;
    }

    glDisable(GL_BLEND);
}

namespace {
void planeColor(bool adjusted)
{
    if (adjusted)
        glColor4f(0.85f, 0.18f, 0.18f, 0.24f);
    else
        glColor4f(0.10f, 0.75f, 0.85f, 0.18f);
}
} // namespace

void GLViewport::drawGridCutViz()
{
    const QVector3D mn = m_mesh.minBounds();
    const QVector3D mx = m_mesh.maxBounds();
    const float margin = qMax((mx - mn).length() * 0.03f, 0.001f);
    const QVector3D a = mn - QVector3D(margin, margin, margin);
    const QVector3D b = mx + QVector3D(margin, margin, margin);

    auto drawX = [&](float x, bool adj) {
        planeColor(adj);
        glBegin(GL_QUADS);
        glVertex3f(x, a.y(), a.z()); glVertex3f(x, b.y(), a.z());
        glVertex3f(x, b.y(), b.z()); glVertex3f(x, a.y(), b.z());
        glEnd();
    };
    auto drawY = [&](float y, bool adj) {
        planeColor(adj);
        glBegin(GL_QUADS);
        glVertex3f(a.x(), y, a.z()); glVertex3f(b.x(), y, a.z());
        glVertex3f(b.x(), y, b.z()); glVertex3f(a.x(), y, b.z());
        glEnd();
    };
    auto drawZ = [&](float z, bool adj) {
        planeColor(adj);
        glBegin(GL_QUADS);
        glVertex3f(a.x(), a.y(), z); glVertex3f(b.x(), a.y(), z);
        glVertex3f(b.x(), b.y(), z); glVertex3f(a.x(), b.y(), z);
        glEnd();
    };

    auto drawAxis = [&](Axis axis, int count) {
        const float minV = axis == Axis::X ? a.x() : (axis == Axis::Y ? a.y() : a.z());
        const float maxV = axis == Axis::X ? b.x() : (axis == Axis::Y ? b.y() : b.z());
        if (count <= 1) {
            const bool adj = (axis == m_adjustedAxis) && (m_adjustedIndex == 0);
            const float mid = 0.5f * (minV + maxV);
            if (axis == Axis::X) drawX(mid, adj);
            else if (axis == Axis::Y) drawY(mid, adj);
            else drawZ(mid, adj);
            return;
        }
        for (int i = 0; i < count; ++i) {
            const float p = minV + float(i + 1) / float(count + 1) * (maxV - minV);
            const bool adj = (axis == m_adjustedAxis) && (i == m_adjustedIndex);
            if (axis == Axis::X) drawX(p, adj);
            else if (axis == Axis::Y) drawY(p, adj);
            else drawZ(p, adj);
        }
    };

    drawAxis(Axis::X, m_cutX);
    drawAxis(Axis::Y, m_cutY);
    drawAxis(Axis::Z, m_cutZ);
}

void GLViewport::drawPlanarCutViz()
{
    const QVector3D mn = m_mesh.minBounds();
    const QVector3D mx = m_mesh.maxBounds();
    const float margin = qMax((mx - mn).length() * 0.03f, 0.001f);
    const QVector3D a = mn - QVector3D(margin, margin, margin);
    const QVector3D b = mx + QVector3D(margin, margin, margin);

    const Axis axis = m_adjustedAxis;
    const int count = (axis == Axis::X) ? m_cutX : (axis == Axis::Y ? m_cutY : m_cutZ);
    const float minV = (axis == Axis::X) ? a.x() : (axis == Axis::Y ? a.y() : a.z());
    const float maxV = (axis == Axis::X) ? b.x() : (axis == Axis::Y ? b.y() : b.z());

    for (int i = 0; i < count; ++i) {
        const float p = minV + float(i + 1) / float(count + 1) * (maxV - minV);
        planeColor(i == m_adjustedIndex);
        glBegin(GL_QUADS);
        if (axis == Axis::X) {
            glVertex3f(p, a.y(), a.z()); glVertex3f(p, b.y(), a.z());
            glVertex3f(p, b.y(), b.z()); glVertex3f(p, a.y(), b.z());
        } else if (axis == Axis::Y) {
            glVertex3f(a.x(), p, a.z()); glVertex3f(b.x(), p, a.z());
            glVertex3f(b.x(), p, b.z()); glVertex3f(a.x(), p, b.z());
        } else {
            glVertex3f(a.x(), a.y(), p); glVertex3f(b.x(), a.y(), p);
            glVertex3f(b.x(), b.y(), p); glVertex3f(a.x(), b.y(), p);
        }
        glEnd();
    }
}

void GLViewport::drawRadialCutViz()
{
    const QVector3D mn = m_mesh.minBounds();
    const QVector3D mx = m_mesh.maxBounds();
    const float margin = qMax((mx - mn).length() * 0.03f, 0.001f);
    const float cx  = (mn.x() + mx.x()) * 0.5f;
    const float cz  = (mn.z() + mx.z()) * 0.5f;
    const float yBot = mn.y() - margin;
    const float yTop = mx.y() + margin;
    const float radius = (qMax(mx.x() - mn.x(), mx.z() - mn.z()) * 0.5f + margin) * 1.2f;

    const int N = m_cutX;
    for (int i = 0; i < N; ++i) {
        const float angle = float(M_PI) * float(i) / float(N);
        const float dx = std::cos(angle) * radius;
        const float dz = std::sin(angle) * radius;
        planeColor(m_adjustedAxis == Axis::X && i == m_adjustedIndex);
        glBegin(GL_QUADS);
        glVertex3f(cx - dx, yBot, cz - dz); glVertex3f(cx + dx, yBot, cz + dz);
        glVertex3f(cx + dx, yTop, cz + dz); glVertex3f(cx - dx, yTop, cz - dz);
        glEnd();
    }
}

void GLViewport::drawConicCutViz()
{
    const QVector3D mn = m_mesh.minBounds();
    const QVector3D mx = m_mesh.maxBounds();
    const float margin = qMax((mx - mn).length() * 0.03f, 0.001f);
    const float cx  = (mn.x() + mx.x()) * 0.5f;
    const float cz  = (mn.z() + mx.z()) * 0.5f;
    const float yBot = mn.y() - margin;
    const float yTop = mx.y() + margin;
    const float maxR = qMax(mx.x() - mn.x(), mx.z() - mn.z()) * 0.5f + margin;

    const int N = m_cutX;
    const int sides = 24;
    for (int ring = 0; ring < N; ++ring) {
        const float r = maxR * float(ring + 1) / float(N + 1);
        planeColor(m_adjustedAxis == Axis::X && ring == m_adjustedIndex);
        glBegin(GL_QUADS);
        for (int s = 0; s < sides; ++s) {
            const float a0 = 2.0f * float(M_PI) * float(s)     / float(sides);
            const float a1 = 2.0f * float(M_PI) * float(s + 1) / float(sides);
            glVertex3f(cx + r * std::cos(a0), yBot, cz + r * std::sin(a0));
            glVertex3f(cx + r * std::cos(a1), yBot, cz + r * std::sin(a1));
            glVertex3f(cx + r * std::cos(a1), yTop, cz + r * std::sin(a1));
            glVertex3f(cx + r * std::cos(a0), yTop, cz + r * std::sin(a0));
        }
        glEnd();
    }
}

void GLViewport::drawFlexiCutViz()
{
    const QVector3D mn = m_mesh.minBounds();
    const QVector3D mx = m_mesh.maxBounds();
    const float margin = qMax((mx - mn).length() * 0.03f, 0.001f);
    const QVector3D a = mn - QVector3D(margin, margin, margin);
    const QVector3D b = mx + QVector3D(margin, margin, margin);
    const float ampX = (b.z() - a.z()) * 0.07f;
    const float ampY = (b.y() - a.y()) * 0.05f;
    const int segs = 20;

    // Wavy X planes
    for (int i = 0; i < m_cutX; ++i) {
        const float xBase = a.x() + float(i + 1) / float(m_cutX + 1) * (b.x() - a.x());
        const float phase = float(i) * 1.3f;
        planeColor(m_adjustedAxis == Axis::X && i == m_adjustedIndex);
        glBegin(GL_QUADS);
        for (int s = 0; s < segs; ++s) {
            const float t0 = float(s)     / float(segs);
            const float t1 = float(s + 1) / float(segs);
            const float z0 = a.z() + t0 * (b.z() - a.z());
            const float z1 = a.z() + t1 * (b.z() - a.z());
            const float x0 = xBase + ampX * std::sin(t0 * float(M_PI) * 2.0f + phase);
            const float x1 = xBase + ampX * std::sin(t1 * float(M_PI) * 2.0f + phase);
            glVertex3f(x0, a.y(), z0); glVertex3f(x1, a.y(), z1);
            glVertex3f(x1, b.y(), z1); glVertex3f(x0, b.y(), z0);
        }
        glEnd();
    }

    // Wavy Y planes
    for (int i = 0; i < m_cutY; ++i) {
        const float yBase = a.y() + float(i + 1) / float(m_cutY + 1) * (b.y() - a.y());
        const float phase = float(i) * 0.9f;
        planeColor(m_adjustedAxis == Axis::Y && i == m_adjustedIndex);
        glBegin(GL_QUADS);
        for (int s = 0; s < segs; ++s) {
            const float t0 = float(s)     / float(segs);
            const float t1 = float(s + 1) / float(segs);
            const float x0 = a.x() + t0 * (b.x() - a.x());
            const float x1 = a.x() + t1 * (b.x() - a.x());
            const float y0 = yBase + ampY * std::sin(t0 * float(M_PI) * 2.0f + phase);
            const float y1 = yBase + ampY * std::sin(t1 * float(M_PI) * 2.0f + phase);
            glVertex3f(x0, y0, a.z()); glVertex3f(x1, y1, a.z());
            glVertex3f(x1, y1, b.z()); glVertex3f(x0, y0, b.z());
        }
        glEnd();
    }
}

void GLViewport::drawModularCutViz()
{
    const QVector3D mn = m_mesh.minBounds();
    const QVector3D mx = m_mesh.maxBounds();
    const float margin = qMax((mx - mn).length() * 0.03f, 0.001f);
    const QVector3D a = mn - QVector3D(margin, margin, margin);
    const QVector3D b = mx + QVector3D(margin, margin, margin);

    const int NX = m_cutX;
    const int NZ = m_cutZ;
    const float xCell   = (b.x() - a.x()) / float(NX + 1);
    const float zCell   = (b.z() - a.z()) / float(NZ + 1);
    const float brickOff = xCell * 0.5f;

    // Z separator planes (full width)
    for (int i = 0; i < NZ; ++i) {
        const float z = a.z() + float(i + 1) * zCell;
        planeColor(m_adjustedAxis == Axis::Z && i == m_adjustedIndex);
        glBegin(GL_QUADS);
        glVertex3f(a.x(), a.y(), z); glVertex3f(b.x(), a.y(), z);
        glVertex3f(b.x(), b.y(), z); glVertex3f(a.x(), b.y(), z);
        glEnd();
    }

    // X planes per Z row with brick offset on odd rows
    for (int iz = 0; iz <= NZ; ++iz) {
        const float z0 = a.z() + float(iz) * zCell;
        const float z1 = qMin(z0 + zCell, b.z());
        const float off = (iz % 2 == 1) ? brickOff : 0.0f;
        for (int i = 0; i < NX; ++i) {
            const float x = a.x() + float(i + 1) * xCell + off;
            if (x <= a.x() || x >= b.x()) continue;
            planeColor(m_adjustedAxis == Axis::X && i == m_adjustedIndex);
            glBegin(GL_QUADS);
            glVertex3f(x, a.y(), z0); glVertex3f(x, b.y(), z0);
            glVertex3f(x, b.y(), z1); glVertex3f(x, a.y(), z1);
            glEnd();
        }
    }

    // Y planes
    for (int i = 0; i < m_cutY; ++i) {
        const float y = a.y() + float(i + 1) / float(m_cutY + 1) * (b.y() - a.y());
        planeColor(m_adjustedAxis == Axis::Y && i == m_adjustedIndex);
        glBegin(GL_QUADS);
        glVertex3f(a.x(), y, a.z()); glVertex3f(b.x(), y, a.z());
        glVertex3f(b.x(), y, b.z()); glVertex3f(a.x(), y, b.z());
        glEnd();
    }
}
