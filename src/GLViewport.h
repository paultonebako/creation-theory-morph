#pragma once

#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPoint>

#include "MeshObject.h"

class QPainter;

class GLViewport : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    enum class Axis
    {
        X,
        Y,
        Z
    };

    enum class CutMethod
    {
        Planar,
        Conic,
        Flexi,
        Grid,
        Radial,
        Modular
    };

    struct MeshTransform {
        float tx = 0.f, ty = 0.f, tz = 0.f;
        float rx = 0.f, ry = 0.f, rz = 0.f;
        float scale = 1.0f;
        bool isIdentity() const;
    };

    enum class RenderMode
    {
        Wireframe,
        Solid,
        SolidWire
    };

    explicit GLViewport(QWidget* parent = nullptr);

    void setMesh(const MeshObject& mesh);
    void clearMesh();

    void setRenderMode(RenderMode mode);
    RenderMode renderMode() const { return m_renderMode; }

    void setCuttingPlaneConfig(int countX, int countY, int countZ, Axis adjustedAxis, int adjustedIndex);
    void setCutMethod(CutMethod method);
    CutMethod cutMethod() const { return m_cutMethod; }

    void setMeshTransform(const MeshTransform& xf);
    MeshTransform meshTransform() const { return m_xform; }
    void resetMeshTransform();

    void setShowCuttingPlanes(bool show);
    void setMeshVisible(bool visible);
    void setDarkMode(bool dark);

    void fitView();

signals:
    // Emitted whenever the live transform changes (keyboard interactive mode or reset)
    void meshTransformChanged(GLViewport::MeshTransform xf);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    // ── Blender-style modal transform ─────────────────────────────────────────
    enum class TxMode { None, Grab, Rotate, Scale };
    enum class TxAxis { Free, X, Y, Z };

    void startTx(TxMode mode);
    void updateTx(const QPoint& pos);
    void confirmTx();
    void cancelTx();
    void drawTransformOverlay(QPainter& painter) const;

    TxMode        m_txMode  = TxMode::None;
    TxAxis        m_txAxis  = TxAxis::Free;
    QPoint        m_txStart;
    MeshTransform m_txSaved;
    // ── ───────────────────────────────────────────────────────────────────────

    void applyCamera();
    void applyMeshTransform();
    void refreshGridFromMesh();
    void drawGrid(float extent, float step);
    void drawAxes(float length);
    void drawMesh(bool wireframeOnly, bool overlayWire);
    void drawMeshRaw();
    void drawCuttingPlanes();
    void drawGridCutViz();
    void drawPlanarCutViz();
    void drawRadialCutViz();
    void drawConicCutViz();
    void drawFlexiCutViz();
    void drawModularCutViz();
    void drawViewCubeGL();
    void drawViewCubeLabels(QPainter& painter);
    int  nearestVisibleFace(const QPoint& pos) const;
    bool handleViewCubeClick(const QPoint& pos);
    QRect   viewCubeRect() const;
    QPoint  projectCubePoint(const QVector3D& p) const;

    MeshObject    m_mesh;
    MeshTransform m_xform;
    RenderMode m_renderMode = RenderMode::SolidWire;
    bool m_showCuttingPlanes = true;
    bool m_meshVisible       = true;
    bool m_darkMode          = true;
    bool m_meshSelected      = false;
    bool m_dragging          = false;
    QPoint m_pressPos;

    float m_gridExtent = 2.0f;
    float m_gridStep = 0.4f;
    float m_axesLength = 1.0f;

    float m_yawDeg = 35.0f;
    float m_pitchDeg = -25.0f;
    float m_distance = 5.0f;
    QVector3D m_pan = QVector3D(0.0f, 0.0f, 0.0f);

    int m_cutX = 4;
    int m_cutY = 4;
    int m_cutZ = 1;
    Axis m_adjustedAxis = Axis::X;
    int m_adjustedIndex = 0;
    CutMethod m_cutMethod = CutMethod::Grid;

    QPoint m_lastMousePos;
    int m_hoveredFace = -1;

    static constexpr int kCubeSize   = 110;
    static constexpr int kCubeMargin = 14;
};
