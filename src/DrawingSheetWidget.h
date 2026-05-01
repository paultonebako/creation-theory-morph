#pragma once
#include <QWidget>
#include <QVector3D>
#include "MeshObject.h"

class DrawingSheetWidget : public QWidget
{
    Q_OBJECT
public:
    enum class PaperSize   { A3, A4 };
    enum class Orientation { Landscape, Portrait };

    explicit DrawingSheetWidget(QWidget* parent = nullptr);

    void setMesh(const MeshObject& mesh, const QString& docName);
    void setPaper(PaperSize sz, Orientation ori);

    // Renders current sheet to a PDF file
    bool exportPDF(const QString& path) const;

protected:
    void paintEvent(QPaintEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;

private:
    struct View {
        QString  label;
        // Projection: right and up in model space
        QVector3D right;
        QVector3D up;
        QRectF    region; // 0..1 on paper
    };

    void renderSheet(QPainter& p, const QRectF& paperPx) const;
    void drawView(QPainter& p, const View& v, const QRectF& rect) const;
    void drawTitleBlock(QPainter& p, const QRectF& paperPx) const;
    QPointF project(const QVector3D& pt, const View& v,
                    const QRectF& rect, float scale, QPointF center) const;

    MeshObject m_mesh;
    QString    m_docName;
    PaperSize  m_paper  = PaperSize::A3;
    Orientation m_ori   = Orientation::Landscape;

    float  m_zoom  = 1.0f;
    QPoint m_pan;
    QPoint m_drag;
    bool   m_panning = false;
};
