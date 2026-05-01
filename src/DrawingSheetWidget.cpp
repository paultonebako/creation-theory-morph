#include "DrawingSheetWidget.h"
#include "Version.h"

#include <QDate>
#include <QMouseEvent>
#include <QPainter>
#include <QPdfWriter>
#include <QWheelEvent>
#include <QtMath>
#include <cmath>

// ── Helpers ──────────────────────────────────────────────────────────────────

static QVector<QPair<int,int>> buildEdgeList(const MeshObject& mesh)
{
    // Deduplicated edge list (pairs of vertex indices, v0 < v1)
    QVector<QPair<int,int>> edges;
    const auto& tris = mesh.triangles();
    edges.reserve(tris.size() * 3);

    QSet<quint64> seen;
    seen.reserve(tris.size() * 3);
    auto addEdge = [&](int a, int b) {
        if (a > b) qSwap(a, b);
        quint64 key = (quint64(a) << 32) | quint32(b);
        if (!seen.contains(key)) {
            seen.insert(key);
            edges.append({a, b});
        }
    };
    for (const auto& t : tris) {
        addEdge(t.v0, t.v1);
        addEdge(t.v1, t.v2);
        addEdge(t.v2, t.v0);
    }
    return edges;
}

// ── DrawingSheetWidget ────────────────────────────────────────────────────────

DrawingSheetWidget::DrawingSheetWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(400, 300);
    setMouseTracking(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(60, 63, 65));
    setPalette(pal);
    setAutoFillBackground(true);

    // Four standard orthographic views
}

void DrawingSheetWidget::setMesh(const MeshObject& mesh, const QString& docName)
{
    m_mesh    = mesh;
    m_docName = docName;
    update();
}

void DrawingSheetWidget::setPaper(PaperSize sz, Orientation ori)
{
    m_paper = sz;
    m_ori   = ori;
    update();
}

// ── Projection ───────────────────────────────────────────────────────────────

QPointF DrawingSheetWidget::project(const QVector3D& pt, const View& v,
                                     const QRectF& rect, float scale,
                                     QPointF center) const
{
    const float px = QVector3D::dotProduct(pt, v.right);
    const float py = QVector3D::dotProduct(pt, v.up);
    return center + QPointF(double(px * scale), double(-py * scale));
}

// ── Per-view draw ─────────────────────────────────────────────────────────────

