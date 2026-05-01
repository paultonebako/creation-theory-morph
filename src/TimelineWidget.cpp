#include "TimelineWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

// ── colours ─────────────────────────────────────────────────────────────────

static constexpr QColor kBg         { 0x00, 0x00, 0x00 };
static constexpr QColor kCtrlBg     { 0x03, 0x03, 0x03 };
static constexpr QColor kSep        { 0x16, 0x16, 0x16 };
static constexpr QColor kBtnHover   { 0x0c, 0x0c, 0x0c };
static constexpr QColor kBtnActive  { 0x10, 0x10, 0x10 };
static constexpr QColor kTileHover  { 0x09, 0x09, 0x09 };
static constexpr QColor kTileDimBg  { 0x06, 0x06, 0x06 };
static constexpr QColor kIconDim    { 0x1c, 0x1c, 0x1c };
static constexpr QColor kTextActive { 0x77, 0x77, 0x77 };
static constexpr QColor kTextDim    { 0x2a, 0x2a, 0x2a };
static constexpr QColor kPlayhead   { 0xff, 0xff, 0xff };
static constexpr QColor kTopLine    { 0x0d, 0x0d, 0x0d };

// ── ctor ─────────────────────────────────────────────────────────────────────

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(kHeight);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

// ── public ───────────────────────────────────────────────────────────────────

void TimelineWidget::setEntries(const QList<HistoryEntry>& entries, int currentIndex)
{
    m_entries      = entries;
    m_currentIndex = currentIndex;
    clampScroll();
    scrollCurrentIntoView();
    update();
}

// ── layout helpers ───────────────────────────────────────────────────────────

QRect TimelineWidget::tilesArea() const
{
    return QRect(kCtrlW + 1, 0, width() - kCtrlW - 1, kHeight);
}

// x coord of the left edge of tile i, in widget space
int TimelineWidget::tileLeft(int i) const
{
    return tilesArea().x() + kGap + i * (kTileW + kGap) - m_scrollOffset;
}

QRect TimelineWidget::buttonRect(int i) const
{
    const int margin   = 6;
    const int spacing  = 4;
    const int nBtns    = 4;
    const int btnW     = (kCtrlW - 2 * margin - (nBtns - 1) * spacing) / nBtns;
    const int btnH     = 30;
    const int btnY     = (kHeight - btnH) / 2;
    return QRect(margin + i * (btnW + spacing), btnY, btnW, btnH);
}

int TimelineWidget::buttonAt(const QPoint& pos) const
{
    if (pos.x() >= kCtrlW) return -1;
    for (int i = 0; i < 4; ++i)
        if (buttonRect(i).contains(pos)) return i;
    return -1;
}

int TimelineWidget::tileAt(const QPoint& pos) const
{
    const QRect area = tilesArea();
    if (!area.contains(pos)) return -1;
    for (int i = 0; i < m_entries.size(); ++i) {
        const int x = tileLeft(i);
        if (pos.x() >= x && pos.x() < x + kTileW) return i;
    }
    return -1;
}

void TimelineWidget::clampScroll()
{
    const int totalW = m_entries.size() * (kTileW + kGap) + kGap;
    const int areaW  = width() - kCtrlW - 1;
    m_scrollOffset   = qBound(0, m_scrollOffset, qMax(0, totalW - areaW));
}

void TimelineWidget::scrollCurrentIntoView()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_entries.size()) return;
    const QRect area = tilesArea();
    const int tx     = tileLeft(m_currentIndex);
    if (tx < area.x())
        m_scrollOffset += tx - area.x() - kGap;
    else if (tx + kTileW > area.right())
        m_scrollOffset += tx + kTileW - area.right() + kGap;
    clampScroll();
}

// ── tile colours / glyphs ────────────────────────────────────────────────────

