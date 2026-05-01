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

    void fitView();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void applyCamera();
    void refreshGridFromMesh();
    void drawGrid(float extent, float step);
    void drawAxes(float length);
    void drawMesh(bool wireframeOnly, bool overlayWire);
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

    MeshObject m_mesh;
    RenderMode m_renderMode = RenderMode::SolidWire;

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