void DrawingSheetWidget::drawView(QPainter& p, const View& v,
                                   const QRectF& rect) const
{
    // Label
    p.setPen(QColor(80, 80, 80));
    QFont lf = p.font();
    lf.setPointSizeF(6.5);
    p.setFont(lf);
    p.drawText(QRectF(rect.x(), rect.y(), rect.width(), 14),
               Qt::AlignHCenter | Qt::AlignTop, v.label);

    if (m_mesh.isEmpty()) return;

    const QRectF drawArea = rect.adjusted(0, 16, 0, -4);

    // Compute mesh bounds in this projection
    const auto& verts = m_mesh.vertices();
    float minPx =  1e30f, maxPx = -1e30f;
    float minPy =  1e30f, maxPy = -1e30f;
    for (const QVector3D& vert : verts) {
        const float px = QVector3D::dotProduct(vert, v.right);
        const float py = QVector3D::dotProduct(vert, v.up);
        minPx = qMin(minPx, px); maxPx = qMax(maxPx, px);
        minPy = qMin(minPy, py); maxPy = qMax(maxPy, py);
    }
    const float spanX = maxPx - minPx;
    const float spanY = maxPy - minPy;
    const float span  = qMax(spanX, spanY);
    if (span < 1e-6f) return;

    const float margin = 0.08f;
    const float scale  = float(qMin(drawArea.width(), drawArea.height()))
                         * (1.0f - 2.0f * margin) / span;
    const QPointF center = drawArea.center()
        + QPointF(double(-(minPx + maxPx) * 0.5f * scale),
                  double( (minPy + maxPy) * 0.5f * scale));

    // Draw edges
    QPen edgePen(QColor(30, 30, 30), 0.6);
    edgePen.setCapStyle(Qt::RoundCap);
    p.setPen(edgePen);
    p.setClipRect(drawArea);

    const QVector<QPair<int,int>> edges = buildEdgeList(m_mesh);
    for (const auto& e : edges) {
        const QPointF a = project(verts[e.first],  v, drawArea, scale, center);
        const QPointF b = project(verts[e.second], v, drawArea, scale, center);
        p.drawLine(a, b);
    }
    p.setClipping(false);

    // View border
    QPen framePen(QColor(160, 160, 160), 0.5);
    p.setPen(framePen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(drawArea);
}

// ── Title block ───────────────────────────────────────────────────────────────

void DrawingSheetWidget::drawTitleBlock(QPainter& p, const QRectF& paper) const
{
    const qreal bw = paper.width()  * 0.32;
    const qreal bh = paper.height() * 0.18;
    const QRectF tb(paper.right() - bw, paper.bottom() - bh, bw, bh);

    p.setBrush(QColor(248, 248, 248));
    p.setPen(QPen(QColor(80, 80, 80), 0.7));
    p.drawRect(tb);

    // Horizontal dividers
    const qreal rowH = bh / 6.0;
    for (int i = 1; i < 6; ++i)
        p.drawLine(QPointF(tb.left(), tb.top() + rowH * i),
                   QPointF(tb.right(), tb.top() + rowH * i));

    // Vertical divider in data rows
    const qreal labelW = tb.width() * 0.36;
    p.drawLine(QPointF(tb.left() + labelW, tb.top() + rowH),
               QPointF(tb.left() + labelW, tb.bottom()));

    auto cell = [&](int row, bool isLabel, const QString& text, Qt::Alignment al) {
        QRectF r;
        if (row == 0) {
            r = QRectF(tb.left(), tb.top(), tb.width(), rowH);
        } else if (isLabel) {
            r = QRectF(tb.left(), tb.top() + rowH * row, labelW, rowH);
        } else {
            r = QRectF(tb.left() + labelW, tb.top() + rowH * row,
                       tb.width() - labelW, rowH);
        }
        p.drawText(r.adjusted(3, 0, -3, 0), al | Qt::AlignVCenter, text);
    };

    QFont hf = p.font();
    hf.setPointSizeF(7.5);
    hf.setBold(true);
    p.setFont(hf);
    p.setPen(QColor(20, 20, 20));
    cell(0, true,
         m_docName.isEmpty() ? QStringLiteral("Untitled") : m_docName,
         Qt::AlignHCenter);

    QFont df = p.font();
    df.setPointSizeF(6);
    df.setBold(false);
    p.setFont(df);

    const QStringList labels = { QStringLiteral("PART NAME"),
                                  QStringLiteral("COMPANY"),
                                  QStringLiteral("DATE"),
                                  QStringLiteral("SCALE"),
                                  QStringLiteral("UNITS") };
    const QStringList vals   = { m_docName.isEmpty() ? QStringLiteral("—") : m_docName,
                                  QStringLiteral(CTM_COMPANY_NAME),
                                  QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")),
                                  QStringLiteral("1:1"),
                                  QStringLiteral("mm") };
    for (int i = 0; i < 5; ++i) {
        p.setPen(QColor(80, 80, 80));
        cell(i + 1, true,  labels[i], Qt::AlignLeft);
        p.setPen(QColor(20, 20, 20));
        cell(i + 1, false, vals[i],   Qt::AlignLeft);
    }
}

// ── Full sheet render ─────────────────────────────────────────────────────────

void DrawingSheetWidget::renderSheet(QPainter& p, const QRectF& paperPx) const
{
    // Paper background
    p.fillRect(paperPx, Qt::white);
    p.setPen(QPen(QColor(160, 160, 160), 1));
    p.drawRect(paperPx);

    // Inner border
    const QRectF border = paperPx.adjusted(18, 18, -18, -18);
    p.setPen(QPen(QColor(60, 60, 60), 0.8));
    p.drawRect(border);

    // Title block
    drawTitleBlock(p, border);

    // Views — third-angle layout
    // Front: top-left  |  Isometric: top-right
    // Top:   bottom-left | Right: bottom-right
    const QRectF tbArea(border.right() - border.width() * 0.32,
                        border.bottom() - border.height() * 0.18,
                        border.width()  * 0.32,
                        border.height() * 0.18);

    const qreal hw = (border.width() - tbArea.width()) * 0.5;
    const qreal hh = border.height() * 0.5;

    const View views[] = {
        // Front: look from +Z, right=X, up=Y
        { QStringLiteral("FRONT"),
          { 1, 0, 0 }, { 0, 1, 0 },
          {} },
        // Top: look from +Y, right=X, up=-Z
        { QStringLiteral("TOP"),
          { 1, 0, 0 }, { 0, 0, -1 },
          {} },
        // Right: look from +X, right=-Z, up=Y
        { QStringLiteral("RIGHT"),
          { 0, 0, -1 }, { 0, 1, 0 },
          {} },
        // Isometric: standard cabinet-like 30° projection
        // right = normalize(1,0,-1), up = normalize(0.5,1,0.5)
        { QStringLiteral("ISOMETRIC"),
          QVector3D(1, 0, -1).normalized(),
          QVector3D(0.5f, 1, 0.5f).normalized(),
          {} }
    };

    const QRectF rects[] = {
        // Front — top left
        { border.left(),        border.top(),  hw, hh },
        // Top   — bottom left
        { border.left(),        border.top() + hh, hw, hh },
        // Right — bottom right (above title block)
        { border.left() + hw,   border.top() + hh, hw, hh },
        // Iso   — top right
        { border.left() + hw,   border.top(),  hw, hh },
    };

    QFont vf = p.font();
    vf.setPointSizeF(6.5);
    p.setFont(vf);
    for (int i = 0; i < 4; ++i) {
        View vw = views[i];
        drawView(p, vw, rects[i]);
    }

    // Projection symbol (third angle)
    const QRectF symRect(border.right() - tbArea.width() + 4,
                         border.bottom() - tbArea.height() * 0.5 - 12,
                         24, 14);
    p.setPen(QPen(QColor(80, 80, 80), 0.5));
    QFont sf = p.font(); sf.setPointSizeF(5); p.setFont(sf);
    p.drawText(symRect, Qt::AlignCenter, QStringLiteral("3rd Angle"));
}

// ── paintEvent ───────────────────────────────────────────────────────────────

void DrawingSheetWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Paper aspect ratio
    const bool landscape = (m_ori == Orientation::Landscape);
    const double aspect  = (m_paper == PaperSize::A3)
        ? (landscape ? 420.0 / 297.0 : 297.0 / 420.0)
        : (landscape ? 297.0 / 210.0 : 210.0 / 297.0);

    const int margin = 24;
    const int avW = width()  - margin * 2;
    const int avH = height() - margin * 2;

    double pw, ph;
    if (double(avW) / double(avH) > aspect) {
        ph = avH * m_zoom;
        pw = ph * aspect;
    } else {
        pw = avW * m_zoom;
        ph = pw / aspect;
    }

    const QRectF paper(margin + m_pan.x() + (avW - pw) * 0.5,
                       margin + m_pan.y() + (avH - ph) * 0.5,
                       pw, ph);

    // Drop shadow
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 60));
    p.drawRect(paper.adjusted(4, 4, 4, 4));

    renderSheet(p, paper);
}

