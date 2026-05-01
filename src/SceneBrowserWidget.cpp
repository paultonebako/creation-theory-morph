#include "SceneBrowserWidget.h"

#include <QHeaderView>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

QIcon SceneBrowserWidget::eyeIcon(bool open, bool darkMode)
{
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QColor c;
    if (darkMode)
        c = open ? QColor(0x88, 0x88, 0x88) : QColor(0x28, 0x28, 0x28);
    else
        c = open ? QColor(0x44, 0x44, 0x44) : QColor(0xcc, 0xcc, 0xcc);
    p.setPen(QPen(c, 1.1));
    p.setBrush(Qt::NoBrush);
    QPainterPath eye;
    eye.moveTo(1, 8);
    eye.quadTo(8, 2, 15, 8);
    eye.quadTo(8, 14, 1, 8);
    p.drawPath(eye);
    if (open) {
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(8, 8), 2.5, 2.5);
    }
    return QIcon(pm);
}

QIcon SceneBrowserWidget::dotIcon(const QColor& col)
{
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(col);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(2, 2, 10, 10), 2, 2);
    return QIcon(pm);
}

void SceneBrowserWidget::applyTreeStylesheet()
{
    if (m_darkMode) {
        m_tree->setStyleSheet(QStringLiteral(
            "QTreeWidget { background:#000; color:#555; border:none; outline:none; }"
            "QTreeWidget::item { padding:3px 2px; border:none; min-height:22px; }"
            "QTreeWidget::item:hover { background:#080808; color:#999; }"
            "QTreeWidget::item:selected { background:#0d0d0d; color:#ccc; border:none; }"
            "QTreeWidget::branch { background:#000; }"
            "QTreeWidget::branch:selected { background:#0d0d0d; }"));
    } else {
        m_tree->setStyleSheet(QStringLiteral(
            "QTreeWidget { background:#fff; color:#888; border:none; outline:none; }"
            "QTreeWidget::item { padding:3px 2px; border:none; min-height:22px; }"
            "QTreeWidget::item:hover { background:#f4f4f4; color:#333; }"
            "QTreeWidget::item:selected { background:#ebebeb; color:#111; border:none; }"
            "QTreeWidget::branch { background:#fff; }"
            "QTreeWidget::branch:selected { background:#ebebeb; }"));
    }
}

SceneBrowserWidget::SceneBrowserWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderHidden(true);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_tree->header()->resizeSection(1, 22);
    m_tree->setIndentation(12);
    m_tree->setIconSize(QSize(14, 14));
    m_tree->setFocusPolicy(Qt::NoFocus);
    m_tree->setRootIsDecorated(true);
    applyTreeStylesheet();

    connect(m_tree, &QTreeWidget::itemClicked,
            this,   &SceneBrowserWidget::onItemClicked);

    lay->addWidget(m_tree);
    refresh(QStringLiteral("Untitled"), false, true);
}

void SceneBrowserWidget::setDarkMode(bool dark)
{
    m_darkMode = dark;
    applyTreeStylesheet();
    refresh(m_docName, m_hasMesh, m_meshVis);
}

void SceneBrowserWidget::refresh(const QString& docName, bool hasMesh, bool meshVisible)
{
    m_docName  = docName;
    m_hasMesh  = hasMesh;
    m_meshVis  = meshVisible;
    m_bodiesItem = nullptr;
    m_meshItem   = nullptr;
    m_tree->clear();

    const QColor dimFg   = m_darkMode ? QColor(0x38, 0x38, 0x38) : QColor(0xaa, 0xaa, 0xaa);
    const QColor activeFg = m_darkMode ? QColor(0x66, 0x66, 0x66) : QColor(0x66, 0x66, 0x66);
    const QColor bodyFg  = m_darkMode ? QColor(0x88, 0x88, 0x88) : QColor(0x33, 0x33, 0x33);
    const QColor dimDot  = m_darkMode ? QColor(0x22, 0x22, 0x22) : QColor(0xcc, 0xcc, 0xcc);
    const QColor catDot  = m_darkMode ? QColor(0x38, 0x38, 0x38) : QColor(0xaa, 0xaa, 0xaa);
    const QColor meshDot = m_darkMode ? QColor(0x2a, 0x44, 0x55) : QColor(0x44, 0x77, 0x99);

    // Document Settings
    {
        auto* it = new QTreeWidgetItem(m_tree);
        it->setText(0, QStringLiteral("Document Settings"));
        it->setForeground(0, dimFg);
        it->setIcon(0, dotIcon(dimDot));
    }

    // Named Views
    {
        auto* it = new QTreeWidgetItem(m_tree);
        it->setText(0, QStringLiteral("Named Views"));
        it->setForeground(0, dimFg);
        it->setIcon(0, dotIcon(dimDot));
    }

    // Origin (hidden by default)
    {
        auto* origin = new QTreeWidgetItem(m_tree);
        origin->setText(0, QStringLiteral("Origin"));
        origin->setForeground(0, dimFg);
        origin->setIcon(0, dotIcon(dimDot));
        origin->setIcon(1, eyeIcon(false, m_darkMode));
        for (const char* name : {"XY Plane", "XZ Plane", "YZ Plane",
                                  "X Axis",  "Y Axis",   "Z Axis", "Center Point"}) {
            auto* ch = new QTreeWidgetItem(origin);
            ch->setText(0, QString::fromLatin1(name));
            ch->setForeground(0, dimFg);
            ch->setIcon(1, eyeIcon(false, m_darkMode));
        }
    }

    // Bodies
    m_bodiesItem = new QTreeWidgetItem(m_tree);
    m_bodiesItem->setText(0, QStringLiteral("Bodies"));
    m_bodiesItem->setForeground(0, activeFg);
    m_bodiesItem->setIcon(0, dotIcon(catDot));
    m_bodiesItem->setIcon(1, eyeIcon(hasMesh && meshVisible, m_darkMode));
    m_bodiesItem->setExpanded(true);

    if (hasMesh) {
        m_meshItem = new QTreeWidgetItem(m_bodiesItem);
        m_meshItem->setText(0, docName);
        m_meshItem->setForeground(0, bodyFg);
        m_meshItem->setIcon(0, dotIcon(meshDot));
        m_meshItem->setIcon(1, eyeIcon(meshVisible, m_darkMode));
        m_meshItem->setData(1, Qt::UserRole, meshVisible);
    }

    // Sketches
    {
        auto* it = new QTreeWidgetItem(m_tree);
        it->setText(0, QStringLiteral("Sketches"));
        it->setForeground(0, dimFg);
        it->setIcon(0, dotIcon(dimDot));
    }
}

void SceneBrowserWidget::onItemClicked(QTreeWidgetItem* item, int col)
{
    if (col != 1) return;
    if (item != m_meshItem && item != m_bodiesItem) return;

    m_meshVis = !m_meshVis;

    if (m_meshItem) {
        m_meshItem->setIcon(1, eyeIcon(m_meshVis, m_darkMode));
        m_meshItem->setData(1, Qt::UserRole, m_meshVis);
    }
    if (m_bodiesItem)
        m_bodiesItem->setIcon(1, eyeIcon(m_meshVis, m_darkMode));

    emit meshVisibilityChanged(m_meshVis);
}
