#include "GLViewport.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
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
    setMouseTracking(true);
}

bool GLViewport::MeshTransform::isIdentity() const
{
    return qFuzzyIsNull(tx) && qFuzzyIsNull(ty) && qFuzzyIsNull(tz) &&
           qFuzzyIsNull(rx) && qFuzzyIsNull(ry) && qFuzzyIsNull(rz) &&
           qFuzzyCompare(scale, 1.0f);
}

void GLViewport::setMeshTransform(const MeshTransform& xf)
{
    m_xform = xf;
    update();
}

void GLViewport::resetMeshTransform()
{
    m_xform = MeshTransform{};
    update();
}

void GLViewport::setShowCuttingPlanes(bool show)
{
    m_showCuttingPlanes = show;
    update();
}

void GLViewport::setMeshVisible(bool visible)
{
    m_meshVisible = visible;
    update();
}

void GLViewport::setDarkMode(bool dark)
{
    m_darkMode = dark;
    update();
}

void GLViewport::applyMeshTransform()
{
    if (m_xform.isIdentity()) return;
    const QVector3D c = m_mesh.center();
    glTranslatef(m_xform.tx, m_xform.ty, m_xform.tz);
    glTranslatef(c.x(), c.y(), c.z());
    glRotatef(m_xform.rx, 1.f, 0.f, 0.f);
    glRotatef(m_xform.ry, 0.f, 1.f, 0.f);
    glRotatef(m_xform.rz, 0.f, 0.f, 1.f);
    glScalef(m_xform.scale, m_xform.scale, m_xform.scale);
    glTranslatef(-c.x(), -c.y(), -c.z());
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
    m_meshSelected = false;
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
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
}

void GLViewport::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void GLViewport::paintGL()
{
    if (m_darkMode)
        glClearColor(0.08f, 0.10f, 0.18f, 1.0f);
    else
        glClearColor(0.97f, 0.97f, 0.97f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    applyCamera();

    // Fix light positions in world space (must happen while view matrix is current)
    {
        const GLfloat p0[] = {  0.6f,  1.8f,  1.2f, 0.0f };  // key: upper-left-front
        const GLfloat p1[] = { -0.8f, -0.4f, -0.6f, 0.0f };  // fill: lower-right-back
        const GLfloat p2[] = {  0.0f,  0.5f, -1.5f, 0.0f };  // rim: behind
        glLightfv(GL_LIGHT0, GL_POSITION, p0);
        glLightfv(GL_LIGHT1, GL_POSITION, p1);
        glLightfv(GL_LIGHT2, GL_POSITION, p2);
    }

    drawGrid(m_gridExtent, m_gridStep);
    drawAxes(m_axesLength);

    // Apply live mesh transform (move/rotate/scale preview)
    glPushMatrix();
    applyMeshTransform();

    // Selection outline — stencil approach
    const bool doOutline = m_meshSelected && m_meshVisible && !m_mesh.isEmpty();
    if (doOutline) {
        glEnable(GL_STENCIL_TEST);
        glStencilMask(0xFF);
        glClear(GL_STENCIL_BUFFER_BIT);
        // Pass 1: mark mesh footprint in stencil (no color/depth write)
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);
        drawMeshRaw();
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        // Normal mesh draw passes write everywhere (stencil = 1 already set)
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    }

    if (m_meshVisible) {
        switch (m_renderMode) {
        case RenderMode::Wireframe: drawMesh(true,  false); break;
        case RenderMode::Solid:     drawMesh(false, false); break;
        case RenderMode::SolidWire: drawMesh(false, true);  break;
        }
    }

    // Outline pass: draw 2% scaled-up version only outside the stencil mask
    if (doOutline) {
        glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        const QVector3D c = m_mesh.center();
        glPushMatrix();
        glTranslatef(c.x(), c.y(), c.z());
        glScalef(1.025f, 1.025f, 1.025f);
        glTranslatef(-c.x(), -c.y(), -c.z());
        if (m_darkMode) glColor3f(1.0f, 1.0f, 1.0f);
        else            glColor3f(0.08f, 0.10f, 0.16f);
        drawMeshRaw();
        glPopMatrix();
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glStencilMask(0xFF);
    }

    if (m_showCuttingPlanes)
        drawCuttingPlanes();
    glPopMatrix();

    drawViewCubeGL();
}

