#pragma once

#include "HistoryEntry.h"

#include <QList>
#include <QWidget>

// Fusion-360-style horizontal history timeline bar.
// Lives at the bottom of the main window.  Emits jumpRequested(idx) where
// idx is in [-1 .. entries.size()-1]; -1 means "before all operations".
class TimelineWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget* parent = nullptr);

    void setEntries(const QList<HistoryEntry>& entries, int currentIndex);

signals:
    void jumpRequested(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QRect buttonRect(int i) const;
    int   tileLeft(int i) const;
    int   tileAt(const QPoint& pos) const;
    int   buttonAt(const QPoint& pos) const;
    QRect tilesArea() const;
    void  scrollCurrentIntoView();
    void  clampScroll();

    static QColor  tileColor(HistoryEntry::Type t);
    static QString tileGlyph(HistoryEntry::Type t);

    void drawButton(QPainter& p, int i) const;

    QList<HistoryEntry> m_entries;
    int m_currentIndex = -1;
    int m_scrollOffset = 0;
    int m_hoveredTile  = -1;
    int m_hoveredBtn   = -1;

    // Layout constants
    static constexpr int kCtrlW  = 160; // left controls column width
    static constexpr int kTileW  = 54;  // width of each operation tile
    static constexpr int kGap    = 5;   // gap between tiles
    static constexpr int kHeight = 58;  // fixed widget height
};