QColor TimelineWidget::tileColor(HistoryEntry::Type t)
{
    // Very dark, barely-tinted backgrounds — editorial B&W feel
    switch (t) {
    case HistoryEntry::Type::LoadMesh:    return QColor(0x0e, 0x18, 0x28); // night blue
    case HistoryEntry::Type::ImportMesh:  return QColor(0x0a, 0x1e, 0x14); // deep green
    case HistoryEntry::Type::Cut:         return QColor(0x28, 0x18, 0x04); // dark amber
    case HistoryEntry::Type::FlipNormals: return QColor(0x18, 0x0a, 0x28); // deep purple
    case HistoryEntry::Type::UnitChange:  return QColor(0x04, 0x18, 0x18); // dark teal
    case HistoryEntry::Type::Transform:   return QColor(0x28, 0x06, 0x16); // dark crimson
    }
    return QColor(0x14, 0x14, 0x14);
}

QString TimelineWidget::tileGlyph(HistoryEntry::Type t)
{
    switch (t) {
    case HistoryEntry::Type::LoadMesh:    return QStringLiteral("O");
    case HistoryEntry::Type::ImportMesh:  return QStringLiteral("+");
    case HistoryEntry::Type::Cut:         return QStringLiteral("C");
    case HistoryEntry::Type::FlipNormals: return QStringLiteral("F");
    case HistoryEntry::Type::UnitChange:  return QStringLiteral("U");
    case HistoryEntry::Type::Transform:   return QStringLiteral("T");
    }
    return QStringLiteral("?");
}

// ── button icon drawing ───────────────────────────────────────────────────────

void TimelineWidget::drawButton(QPainter& p, int i) const
{
    const QRect r    = buttonRect(i);
    const bool  hov  = (m_hoveredBtn == i);
    const bool  enab = (i <= 1) ? (m_currentIndex >= 0)
                                 : (m_currentIndex < m_entries.size() - 1);

    if (hov && enab)
        p.fillRect(r, kBtnHover);

    const QColor col = enab ? QColor(0xcc, 0xcc, 0xd0) : QColor(0x50, 0x50, 0x54);
    p.setBrush(col);
    p.setPen(Qt::NoPen);

    const float cx = r.center().x();
    const float cy = r.center().y();
    const float th = 6.0f;  // triangle half-height
    const float tw = 5.0f;  // triangle half-width
    const float bw = 2.0f;  // bar width
    const float bg = 4.0f;  // gap between bar and triangle

    auto leftTri = [&](float ox) {
        QPolygonF tri;
        tri << QPointF(cx + ox - tw, cy)
            << QPointF(cx + ox + tw, cy - th)
            << QPointF(cx + ox + tw, cy + th);
        p.drawPolygon(tri);
    };
    auto rightTri = [&](float ox) {
        QPolygonF tri;
        tri << QPointF(cx + ox + tw, cy)
            << QPointF(cx + ox - tw, cy - th)
            << QPointF(cx + ox - tw, cy + th);
        p.drawPolygon(tri);
    };
    auto bar = [&](float bx) {
        p.fillRect(QRectF(bx - bw / 2, cy - th, bw, th * 2.0f), col);
    };

    switch (i) {
    case 0: // |◀  skip to start
        bar(cx - tw - bg / 2);
        leftTri(bg / 2);
        break;
    case 1: // ◀   step back
        leftTri(0);
        break;
    case 2: // ▶   step forward
        rightTri(0);
        break;
    case 3: // ▶|  skip to end
        rightTri(-bg / 2);
        bar(cx + tw + bg / 2);
        break;
    default:
        break;
    }
}

// ── paint ─────────────────────────────────────────────────────────────────────

void TimelineWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // ── backgrounds ──────────────────────────────────────────────────────────
    p.fillRect(rect(), kBg);
    p.fillRect(QRect(0, 0, kCtrlW, kHeight), kCtrlBg);

    // top border line
    p.fillRect(QRect(0, 0, width(), 1), kTopLine);

    // ── separator ────────────────────────────────────────────────────────────
    p.fillRect(QRect(kCtrlW, 5, 1, kHeight - 10), kSep);

    // ── playback buttons ─────────────────────────────────────────────────────
    for (int i = 0; i < 4; ++i)
        drawButton(p, i);

    // ── tiles area ───────────────────────────────────────────────────────────
    const QRect area = tilesArea();
    p.setClipRect(area);

    if (m_entries.isEmpty()) {
        p.setFont(QFont(QStringLiteral("Segoe UI"), 8));
        p.setPen(QColor(0x50, 0x50, 0x54));
        p.drawText(area.adjusted(10, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   QStringLiteral("Timeline — load or cut a mesh to begin recording operations"));
    }

    static constexpr int kIconSize = 28;
    static constexpr int kIconTop  = 7;
    static constexpr int kLabelTop = kIconTop + kIconSize + 3;
    static constexpr int kLabelH   = 12;

    for (int i = 0; i < m_entries.size(); ++i) {
        const int tx = tileLeft(i);
        if (tx + kTileW < area.x()) continue;
        if (tx > area.right())      break;

        const bool active = (i <= m_currentIndex);
        const HistoryEntry& e = m_entries[i];

        // hover highlight
        if (m_hoveredTile == i) {
            p.fillRect(QRect(tx, 2, kTileW, kHeight - 4), kTileHover);
        }

        // icon rounded rect
        const QRect iconRect(tx + (kTileW - kIconSize) / 2, kIconTop, kIconSize, kIconSize);
        const QColor iconCol = active ? tileColor(e.type) : kIconDim;
        p.setBrush(iconCol);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(iconRect, 4, 4);

        // glyph letter
        QFont gFont(QStringLiteral("Segoe UI"), 9, QFont::Bold);
        p.setFont(gFont);
        p.setPen(active ? Qt::white : QColor(0x88, 0x88, 0x8e));
        p.drawText(iconRect, Qt::AlignCenter, tileGlyph(e.type));

        // label
        QFont lFont(QStringLiteral("Segoe UI"), 7);
        p.setFont(lFont);
        p.setPen(active ? kTextActive : kTextDim);
        const QRect labelRect(tx + 1, kLabelTop, kTileW - 2, kLabelH);
        const QFontMetrics fm(lFont);
        const QString elided = fm.elidedText(e.label, Qt::ElideRight, kTileW - 4);
        p.drawText(labelRect, Qt::AlignCenter | Qt::AlignTop, elided);
    }

    // ── playhead ─────────────────────────────────────────────────────────────
    if (m_currentIndex >= 0 && m_currentIndex < m_entries.size()) {
        const int phX = tileLeft(m_currentIndex) + kTileW + kGap / 2;
        if (phX >= area.x() && phX <= area.right()) {
            // vertical line
            p.setPen(QPen(kPlayhead, 2));
            p.drawLine(phX, 1, phX, kHeight - 1);

            // downward triangle cap at top
            QPainterPath tri;
            tri.moveTo(phX - 5, 1);
            tri.lineTo(phX + 5, 1);
            tri.lineTo(phX,     9);
            tri.closeSubpath();
            p.setPen(Qt::NoPen);
            p.setBrush(kPlayhead);
            p.drawPath(tri);
        }
    }

    p.setClipping(false);
}

// ── mouse ─────────────────────────────────────────────────────────────────────

void TimelineWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    const QPoint pos = event->pos();

    const int btn = buttonAt(pos);
    if (btn >= 0) {
        int target = m_currentIndex;
        switch (btn) {
        case 0: target = -1; break;
        case 1: target = qMax(-1, m_currentIndex - 1); break;
        case 2: target = qMin(m_entries.size() - 1, m_currentIndex + 1); break;
        case 3: target = m_entries.size() - 1; break;
        }
        if (target != m_currentIndex) {
            m_currentIndex = target;
            update();
            emit jumpRequested(target);
        }
        return;
    }

    const int tile = tileAt(pos);
    if (tile >= 0 && tile != m_currentIndex) {
        m_currentIndex = tile;
        update();
        emit jumpRequested(tile);
    }
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
    const int prevBtn  = m_hoveredBtn;
    const int prevTile = m_hoveredTile;
    m_hoveredBtn  = buttonAt(event->pos());
    m_hoveredTile = (m_hoveredBtn < 0) ? tileAt(event->pos()) : -1;
    if (m_hoveredBtn != prevBtn || m_hoveredTile != prevTile)
        update();
}

void TimelineWidget::leaveEvent(QEvent*)
{
    m_hoveredBtn  = -1;
    m_hoveredTile = -1;
    update();
}

void TimelineWidget::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().x() != 0
                          ? event->angleDelta().x()
                          : event->angleDelta().y();
    m_scrollOffset -= delta / 3;
    clampScroll();
    update();
    event->accept();
}