void GLViewport::paintEvent(QPaintEvent* event)
{
    QOpenGLWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    drawViewCubeLabels(painter);
    drawTransformOverlay(painter);
}

// ── Blender-style modal transform ─────────────────────────────────────────────

void GLViewport::startTx(TxMode mode)
{
    m_txMode  = mode;
    m_txAxis  = TxAxis::Free;
    m_txStart = mapFromGlobal(QCursor::pos());
    m_txSaved = m_xform;
    setCursor(Qt::CrossCursor);
    update();
}

void GLViewport::updateTx(const QPoint& pos)
{
    const float dx = float(pos.x() - m_txStart.x());
    MeshTransform xf = m_txSaved;

    switch (m_txMode) {
    case TxMode::Grab: {
        const float sens = m_distance * 0.002f;
        switch (m_txAxis) {
        case TxAxis::X:    xf.tx += dx * sens; break;
        case TxAxis::Y:    xf.ty -= float(pos.y() - m_txStart.y()) * sens; break;
        case TxAxis::Z:    xf.tz += dx * sens; break;
        case TxAxis::Free: xf.tx += dx * sens;
                           xf.tz += float(pos.y() - m_txStart.y()) * sens; break;
        }
        break;
    }
    case TxMode::Rotate: {
        const float deg = dx * 0.5f;
        switch (m_txAxis) {
        case TxAxis::X:    xf.rx = m_txSaved.rx + deg; break;
        case TxAxis::Z:    xf.rz = m_txSaved.rz + deg; break;
        case TxAxis::Y:
        case TxAxis::Free: xf.ry = m_txSaved.ry + deg; break;
        }
        break;
    }
    case TxMode::Scale: {
        // Exponential so left/right feel symmetric
        const float factor = std::exp(dx / 250.f);
        xf.scale = qMax(0.001f, m_txSaved.scale * factor);
        break;
    }
    case TxMode::None: break;
    }

    m_xform = xf;
    emit meshTransformChanged(m_xform);
    update();
}

void GLViewport::confirmTx()
{
    m_txMode = TxMode::None;
    unsetCursor();
    update();
}

void GLViewport::cancelTx()
{
    m_xform = m_txSaved;
    m_txMode = TxMode::None;
    emit meshTransformChanged(m_xform);
    unsetCursor();
    update();
}

void GLViewport::drawTransformOverlay(QPainter& painter) const
{
    if (m_txMode == TxMode::None) return;

    const QString modeStr = (m_txMode == TxMode::Grab)   ? QStringLiteral("GRAB")
                          : (m_txMode == TxMode::Rotate) ? QStringLiteral("ROTATE")
                                                          : QStringLiteral("SCALE");
    const QString axisStr = (m_txAxis == TxAxis::X) ? QStringLiteral("  X axis")
                          : (m_txAxis == TxAxis::Y) ? QStringLiteral("  Y axis")
                          : (m_txAxis == TxAxis::Z) ? QStringLiteral("  Z axis")
                                                    : QStringLiteral("  (free)");
    const QString text = modeStr + axisStr + QStringLiteral("   ⏎ confirm  ⎋ cancel");

    QFont f = painter.font();
    f.setPointSize(10);
    f.setBold(true);
    painter.setFont(f);

    const QFontMetrics fm(f);
    const int textW = fm.horizontalAdvance(text) + 20;
    const int textH = fm.height() + 12;
    const int bx    = (width() - textW) / 2;
    const int by    = height() - textH - 28; // above timeline

    painter.setOpacity(0.85);
    painter.fillRect(bx - 4, by - 4, textW + 8, textH + 8, QColor(0x1a, 0x1a, 0x1e));
    painter.setOpacity(1.0);
    painter.setPen(QColor(0xff, 0xb3, 0x00));
    painter.drawText(QRect(bx, by, textW, textH), Qt::AlignCenter, text);
}

// ── Input events ──────────────────────────────────────────────────────────────