// ── PDF export ────────────────────────────────────────────────────────────────

bool DrawingSheetWidget::exportPDF(const QString& path) const
{
    QPdfWriter writer(path);
    writer.setTitle(m_docName);
    writer.setCreator(QStringLiteral(CTM_PRODUCT_NAME " " CTM_VERSION_STRING));
    const bool landscape = (m_ori == Orientation::Landscape);
    writer.setPageOrientation(landscape ? QPageLayout::Landscape
                                        : QPageLayout::Portrait);
    writer.setPageSize((m_paper == PaperSize::A3)
                       ? QPageSize::A3 : QPageSize::A4);
    writer.setResolution(150);

    const QRectF paper(0, 0, writer.width(), writer.height());
    QPainter p(&writer);
    if (!p.isActive()) return false;
    renderSheet(p, paper);
    return true;
}

// ── Navigation ───────────────────────────────────────────────────────────────

void DrawingSheetWidget::wheelEvent(QWheelEvent* e)
{
    const float factor = (e->angleDelta().y() > 0) ? 1.12f : 1.0f / 1.12f;
    m_zoom = qBound(0.3f, m_zoom * factor, 5.0f);
    update();
}

void DrawingSheetWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::MiddleButton) {
        m_panning = true;
        m_drag    = e->pos();
    }
}

void DrawingSheetWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (m_panning && (e->buttons() & Qt::MiddleButton)) {
        m_pan += e->pos() - m_drag;
        m_drag = e->pos();
        update();
    }
}