void GLViewport::keyPressEvent(QKeyEvent* event)
{
    if (m_txMode == TxMode::None) {
        switch (event->key()) {
        case Qt::Key_G: startTx(TxMode::Grab);   return;
        case Qt::Key_R: startTx(TxMode::Rotate); return;
        case Qt::Key_S: startTx(TxMode::Scale);  return;
        case Qt::Key_Escape:
            if (m_meshSelected) { m_meshSelected = false; update(); return; }
            break;
        default: break;
        }
    } else {
        switch (event->key()) {
        case Qt::Key_X: m_txAxis = TxAxis::X; updateTx(mapFromGlobal(QCursor::pos())); return;
        case Qt::Key_Y: m_txAxis = TxAxis::Y; updateTx(mapFromGlobal(QCursor::pos())); return;
        case Qt::Key_Z: m_txAxis = TxAxis::Z; updateTx(mapFromGlobal(QCursor::pos())); return;
        case Qt::Key_Return:
        case Qt::Key_Enter:  confirmTx(); return;
        case Qt::Key_Escape: cancelTx();  return;
        default: break;
        }
    }
    QOpenGLWidget::keyPressEvent(event);
}

void GLViewport::mousePressEvent(QMouseEvent* event)
{
    if (m_txMode != TxMode::None) {
        if (event->button() == Qt::LeftButton)  { confirmTx(); return; }
        if (event->button() == Qt::RightButton) { cancelTx();  return; }
    }
    if (handleViewCubeClick(event->pos()))
        return;
    m_pressPos     = event->pos();
    m_lastMousePos = event->pos();
    m_dragging     = false;
}

void GLViewport::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    if (m_txMode != TxMode::None) return;
    if (m_dragging) return;
    if (handleViewCubeClick(event->pos())) return;
    if (!m_mesh.isEmpty()) {
        m_meshSelected = true;
        update();
    }
}

void GLViewport::mouseMoveEvent(QMouseEvent* event)
{
    // Route to transform if active
    if (m_txMode != TxMode::None) {
        updateTx(event->pos());
        return;
    }

    const QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();

    if (event->buttons() & Qt::LeftButton) {
        if ((event->pos() - m_pressPos).manhattanLength() > 5)
            m_dragging = true;
        if (!viewCubeRect().contains(event->pos() - delta)) {
            m_yawDeg += delta.x() * 0.5f;
            m_pitchDeg += delta.y() * 0.5f;
            m_pitchDeg = qBound(-89.0f, m_pitchDeg, 89.0f);
            update();
        }
    } else if (event->buttons() & Qt::MiddleButton) {
        const float scale = m_distance * 0.0025f;
        m_pan += QVector3D(delta.x() * scale, -delta.y() * scale, 0.0f);
        update();
    }

    if (event->buttons() == Qt::NoButton) {
        const int newHover = nearestVisibleFace(event->pos());
        if (newHover != m_hoveredFace) {
            m_hoveredFace = newHover;
            setCursor(m_hoveredFace >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
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
    glDisable(GL_LINE_STIPPLE);

    // Minor lines — 4× finer, dimmer
    const float minorStep = step / 4.0f;
    const float minC = m_darkMode ? 0.11f : 0.72f;
    glLineWidth(0.6f);
    glColor3f(minC, minC, minC);
    glBegin(GL_LINES);
    for (float i = -extent; i <= extent + 1e-4f; i += minorStep) {
        const float fi = std::round(i / step) * step;
        if (std::fabs(i - fi) < minorStep * 0.45f) continue; // skip major positions
        glVertex3f(-extent, 0.0f, i);
        glVertex3f( extent, 0.0f, i);
        glVertex3f(i, 0.0f, -extent);
        glVertex3f(i, 0.0f,  extent);
    }
    glEnd();

    // Major lines — brighter, slightly thicker
    const float majC = m_darkMode ? 0.22f : 0.55f;
    glLineWidth(1.0f);
    glColor3f(majC, majC, majC);
    glBegin(GL_LINES);
    for (float i = -extent; i <= extent + 1e-4f; i += step) {
        glVertex3f(-extent, 0.0f, i);
        glVertex3f( extent, 0.0f, i);
        glVertex3f(i, 0.0f, -extent);
        glVertex3f(i, 0.0f,  extent);
    }
    glEnd();
    glLineWidth(1.0f);
}

void GLViewport::drawAxes(float length)
{
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor3f(0.65f, 0.14f, 0.14f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(length, 0.0f, 0.0f);

    glColor3f(0.14f, 0.60f, 0.14f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, length, 0.0f);

    glColor3f(0.14f, 0.30f, 0.72f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, length);
    glEnd();
    glLineWidth(1.0f);
}

void GLViewport::drawMesh(bool wireframeOnly, bool overlayWire)
{
    if (m_mesh.isEmpty()) return;

    const auto& verts  = m_mesh.vertices();
    const auto& tris   = m_mesh.triangles();
    const auto& vnorms = m_mesh.vertexNormals();
    const bool  hasVN  = (vnorms.size() == verts.size());

    if (!wireframeOnly) {
        // ── 3-light Phong shading (F360 style) ───────────────────────────────
        glEnable(GL_LIGHTING);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

        // Key light — warm white from upper-left-front
        glEnable(GL_LIGHT0);
        const GLfloat d0[] = { 0.80f, 0.80f, 0.78f, 1.0f };
        const GLfloat s0[] = { 0.40f, 0.40f, 0.38f, 1.0f };
        const GLfloat a0[] = { 0.0f,  0.0f,  0.0f,  1.0f };
        glLightfv(GL_LIGHT0, GL_DIFFUSE,  d0);
        glLightfv(GL_LIGHT0, GL_SPECULAR, s0);
        glLightfv(GL_LIGHT0, GL_AMBIENT,  a0);

        // Fill light — cool dim from lower-right-back
        glEnable(GL_LIGHT1);
        const GLfloat d1[] = { 0.18f, 0.18f, 0.22f, 1.0f };
        const GLfloat s1[] = { 0.0f,  0.0f,  0.0f,  1.0f };
        const GLfloat a1[] = { 0.0f,  0.0f,  0.0f,  1.0f };
        glLightfv(GL_LIGHT1, GL_DIFFUSE,  d1);
        glLightfv(GL_LIGHT1, GL_SPECULAR, s1);
        glLightfv(GL_LIGHT1, GL_AMBIENT,  a1);

        // Rim light — subtle from behind
        glEnable(GL_LIGHT2);
        const GLfloat d2[] = { 0.12f, 0.12f, 0.14f, 1.0f };
        const GLfloat s2[] = { 0.0f,  0.0f,  0.0f,  1.0f };
        const GLfloat a2[] = { 0.0f,  0.0f,  0.0f,  1.0f };
        glLightfv(GL_LIGHT2, GL_DIFFUSE,  d2);
        glLightfv(GL_LIGHT2, GL_SPECULAR, s2);
        glLightfv(GL_LIGHT2, GL_AMBIENT,  a2);

        // Global ambient — F360 never goes fully black in shadow
        const GLfloat ga[] = { 0.38f, 0.38f, 0.38f, 1.0f };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ga);

        // Material specular + shininess
        const GLfloat mSpec[]  = { 0.28f, 0.28f, 0.28f, 1.0f };
        const GLfloat mShiny[] = { 38.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,  mSpec);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mShiny);

        // Mesh base colour — warm light-gray (F360 default material)
        glColor3f(0.84f, 0.82f, 0.79f);

        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glBegin(GL_TRIANGLES);
        for (int i = 0; i < tris.size(); ++i) {
            const int i0 = tris[i].v0, i1 = tris[i].v1, i2 = tris[i].v2;
            if (hasVN) {
                const QVector3D& n0 = vnorms[i0];
                glNormal3f(n0.x(), n0.y(), n0.z());
                glVertex3f(verts[i0].x(), verts[i0].y(), verts[i0].z());
                const QVector3D& n1 = vnorms[i1];
                glNormal3f(n1.x(), n1.y(), n1.z());
                glVertex3f(verts[i1].x(), verts[i1].y(), verts[i1].z());
                const QVector3D& n2 = vnorms[i2];
                glNormal3f(n2.x(), n2.y(), n2.z());
                glVertex3f(verts[i2].x(), verts[i2].y(), verts[i2].z());
            } else {
                const QVector3D& fn = m_mesh.faceNormals()[i];
                glNormal3f(fn.x(), fn.y(), fn.z());
                glVertex3f(verts[i0].x(), verts[i0].y(), verts[i0].z());
                glVertex3f(verts[i1].x(), verts[i1].y(), verts[i1].z());
                glVertex3f(verts[i2].x(), verts[i2].y(), verts[i2].z());
            }
        }
        glEnd();

        glDisable(GL_LIGHTING);
        glDisable(GL_LIGHT0);
        glDisable(GL_LIGHT1);
        glDisable(GL_LIGHT2);
        glDisable(GL_COLOR_MATERIAL);
    }

    if (wireframeOnly || overlayWire) {
        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        // Wire colour — dark in light mode, barely visible overlay in dark mode
        if (m_darkMode) glColor3f(0.30f, 0.32f, 0.38f);
        else            glColor3f(0.20f, 0.22f, 0.26f);
        glLineWidth(0.8f);
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

void GLViewport::drawMeshRaw()
{
    if (m_mesh.isEmpty()) return;
    const auto& verts = m_mesh.vertices();
    const auto& tris  = m_mesh.triangles();
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBegin(GL_TRIANGLES);
    for (const auto& tri : tris) {
        const QVector3D& a = verts[tri.v0];
        const QVector3D& b = verts[tri.v1];
        const QVector3D& c = verts[tri.v2];
        glVertex3f(a.x(), a.y(), a.z());
        glVertex3f(b.x(), b.y(), b.z());
        glVertex3f(c.x(), c.y(), c.z());
    }
    glEnd();
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

// ── ViewCube ─────────────────────────────────────────────────────────────────

QRect GLViewport::viewCubeRect() const
{
    return QRect(width() - kCubeSize - kCubeMargin, kCubeMargin, kCubeSize, kCubeSize);
}

QPoint GLViewport::projectCubePoint(const QVector3D& p) const
{
    const QRect cr = viewCubeRect();
    const float yr  = qDegreesToRadians(m_yawDeg);
    const float pr  = qDegreesToRadians(m_pitchDeg);
    const float cosY = std::cos(yr), sinY = std::sin(yr);
    const float cosP = std::cos(pr), sinP = std::sin(pr);
    // R_yaw then R_pitch
    const float x1 =  p.x()*cosY + p.z()*sinY;
    const float y1 =  p.y();
    const float z1 = -p.x()*sinY + p.z()*cosY;
    const float x2 = x1;
    const float y2 = y1*cosP - z1*sinP;
    const float half = cr.width() * 0.5f;
    return QPoint(int(cr.x() + half + (x2 / 1.8f) * half),
                  int(cr.y() + half - (y2 / 1.8f) * half));
}

static float cubeViewZ(const QVector3D& n, float cosY, float sinY, float cosP, float sinP)
{
    const float y1 =  n.y();
    const float z1 = -n.x()*sinY + n.z()*cosY;
    return y1*sinP + z1*cosP;
}

int GLViewport::nearestVisibleFace(const QPoint& pos) const
{
    if (!viewCubeRect().contains(pos)) return -1;

    const float yr  = qDegreesToRadians(m_yawDeg);
    const float pr  = qDegreesToRadians(m_pitchDeg);
    const float cosY = std::cos(yr), sinY = std::sin(yr);
    const float cosP = std::cos(pr), sinP = std::sin(pr);

    static const QVector3D kCenters[6] = {
        {0,1,0},{0,-1,0},{0,0,1},{0,0,-1},{1,0,0},{-1,0,0}
    };

    float bestDist = 1e9f;
    int   bestFace = -1;
    for (int i = 0; i < 6; ++i) {
        if (cubeViewZ(kCenters[i], cosY, sinY, cosP, sinP) < 0.05f)
            continue;
        const QPoint proj = projectCubePoint(kCenters[i] * 0.82f);
        const float dx = float(pos.x() - proj.x());
        const float dy = float(pos.y() - proj.y());
        const float d2 = dx*dx + dy*dy;
        if (d2 < bestDist) { bestDist = d2; bestFace = i; }
    }
    return bestFace;
}

bool GLViewport::handleViewCubeClick(const QPoint& pos)
{
    const int face = nearestVisibleFace(pos);
    if (face < 0) return false;

    // {yaw, pitch} snap for each face: TOP, BTM, FRONT, BACK, RIGHT, LEFT
    static constexpr float kSnap[6][2] = {
        {  0.f, -89.f},
        {  0.f,  89.f},
        {  0.f,   0.f},
        {180.f,   0.f},
        {-90.f,   0.f},
        { 90.f,   0.f},
    };
    m_yawDeg   = kSnap[face][0];
    m_pitchDeg = kSnap[face][1];
    update();
    return true;
}

void GLViewport::drawViewCubeGL()
{
    const qreal dpr = devicePixelRatio();
    const QRect cr  = viewCubeRect(); // logical pixels — QPainter uses these
    // GL viewport/scissor need physical (device) pixels
    const int glX = qRound(cr.x() * dpr);
    const int glY = qRound((height() - cr.y() - cr.height()) * dpr);
    const int glW = qRound(cr.width()  * dpr);
    const int glH = qRound(cr.height() * dpr);

    glViewport(glX, glY, glW, glH);
    glScissor (glX, glY, glW, glH);
    glEnable(GL_SCISSOR_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1.8, 1.8, -1.8, 1.8, -10.0, 10.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glRotatef(m_pitchDeg, 1.0f, 0.0f, 0.0f);
    glRotatef(m_yawDeg,   0.0f, 1.0f, 0.0f);

    struct FaceDef { float v[4][3]; float nx, ny, nz; };
    static constexpr float s = 0.82f;
    static constexpr FaceDef kFaces[6] = {
        {{{-s,s,-s},{s,s,-s},{s,s,s},{-s,s,s}},       0, 1, 0},  // TOP
        {{{-s,-s,s},{s,-s,s},{s,-s,-s},{-s,-s,-s}},    0,-1, 0},  // BTM
        {{{-s,-s,s},{s,-s,s},{s,s,s},{-s,s,s}},        0, 0, 1},  // FRONT
        {{{s,-s,-s},{-s,-s,-s},{-s,s,-s},{s,s,-s}},    0, 0,-1},  // BACK
        {{{s,-s,s},{s,-s,-s},{s,s,-s},{s,s,s}},        1, 0, 0},  // RIGHT
        {{{-s,-s,-s},{-s,-s,s},{-s,s,s},{-s,s,-s}},   -1, 0, 0},  // LEFT
    };

    const float yr  = qDegreesToRadians(m_yawDeg);
    const float pr  = qDegreesToRadians(m_pitchDeg);
    const float cosY = std::cos(yr), sinY = std::sin(yr);
    const float cosP = std::cos(pr), sinP = std::sin(pr);

    for (int i = 0; i < 6; ++i) {
        const QVector3D n(kFaces[i].nx, kFaces[i].ny, kFaces[i].nz);
        const float nz = cubeViewZ(n, cosY, sinY, cosP, sinP);
        const float lit = m_darkMode
            ? qBound(0.04f, 0.08f + nz * 0.09f, 0.18f)
            : qBound(0.80f, 0.86f + nz * 0.06f, 0.94f);
        const float hov = m_darkMode ? 0.22f : 0.72f;

        if (i == m_hoveredFace)
            glColor4f(hov, hov, hov, 0.94f);
        else
            glColor4f(lit, lit, lit, 0.94f);

        glBegin(GL_QUADS);
        for (int v = 0; v < 4; ++v)
            glVertex3f(kFaces[i].v[v][0], kFaces[i].v[v][1], kFaces[i].v[v][2]);
        glEnd();
    }

    // Edges
    glDisable(GL_BLEND);
    { const float ec = m_darkMode ? 0.28f : 0.55f; glColor3f(ec, ec, ec); }
    glLineWidth(1.2f);
    static constexpr float e = s;
    static constexpr float kEdgePts[8][3] = {
        {-e,-e,-e},{e,-e,-e},{e,e,-e},{-e,e,-e},
        {-e,-e, e},{e,-e, e},{e,e, e},{-e,e, e},
    };
    static constexpr int kEdgeIdx[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7},
    };
    glBegin(GL_LINES);
    for (const auto& ei : kEdgeIdx) {
        glVertex3fv(kEdgePts[ei[0]]);
        glVertex3fv(kEdgePts[ei[1]]);
    }
    glEnd();

    // Axis stubs
    const float ax = 1.38f;
    glLineWidth(2.5f);
    glBegin(GL_LINES);
    glColor3f(0.75f, 0.18f, 0.18f); glVertex3f(0,0,0); glVertex3f(ax, 0,  0);
    glColor3f(0.18f, 0.70f, 0.18f); glVertex3f(0,0,0); glVertex3f(0,  ax, 0);
    glColor3f(0.18f, 0.36f, 0.80f); glVertex3f(0,0,0); glVertex3f(0,  0,  ax);
    glEnd();
    glLineWidth(1.0f);

    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_CULL_FACE);
    // Restore full physical viewport (resizeGL sets physical pixels, so must we)
    glViewport(0, 0, qRound(width() * dpr), qRound(height() * dpr));
}

void GLViewport::drawViewCubeLabels(QPainter& painter)
{
    const QRect cr  = viewCubeRect();
    const int   sz  = cr.width();
    const int   vpX = cr.x();
    const int   vpY = cr.y();

    const float yr  = qDegreesToRadians(m_yawDeg);
    const float pr  = qDegreesToRadians(m_pitchDeg);
    const float cosY = std::cos(yr), sinY = std::sin(yr);
    const float cosP = std::cos(pr), sinP = std::sin(pr);

    auto project = [&](const QVector3D& p) -> QPoint {
        const float x1 =  p.x()*cosY + p.z()*sinY;
        const float y1 =  p.y();
        const float z1 = -p.x()*sinY + p.z()*cosY;
        const float x2 = x1;
        const float y2 = y1*cosP - z1*sinP;
        const float half = sz * 0.5f;
        return QPoint(int(vpX + half + (x2 / 1.8f) * half),
                      int(vpY + half - (y2 / 1.8f) * half));
    };

    static constexpr float s = 0.82f;
    struct LabelDef { QVector3D pos; QVector3D normal; const char* text; };
    static const LabelDef kLabels[6] = {
        {{0,s,0},   {0, 1,0}, "TOP"  },
        {{0,-s,0},  {0,-1,0}, "BTM"  },
        {{0,0,s},   {0,0, 1}, "FRONT"},
        {{0,0,-s},  {0,0,-1}, "BACK" },
        {{s,0,0},   {1,0, 0}, "RIGHT"},
        {{-s,0,0},  {-1,0,0}, "LEFT" },
    };

    QFont font = painter.font();
    font.setPixelSize(9);
    font.setBold(true);
    painter.setFont(font);

    for (const auto& lbl : kLabels) {
        const float nz = cubeViewZ(lbl.normal, cosY, sinY, cosP, sinP);
        if (nz < 0.08f) continue;
        const float alpha = qBound(0.0f, nz * 1.6f, 1.0f);
        const QPoint pt = project(lbl.pos);
        painter.setPen(QColor(38, 40, 52, int(alpha * 235)));
        painter.drawText(QRect(pt.x() - 22, pt.y() - 8, 44, 16),
                         Qt::AlignCenter, QString::fromLatin1(lbl.text));
    }

    // Axis letter labels at tips
    const float ax = 1.38f;
    struct AxisLabel { QVector3D pos; const char* lbl; QColor col; };
    const AxisLabel kAxes[3] = {
        {{ax, 0,  0}, "X", QColor(220, 55,  55)},
        {{ 0, ax, 0}, "Y", QColor( 55,200,  55)},
        {{ 0, 0,  ax},"Z", QColor( 55,100, 240)},
    };
    font.setPixelSize(12);
    painter.setFont(font);
    for (const auto& al : kAxes) {
        const QPoint pt = project(al.pos);
        painter.setPen(al.col);
        painter.drawText(QRect(pt.x() - 9, pt.y() - 9, 18, 18),
                         Qt::AlignCenter, QString::fromLatin1(al.lbl));
    }

    // Subtle border around the cube area
    painter.setPen(QPen(QColor(80, 85, 100, 70), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(cr.adjusted(-2, -2, 2, 2), 6, 6);
}
