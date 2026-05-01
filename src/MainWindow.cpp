#include "MainWindow.h"

#include "GLViewport.h"
#include "ObjLoader.h"
#include "PreferencesDialog.h"
#include "SceneBrowserWidget.h"
#include "StlLoader.h"
#include "TimelineWidget.h"
#include "Version.h"

#include <QApplication>
#include <QActionGroup>
#include <QComboBox>
#include <QCheckBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QList>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSaveFile>
#include <QDir>
#include <QTextStream>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QStyleFactory>
#include <QToolBar>
#include <QVBoxLayout>

#include <cmath>
#include <functional>

namespace {
QString safeStem(const QString& filePath)
{
    const QString stem = QFileInfo(filePath).completeBaseName();
    return stem.isEmpty() ? QStringLiteral("mesh") : stem;
}

bool writeAsciiStl(const QString& path, const MeshObject& mesh)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << "solid part\n";
    const auto& verts = mesh.vertices();
    const auto& tris = mesh.triangles();
    const auto& norms = mesh.faceNormals();
    for (int i = 0; i < tris.size(); ++i) {
        const auto& t = tris[i];
        const QVector3D n = (i < norms.size()) ? norms[i] : QVector3D(0, 1, 0);
        out << "  facet normal " << n.x() << " " << n.y() << " " << n.z() << "\n";
        out << "    outer loop\n";
        const QVector3D a = verts[t.v0];
        const QVector3D b = verts[t.v1];
        const QVector3D c = verts[t.v2];
        out << "      vertex " << a.x() << " " << a.y() << " " << a.z() << "\n";
        out << "      vertex " << b.x() << " " << b.y() << " " << b.z() << "\n";
        out << "      vertex " << c.x() << " " << c.y() << " " << c.z() << "\n";
        out << "    endloop\n";
        out << "  endfacet\n";
    }
    out << "endsolid part\n";
    return file.commit();
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // Central widget: viewport frame above, timeline bar below
    auto* centralContainer = new QWidget(this);
    auto* outerLayout = new QVBoxLayout(centralContainer);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_viewport = new GLViewport(centralContainer);
    m_viewportFrame = new QFrame(centralContainer);
    m_viewportFrame->setObjectName(QStringLiteral("viewportFrame"));
    m_viewportFrame->setFrameShape(QFrame::StyledPanel);
    m_viewportFrame->setFrameShadow(QFrame::Plain);
    auto* centralLay = new QVBoxLayout(m_viewportFrame);
    centralLay->setContentsMargins(2, 2, 2, 2);
    centralLay->setSpacing(0);
    centralLay->addWidget(m_viewport);
    outerLayout->addWidget(m_viewportFrame, 1);

    m_timeline = new TimelineWidget(centralContainer);
    outerLayout->addWidget(m_timeline, 0);

    setCentralWidget(centralContainer);

    createDockPanels();
    createToolBar();
    createMenus();
    createStatusWidgets();

    connect(m_timeline, &TimelineWidget::jumpRequested, this, &MainWindow::applyHistoryAt);

    // Sync spinboxes when viewport changes transform interactively (Blender-style R/S/G)
    connect(m_viewport, &GLViewport::meshTransformChanged, this, [this](GLViewport::MeshTransform xf) {
        if (!m_xfTX) return;
        QSignalBlocker b0(m_xfTX), b1(m_xfTY), b2(m_xfTZ);
        QSignalBlocker b3(m_xfRX), b4(m_xfRY), b5(m_xfRZ), b6(m_xfScale);
        m_xfTX->setValue(double(xf.tx));
        m_xfTY->setValue(double(xf.ty));
        m_xfTZ->setValue(double(xf.tz));
        m_xfRX->setValue(double(xf.rx));
        m_xfRY->setValue(double(xf.ry));
        m_xfRZ->setValue(double(xf.rz));
        m_xfScale->setValue(double(xf.scale));
    });

    // Cutting planes follow Cut Designer dock visibility
    connect(m_cutDock, &QDockWidget::visibilityChanged,
            m_viewport, &GLViewport::setShowCuttingPlanes);
    m_viewport->setShowCuttingPlanes(m_cutDock->isVisible());

    // Scene browser eye toggle drives mesh visibility in viewport
    connect(m_sceneBrowser, &SceneBrowserWidget::meshVisibilityChanged,
            m_viewport, &GLViewport::setMeshVisible);

    resize(1400, 880);
    updateMeshInfoStatus();
    updateWindowTitle();
    updateSceneBrowser();
    onCuttingPlaneChanged();
    applyTheme(ThemeMode::Dark);
}

// ── theme stylesheets ─────────────────────────────────────────────────────────

QString MainWindow::darkStyleSheet()
{
    return QStringLiteral(
        "QMainWindow { background: #000000; }"
        "QWidget { background: #000000; color: #ffffff; }"
        "QMenuBar { background: #000000; color: #aaaaaa; border-bottom: 1px solid #111111; padding: 3px 4px; }"
        "QMenuBar::item { padding: 4px 12px; background: transparent; }"
        "QMenuBar::item:selected { background: #0d0d0d; color: #ffffff; }"
        "QMenu { background: #060606; color: #cccccc; border: 1px solid #1c1c1c; padding: 4px 0; }"
        "QMenu::item { padding: 6px 24px 6px 16px; }"
        "QMenu::item:selected { background: #111111; color: #ffffff; }"
        "QMenu::separator { height: 1px; background: #111111; margin: 4px 0; }"
        "QToolBar { background: #000000; border: none; border-bottom: 1px solid #111111; spacing: 0px; padding: 3px 6px; }"
        "QToolButton { background: transparent; color: #888888; padding: 4px 12px; border: none; border-radius: 0; }"
        "QToolButton:hover { background: #0c0c0c; color: #ffffff; }"
        "QToolButton:checked { background: #0f0f0f; color: #ffffff; border-bottom: 1px solid #333333; }"
        "QToolBar::separator { background: #111111; width: 1px; margin: 3px 8px; }"
        "QDockWidget { color: #ffffff; }"
        "QDockWidget::title { background: #000000; color: #444444; padding: 8px 10px; border-bottom: 1px solid #0f0f0f; font-size: 8pt; }"
        "QDockWidget > QWidget { background: #000000; }"
        "QGroupBox { border: none; border-top: 1px solid #111111; margin-top: 18px; padding-top: 10px; color: #444444; font-size: 8pt; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 0px; padding: 0 4px; color: #444444; }"
        "QLabel { color: #888888; background: transparent; }"
        "QSpinBox, QDoubleSpinBox { background: transparent; color: #dddddd; border: none; border-bottom: 1px solid #1e1e1e; padding: 2px 4px; selection-background-color: #1a1a1a; }"
        "QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { background: #050505; border: none; width: 14px; }"
        "QComboBox { background: transparent; color: #dddddd; border: none; border-bottom: 1px solid #1e1e1e; padding: 2px 4px; min-width: 6em; }"
        "QComboBox::drop-down { border: none; background: transparent; }"
        "QComboBox QAbstractItemView { background: #060606; color: #cccccc; border: 1px solid #1c1c1c; selection-background-color: #111111; outline: none; }"
        "QRadioButton { color: #888888; spacing: 6px; background: transparent; }"
        "QRadioButton::indicator { width: 11px; height: 11px; border: 1px solid #2a2a2a; border-radius: 6px; background: transparent; }"
        "QRadioButton::indicator:checked { background: #ffffff; border-color: #ffffff; }"
        "QCheckBox { color: #888888; spacing: 6px; background: transparent; }"
        "QCheckBox::indicator { width: 11px; height: 11px; border: 1px solid #2a2a2a; border-radius: 2px; background: transparent; }"
        "QCheckBox::indicator:checked { background: #ffffff; border-color: #ffffff; }"
        "QPushButton { background: transparent; color: #aaaaaa; border: 1px solid #1e1e1e; padding: 5px 14px; font-size: 9pt; }"
        "QPushButton:hover { background: #0c0c0c; border-color: #333333; color: #ffffff; }"
        "QPushButton:pressed { background: #141414; }"
        "QSlider::groove:horizontal { background: #181818; height: 1px; border: none; margin: 5px 0; }"
        "QSlider::handle:horizontal { background: #ffffff; width: 9px; height: 9px; border-radius: 5px; margin: -4px 0; }"
        "QSlider::sub-page:horizontal { background: #444444; }"
        "QScrollBar:vertical { background: #000000; width: 4px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #1e1e1e; border-radius: 2px; min-height: 20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal { background: #000000; height: 4px; }"
        "QScrollBar::handle:horizontal { background: #1e1e1e; border-radius: 2px; min-width: 20px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
        "QStatusBar { background: #000000; color: #444444; border-top: 1px solid #0d0d0d; }"
        "QStatusBar QLabel { color: #444444; padding: 0 8px; }"
        "QFrame#viewportFrame { background: #030303; border: 1px solid #0d0d0d; }");
}

QString MainWindow::lightStyleSheet()
{
    return QStringLiteral(
        "QMainWindow { background: #ffffff; }"
        "QWidget { background: #ffffff; color: #111111; }"
        "QMenuBar { background: #fafafa; color: #555555; border-bottom: 1px solid #e8e8e8; padding: 3px 4px; }"
        "QMenuBar::item { padding: 4px 12px; background: transparent; }"
        "QMenuBar::item:selected { background: #f0f0f0; color: #000000; }"
        "QMenu { background: #ffffff; color: #333333; border: 1px solid #d8d8d8; padding: 4px 0; }"
        "QMenu::item { padding: 6px 24px 6px 16px; }"
        "QMenu::item:selected { background: #f0f0f0; color: #000000; }"
        "QMenu::separator { height: 1px; background: #e8e8e8; margin: 4px 0; }"
        "QToolBar { background: #f8f8f8; border: none; border-bottom: 1px solid #e8e8e8; spacing: 0px; padding: 3px 6px; }"
        "QToolButton { background: transparent; color: #555555; padding: 4px 12px; border: none; border-radius: 0; }"
        "QToolButton:hover { background: #eeeeee; color: #000000; }"
        "QToolButton:checked { background: #e4e4e4; color: #000000; border-bottom: 1px solid #999999; }"
        "QToolBar::separator { background: #e0e0e0; width: 1px; margin: 3px 8px; }"
        "QDockWidget { color: #111111; }"
        "QDockWidget::title { background: #f5f5f5; color: #888888; padding: 8px 10px; border-bottom: 1px solid #e8e8e8; font-size: 8pt; }"
        "QDockWidget > QWidget { background: #ffffff; }"
        "QGroupBox { border: none; border-top: 1px solid #e0e0e0; margin-top: 18px; padding-top: 10px; color: #aaaaaa; font-size: 8pt; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 0px; padding: 0 4px; color: #aaaaaa; }"
        "QLabel { color: #666666; background: transparent; }"
        "QSpinBox, QDoubleSpinBox { background: transparent; color: #111111; border: none; border-bottom: 1px solid #d0d0d0; padding: 2px 4px; selection-background-color: #e8e8e8; }"
        "QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { background: #f2f2f2; border: none; width: 14px; }"
        "QComboBox { background: transparent; color: #111111; border: none; border-bottom: 1px solid #d0d0d0; padding: 2px 4px; min-width: 6em; }"
        "QComboBox::drop-down { border: none; background: transparent; }"
        "QComboBox QAbstractItemView { background: #ffffff; color: #333333; border: 1px solid #d8d8d8; selection-background-color: #f0f0f0; outline: none; }"
        "QRadioButton { color: #666666; spacing: 6px; background: transparent; }"
        "QRadioButton::indicator { width: 11px; height: 11px; border: 1px solid #cccccc; border-radius: 6px; background: transparent; }"
        "QRadioButton::indicator:checked { background: #111111; border-color: #111111; }"
        "QCheckBox { color: #666666; spacing: 6px; background: transparent; }"
        "QCheckBox::indicator { width: 11px; height: 11px; border: 1px solid #cccccc; border-radius: 2px; background: transparent; }"
        "QCheckBox::indicator:checked { background: #111111; border-color: #111111; }"
        "QPushButton { background: transparent; color: #555555; border: 1px solid #d0d0d0; padding: 5px 14px; font-size: 9pt; }"
        "QPushButton:hover { background: #f5f5f5; border-color: #aaaaaa; color: #000000; }"
        "QPushButton:pressed { background: #e8e8e8; }"
        "QSlider::groove:horizontal { background: #e0e0e0; height: 1px; border: none; margin: 5px 0; }"
        "QSlider::handle:horizontal { background: #111111; width: 9px; height: 9px; border-radius: 5px; margin: -4px 0; }"
        "QSlider::sub-page:horizontal { background: #888888; }"
        "QScrollBar:vertical { background: #ffffff; width: 4px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #cccccc; border-radius: 2px; min-height: 20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal { background: #ffffff; height: 4px; }"
        "QScrollBar::handle:horizontal { background: #cccccc; border-radius: 2px; min-width: 20px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
        "QStatusBar { background: #fafafa; color: #888888; border-top: 1px solid #e8e8e8; }"
        "QStatusBar QLabel { color: #888888; padding: 0 8px; }"
        "QFrame#viewportFrame { background: #e8e8e8; border: 1px solid #d8d8d8; }");
}

void MainWindow::applyTheme(ThemeMode mode)
{
    m_themeMode = mode;
    const bool dark = (mode == ThemeMode::Dark);

    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    if (dark) {
        p.setColor(QPalette::Window,          QColor(  0,   0,   0));
        p.setColor(QPalette::WindowText,       QColor(255, 255, 255));
        p.setColor(QPalette::Base,             QColor(  6,   6,   6));
        p.setColor(QPalette::AlternateBase,    QColor( 10,  10,  10));
        p.setColor(QPalette::ToolTipBase,      QColor(  6,   6,   6));
        p.setColor(QPalette::ToolTipText,      QColor(255, 255, 255));
        p.setColor(QPalette::Text,             QColor(255, 255, 255));
        p.setColor(QPalette::BrightText,       QColor(255, 255, 255));
        p.setColor(QPalette::Button,           QColor( 12,  12,  12));
        p.setColor(QPalette::ButtonText,       QColor(255, 255, 255));
        p.setColor(QPalette::Highlight,        QColor( 20,  20,  20));
        p.setColor(QPalette::HighlightedText,  QColor(255, 255, 255));
        p.setColor(QPalette::Link,             QColor(180, 180, 255));
        p.setColor(QPalette::Mid,              QColor( 18,  18,  18));
        p.setColor(QPalette::Midlight,         QColor( 25,  25,  25));
        p.setColor(QPalette::Dark,             QColor(  0,   0,   0));
        p.setColor(QPalette::Shadow,           QColor(  0,   0,   0));
        p.setColor(QPalette::PlaceholderText,  QColor( 80,  80,  80));
    } else {
        p.setColor(QPalette::Window,           QColor(255, 255, 255));
        p.setColor(QPalette::WindowText,       QColor( 17,  17,  17));
        p.setColor(QPalette::Base,             QColor(255, 255, 255));
        p.setColor(QPalette::AlternateBase,    QColor(245, 245, 245));
        p.setColor(QPalette::ToolTipBase,      QColor(255, 255, 255));
        p.setColor(QPalette::ToolTipText,      QColor( 17,  17,  17));
        p.setColor(QPalette::Text,             QColor( 17,  17,  17));
        p.setColor(QPalette::BrightText,       QColor(  0,   0,   0));
        p.setColor(QPalette::Button,           QColor(245, 245, 245));
        p.setColor(QPalette::ButtonText,       QColor( 17,  17,  17));
        p.setColor(QPalette::Highlight,        QColor(230, 230, 230));
        p.setColor(QPalette::HighlightedText,  QColor(  0,   0,   0));
        p.setColor(QPalette::Link,             QColor( 80,  80, 200));
        p.setColor(QPalette::Mid,              QColor(200, 200, 200));
        p.setColor(QPalette::Midlight,         QColor(220, 220, 220));
        p.setColor(QPalette::Dark,             QColor(180, 180, 180));
        p.setColor(QPalette::Shadow,           QColor(120, 120, 120));
        p.setColor(QPalette::PlaceholderText,  QColor(150, 150, 150));
    }
    qApp->setPalette(p);
    qApp->setStyleSheet(dark ? darkStyleSheet() : lightStyleSheet());

    if (m_viewport)      m_viewport->setDarkMode(dark);
    if (m_sceneBrowser)  m_sceneBrowser->setDarkMode(dark);
}

void MainWindow::showPreferences()
{
    const auto curTheme = (m_themeMode == ThemeMode::Dark)
                          ? PreferencesDialog::Theme::Dark
                          : PreferencesDialog::Theme::Light;
    PreferencesDialog dlg(curTheme, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const ThemeMode newMode = (dlg.selectedTheme() == PreferencesDialog::Theme::Light)
                              ? ThemeMode::Light : ThemeMode::Dark;
    if (newMode != m_themeMode)
        applyTheme(newMode);
}

void MainWindow::createToolBar()
{
    auto* tb = addToolBar(QStringLiteral("Main"));
    tb->setMovable(true);
    tb->setIconSize(QSize(22, 22));

    auto addTb = [this, tb](const QString& text, auto slot, QStyle::StandardPixmap pix) {
        QAction* a = tb->addAction(style()->standardIcon(pix), text, this, slot);
        a->setToolTip(text);
        return a;
    };

    addTb(QStringLiteral("Open"), &MainWindow::openMesh, QStyle::SP_DialogOpenButton);
    addTb(QStringLiteral("Import"), &MainWindow::importMesh, QStyle::SP_FileDialogNewFolder);
    addTb(QStringLiteral("Clear"), &MainWindow::clearScene, QStyle::SP_DialogDiscardButton);
    tb->addSeparator();
    addTb(QStringLiteral("Fit"), &MainWindow::fitView, QStyle::SP_ArrowDown);
    addTb(QStringLiteral("Flip normals"), &MainWindow::flipNormals, QStyle::SP_BrowserReload);
    tb->addSeparator();

    auto* wireAct = tb->addAction(QStringLiteral("Wire"));
    wireAct->setCheckable(true);
    auto* solidAct = tb->addAction(QStringLiteral("Solid"));
    solidAct->setCheckable(true);
    auto* bothAct = tb->addAction(QStringLiteral("Solid+Wire"));
    bothAct->setCheckable(true);
    bothAct->setChecked(true);
    auto* g = new QActionGroup(this);
    g->setExclusive(true);
    g->addAction(wireAct);
    g->addAction(solidAct);
    g->addAction(bothAct);
    connect(wireAct, &QAction::triggered, this, &MainWindow::setWireframeMode);
    connect(solidAct, &QAction::triggered, this, &MainWindow::setSolidMode);
    connect(bothAct, &QAction::triggered, this, &MainWindow::setSolidWireMode);

    tb->addSeparator();
    QAction* browserAct = tb->addAction(QStringLiteral("Browser"));
    browserAct->setCheckable(true);
    browserAct->setChecked(true);
    browserAct->setToolTip(QStringLiteral("Show / hide the Scene Browser"));
    connect(browserAct, &QAction::toggled, this, [this](bool on) {
        if (m_browserDock) m_browserDock->setVisible(on);
    });
    if (m_browserDock) {
        connect(m_browserDock, &QDockWidget::visibilityChanged, browserAct, &QAction::setChecked);
    }

    QAction* cutPanelAct = tb->addAction(QStringLiteral("Cut Designer"));
    cutPanelAct->setCheckable(true);
    cutPanelAct->setChecked(true);
    cutPanelAct->setToolTip(QStringLiteral("Show / hide the Cut Designer panel"));
    connect(cutPanelAct, &QAction::toggled, this, [this](bool on) {
        if (m_cutDock) m_cutDock->setVisible(on);
    });
    if (m_cutDock) {
        connect(m_cutDock, &QDockWidget::visibilityChanged, cutPanelAct, &QAction::setChecked);
    }

    QAction* xfPanelAct = tb->addAction(QStringLiteral("Transform"));
    xfPanelAct->setCheckable(true);
    xfPanelAct->setChecked(true);
    xfPanelAct->setToolTip(QStringLiteral("Show / hide the Transform panel  (R=rotate  S=scale  G=grab)"));
    connect(xfPanelAct, &QAction::toggled, this, [this](bool on) {
        if (m_xfDock) m_xfDock->setVisible(on);
    });
    if (m_xfDock) {
        connect(m_xfDock, &QDockWidget::visibilityChanged, xfPanelAct, &QAction::setChecked);
    }
}

void MainWindow::createDockPanels()
{
    // ── Scene Browser dock ────────────────────────────────────────────────────
    m_sceneBrowser = new SceneBrowserWidget(this);
    m_browserDock  = new QDockWidget(QStringLiteral("Scene Browser"), this);
    m_browserDock->setObjectName(QStringLiteral("dockSceneBrowser"));
    m_browserDock->setWidget(m_sceneBrowser);
    m_browserDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_browserDock->setMinimumWidth(200);
    addDockWidget(Qt::LeftDockWidgetArea, m_browserDock);

    if (!m_unitCombo) {
        m_unitCombo = new QComboBox(this);
        m_unitCombo->addItem(QStringLiteral("Model units: mm"), static_cast<int>(LinearUnit::Millimeters));
        m_unitCombo->addItem(QStringLiteral("Model units: cm"), static_cast<int>(LinearUnit::Centimeters));
        m_unitCombo->addItem(QStringLiteral("Model units: m"), static_cast<int>(LinearUnit::Meters));
        m_unitCombo->addItem(QStringLiteral("Model units: in"), static_cast<int>(LinearUnit::Inches));
        m_unitCombo->setCurrentIndex(0);
        connect(m_unitCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onModelUnitChanged);
    }

    m_cutPanel = new QWidget(this);
    auto* root = new QVBoxLayout(m_cutPanel);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto addRow = [this](QVBoxLayout* layout, const QString& label, QWidget* field) {
        auto* row = new QHBoxLayout();
        auto* l = new QLabel(label, this);
        l->setMinimumWidth(120);
        row->addWidget(l);
        row->addWidget(field, 1);
        layout->addLayout(row);
    };

    auto* inputGroup = new QGroupBox(QStringLiteral("Input"), m_cutPanel);
    auto* inputLayout = new QVBoxLayout(inputGroup);
    m_methodCombo = new QComboBox(inputGroup);
    m_methodCombo->addItem(QStringLiteral("Planar cut"),  static_cast<int>(GLViewport::CutMethod::Planar));
    m_methodCombo->addItem(QStringLiteral("Conic cut"),   static_cast<int>(GLViewport::CutMethod::Conic));
    m_methodCombo->addItem(QStringLiteral("Flexi cut"),   static_cast<int>(GLViewport::CutMethod::Flexi));
    m_methodCombo->addItem(QStringLiteral("Grid cut"),    static_cast<int>(GLViewport::CutMethod::Grid));
    m_methodCombo->addItem(QStringLiteral("Radial cut"),  static_cast<int>(GLViewport::CutMethod::Radial));
    m_methodCombo->addItem(QStringLiteral("Modular cut"), static_cast<int>(GLViewport::CutMethod::Modular));
    m_methodCombo->setCurrentIndex(3); // default: Grid cut
    addRow(inputLayout, QStringLiteral("Method"), m_methodCombo);
    addRow(inputLayout, QStringLiteral("Unit"), m_unitCombo);

    auto* cutGroup = new QGroupBox(QStringLiteral("Cutting Plane"), m_cutPanel);
    auto* cutLayout = new QVBoxLayout(cutGroup);
    m_cutXSpin = new QSpinBox(cutGroup);
    m_cutYSpin = new QSpinBox(cutGroup);
    m_cutZSpin = new QSpinBox(cutGroup);
    for (QSpinBox* s : QList<QSpinBox*>{m_cutXSpin, m_cutYSpin, m_cutZSpin}) {
        s->setRange(1, 32);
    }
    m_cutXSpin->setValue(4);
    m_cutYSpin->setValue(4);
    m_cutZSpin->setValue(1);
    addRow(cutLayout, QStringLiteral("Sections X"), m_cutXSpin);
    addRow(cutLayout, QStringLiteral("Sections Y"), m_cutYSpin);
    addRow(cutLayout, QStringLiteral("Sections Z"), m_cutZSpin);

    m_staggeredCheck = new QCheckBox(QStringLiteral("Staggered cut pattern"), cutGroup);
    cutLayout->addWidget(m_staggeredCheck);

    m_adjustAxisCombo = new QComboBox(cutGroup);
    m_adjustAxisCombo->addItem(QStringLiteral("X"), static_cast<int>(GLViewport::Axis::X));
    m_adjustAxisCombo->addItem(QStringLiteral("Y"), static_cast<int>(GLViewport::Axis::Y));
    m_adjustAxisCombo->addItem(QStringLiteral("Z"), static_cast<int>(GLViewport::Axis::Z));
    m_adjustIndexSpin = new QSpinBox(cutGroup);
    m_adjustIndexSpin->setRange(0, 31);
    m_adjustIndexSpin->setValue(0);
    addRow(cutLayout, QStringLiteral("Adjust axis"), m_adjustAxisCombo);
    addRow(cutLayout, QStringLiteral("Adjust index"), m_adjustIndexSpin);

    m_adjustValueSpin = new QDoubleSpinBox(cutGroup);
    m_adjustValueSpin->setRange(-1000.0, 1000.0);
    m_adjustValueSpin->setDecimals(2);
    m_adjustValueSpin->setSingleStep(0.1);
    m_adjustValueSpin->setValue(0.0);
    addRow(cutLayout, QStringLiteral("Adjust value"), m_adjustValueSpin);

    m_adjustSlider = new QSlider(Qt::Horizontal, cutGroup);
    m_adjustSlider->setRange(-1000, 1000);
    m_adjustSlider->setValue(0);
    cutLayout->addWidget(m_adjustSlider);

    m_cutChamferSpin = new QDoubleSpinBox(cutGroup);
    m_cutChamferSpin->setRange(0.0, 1000.0);
    m_cutChamferSpin->setDecimals(2);
    m_cutChamferSpin->setSingleStep(0.1);
    m_cutChamferSpin->setValue(0.0);
    addRow(cutLayout, QStringLiteral("Cut chamfer"), m_cutChamferSpin);

    auto* closeRow = new QHBoxLayout();
    closeRow->addWidget(new QLabel(QStringLiteral("Close cut"), cutGroup));
    m_closeCutYes = new QRadioButton(QStringLiteral("Yes"), cutGroup);
    m_closeCutNo = new QRadioButton(QStringLiteral("No"), cutGroup);
    m_closeCutYes->setChecked(true);
    closeRow->addWidget(m_closeCutYes);
    closeRow->addWidget(m_closeCutNo);
    closeRow->addStretch(1);
    cutLayout->addLayout(closeRow);

    auto* partRow = new QHBoxLayout();
    partRow->addWidget(new QLabel(QStringLiteral("Part number"), cutGroup));
    m_partNumberYes = new QRadioButton(QStringLiteral("Yes"), cutGroup);
    m_partNumberNo = new QRadioButton(QStringLiteral("No"), cutGroup);
    m_partNumberYes->setChecked(true);
    partRow->addWidget(m_partNumberYes);
    partRow->addWidget(m_partNumberNo);
    partRow->addStretch(1);
    cutLayout->addLayout(partRow);

    auto* connGroup = new QGroupBox(QStringLiteral("Connector"), m_cutPanel);
    auto* connLayout = new QVBoxLayout(connGroup);
    auto* connTypeRow = new QHBoxLayout();
    m_connectorPlug = new QRadioButton(QStringLiteral("Plug"), connGroup);
    m_connectorDowel = new QRadioButton(QStringLiteral("Dowel"), connGroup);
    m_connectorNone = new QRadioButton(QStringLiteral("None"), connGroup);
    m_connectorPlug->setChecked(true);
    connTypeRow->addWidget(m_connectorPlug);
    connTypeRow->addWidget(m_connectorDowel);
    connTypeRow->addWidget(m_connectorNone);
    connLayout->addLayout(connTypeRow);

    auto* connSubtypeRow = new QHBoxLayout();
    m_connectorPrism = new QRadioButton(QStringLiteral("Prism"), connGroup);
    m_connectorFrustum = new QRadioButton(QStringLiteral("Frustum"), connGroup);
    m_connectorPrism->setChecked(true);
    connSubtypeRow->addWidget(m_connectorPrism);
    connSubtypeRow->addWidget(m_connectorFrustum);
    connSubtypeRow->addStretch(1);
    connLayout->addLayout(connSubtypeRow);

    m_connectorShapeCombo = new QComboBox(connGroup);
    m_connectorShapeCombo->addItems({QStringLiteral("Square"), QStringLiteral("Triangle"), QStringLiteral("Circle")});
    addRow(connLayout, QStringLiteral("Shape"), m_connectorShapeCombo);

    m_connectorNumberCombo = new QComboBox(connGroup);
    m_connectorNumberCombo->addItems({QStringLiteral("Any"), QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"), QStringLiteral("4")});
    addRow(connLayout, QStringLiteral("Number"), m_connectorNumberCombo);

    m_connectorDepthRatio = new QDoubleSpinBox(connGroup);
    m_connectorDepthRatio->setRange(0.1, 10.0);
    m_connectorDepthRatio->setDecimals(2);
    m_connectorDepthRatio->setValue(1.5);
    addRow(connLayout, QStringLiteral("Depth ratio"), m_connectorDepthRatio);

    m_connectorSize = new QDoubleSpinBox(connGroup);
    m_connectorSize->setRange(0.1, 1000.0);
    m_connectorSize->setDecimals(2);
    m_connectorSize->setValue(5.0);
    addRow(connLayout, QStringLiteral("Size"), m_connectorSize);

    m_connectorTolerance = new QDoubleSpinBox(connGroup);
    m_connectorTolerance->setRange(-10.0, 10.0);
    m_connectorTolerance->setDecimals(2);
    m_connectorTolerance->setValue(-0.2);
    addRow(connLayout, QStringLiteral("Tolerance"), m_connectorTolerance);

    auto* actionRow = new QHBoxLayout();
    auto* cutBtn = new QPushButton(QStringLiteral("Cut"), m_cutPanel);
    auto* closeBtn = new QPushButton(QStringLiteral("Close"), m_cutPanel);
    actionRow->addWidget(cutBtn);
    actionRow->addWidget(closeBtn);
    root->addLayout(actionRow);

    root->addWidget(inputGroup);
    root->addWidget(cutGroup);
    root->addWidget(connGroup);
    root->addStretch(1);

    m_cutDock = new QDockWidget(QStringLiteral("Cut Designer"), this);
    m_cutDock->setObjectName(QStringLiteral("dockCutDesigner"));
    m_cutDock->setWidget(m_cutPanel);
    m_cutDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_cutDock->setMinimumWidth(280);
    addDockWidget(Qt::LeftDockWidgetArea, m_cutDock);

    // ── Transform dock ────────────────────────────────────────────────────────
    auto* xfPanel  = new QWidget(this);
    auto* xfRoot   = new QVBoxLayout(xfPanel);
    xfRoot->setContentsMargins(8, 8, 8, 8);
    xfRoot->setSpacing(6);

    auto addXfRow = [this](QVBoxLayout* layout, const QString& label, QDoubleSpinBox*& spin,
                            double lo, double hi, double step, int dec, double def) {
        auto* row = new QHBoxLayout();
        auto* l = new QLabel(label, this);
        l->setMinimumWidth(80);
        spin = new QDoubleSpinBox(this);
        spin->setRange(lo, hi);
        spin->setSingleStep(step);
        spin->setDecimals(dec);
        spin->setValue(def);
        row->addWidget(l);
        row->addWidget(spin, 1);
        layout->addLayout(row);
    };

    auto* tGroup = new QGroupBox(QStringLiteral("Translate"), xfPanel);
    auto* tLayout = new QVBoxLayout(tGroup);
    addXfRow(tLayout, QStringLiteral("X"), m_xfTX, -10000, 10000, 0.5, 2, 0.0);
    addXfRow(tLayout, QStringLiteral("Y"), m_xfTY, -10000, 10000, 0.5, 2, 0.0);
    addXfRow(tLayout, QStringLiteral("Z"), m_xfTZ, -10000, 10000, 0.5, 2, 0.0);

    auto* rGroup = new QGroupBox(QStringLiteral("Rotate (degrees)"), xfPanel);
    auto* rLayout = new QVBoxLayout(rGroup);
    addXfRow(rLayout, QStringLiteral("X"), m_xfRX, -360, 360, 1.0, 1, 0.0);
    addXfRow(rLayout, QStringLiteral("Y"), m_xfRY, -360, 360, 1.0, 1, 0.0);
    addXfRow(rLayout, QStringLiteral("Z"), m_xfRZ, -360, 360, 1.0, 1, 0.0);

    auto* sGroup = new QGroupBox(QStringLiteral("Scale"), xfPanel);
    auto* sLayout = new QVBoxLayout(sGroup);
    addXfRow(sLayout, QStringLiteral("Uniform"), m_xfScale, 0.001, 1000.0, 0.01, 3, 1.0);

    auto* xfBtnRow = new QHBoxLayout();
    auto* applyXfBtn = new QPushButton(QStringLiteral("Apply Transform"), xfPanel);
    auto* resetXfBtn = new QPushButton(QStringLiteral("Reset"), xfPanel);
    xfBtnRow->addWidget(applyXfBtn);
    xfBtnRow->addWidget(resetXfBtn);

    xfRoot->addWidget(tGroup);
    xfRoot->addWidget(rGroup);
    xfRoot->addWidget(sGroup);
    xfRoot->addLayout(xfBtnRow);
    xfRoot->addStretch(1);

    m_xfDock = new QDockWidget(QStringLiteral("Transform"), this);
    m_xfDock->setObjectName(QStringLiteral("dockTransform"));
    m_xfDock->setWidget(xfPanel);
    m_xfDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_xfDock->setMinimumWidth(240);
    addDockWidget(Qt::RightDockWidgetArea, m_xfDock);

    for (QDoubleSpinBox* s : {m_xfTX, m_xfTY, m_xfTZ, m_xfRX, m_xfRY, m_xfRZ, m_xfScale}) {
        connect(s, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onTransformChanged);
    }
    connect(applyXfBtn, &QPushButton::clicked, this, &MainWindow::applyTransform);
    connect(resetXfBtn, &QPushButton::clicked, this, &MainWindow::resetTransformUI);

    connect(m_cutXSpin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onCuttingPlaneChanged);
    connect(m_cutYSpin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onCuttingPlaneChanged);
    connect(m_cutZSpin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onCuttingPlaneChanged);
    connect(m_adjustAxisCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::onCuttingPlaneChanged);
    connect(m_adjustIndexSpin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onCuttingPlaneChanged);
    connect(m_staggeredCheck, &QCheckBox::toggled, this, &MainWindow::onCuttingPlaneChanged);
    connect(m_adjustValueSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onCuttingPlaneChanged);
    connect(m_cutChamferSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onCuttingPlaneChanged);
    connect(m_closeCutYes, &QRadioButton::toggled, this, &MainWindow::onCuttingPlaneChanged);
    connect(m_partNumberYes, &QRadioButton::toggled, this, &MainWindow::onCuttingPlaneChanged);
    connect(m_adjustSlider, &QSlider::valueChanged, this, [this](int v) {
        if (!m_adjustValueSpin) {
            return;
        }
        const double newVal = double(v) / 100.0;
        if (!qFuzzyCompare(m_adjustValueSpin->value() + 1.0, newVal + 1.0)) {
            m_adjustValueSpin->setValue(newVal);
        }
    });
    connect(m_adjustValueSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (!m_adjustSlider) {
            return;
        }
        const int intVal = int(v * 100.0);
        if (m_adjustSlider->value() != intVal) {
            m_adjustSlider->setValue(intVal);
        }
    });

    connect(m_connectorPlug, &QRadioButton::toggled, this, &MainWindow::onConnectorSettingsChanged);
    connect(m_connectorDowel, &QRadioButton::toggled, this, &MainWindow::onConnectorSettingsChanged);
    connect(m_connectorNone, &QRadioButton::toggled, this, &MainWindow::onConnectorSettingsChanged);
    connect(m_connectorPrism, &QRadioButton::toggled, this, &MainWindow::onConnectorSettingsChanged);
    connect(m_connectorFrustum, &QRadioButton::toggled, this, &MainWindow::onConnectorSettingsChanged);
    connect(m_connectorShapeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::onConnectorSettingsChanged);
    connect(m_connectorNumberCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::onConnectorSettingsChanged);
    connect(m_connectorDepthRatio, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onConnectorSettingsChanged);
    connect(m_connectorSize, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onConnectorSettingsChanged);
    connect(m_connectorTolerance, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onConnectorSettingsChanged);
    connect(m_methodCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::onCutMethodChanged);
    connect(cutBtn, &QPushButton::clicked, this, &MainWindow::performCut);
    connect(closeBtn, &QPushButton::clicked, this, [this]() { if (m_cutDock) m_cutDock->hide(); });
}

void MainWindow::createStatusWidgets()
{
    m_meshInfoLabel = new QLabel(QStringLiteral("No mesh"));
    m_meshInfoLabel->setMinimumWidth(280);

    statusBar()->setStyleSheet(QStringLiteral("QStatusBar::item { border: none; }"));
    statusBar()->addPermanentWidget(m_meshInfoLabel, 1);
    statusBar()->showMessage(QStringLiteral("Ready — one file unit = one entry in the mesh file (STL/OBJ is unitless)"));
}

void MainWindow::createMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    QAction* openAction = fileMenu->addAction(QStringLiteral("&Open..."));
    QAction* importAction = fileMenu->addAction(QStringLiteral("&Import..."));
    fileMenu->addSeparator();
    QAction* clearAction = fileMenu->addAction(QStringLiteral("&Clear Scene"));
    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction(QStringLiteral("E&xit"));

    connect(openAction, &QAction::triggered, this, &MainWindow::openMesh);
    connect(importAction, &QAction::triggered, this, &MainWindow::importMesh);
    connect(clearAction, &QAction::triggered, this, &MainWindow::clearScene);
    connect(exitAction, &QAction::triggered, this, &MainWindow::exitApp);

    QMenu* editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
    QAction* undoAct = editMenu->addAction(QStringLiteral("Undo"));
    undoAct->setShortcut(QKeySequence::Undo);
    connect(undoAct, &QAction::triggered, this, [this]() {
        if (m_histIdx >= 0) applyHistoryAt(m_histIdx - 1);
    });
    QAction* redoAct = editMenu->addAction(QStringLiteral("Redo"));
    redoAct->setShortcut(QKeySequence::Redo);
    connect(redoAct, &QAction::triggered, this, [this]() {
        if (m_histIdx < m_history.size() - 1) applyHistoryAt(m_histIdx + 1);
    });
    editMenu->addSeparator();
    editMenu->addAction(QStringLiteral("Preferences"), this, &MainWindow::showPreferences);

    QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    QActionGroup* renderGroup = new QActionGroup(this);
    renderGroup->setExclusive(true);

    QAction* wireframeAction = viewMenu->addAction(QStringLiteral("Wireframe"));
    wireframeAction->setCheckable(true);
    renderGroup->addAction(wireframeAction);

    QAction* solidAction = viewMenu->addAction(QStringLiteral("Solid"));
    solidAction->setCheckable(true);
    renderGroup->addAction(solidAction);

    QAction* solidWireAction = viewMenu->addAction(QStringLiteral("Solid + Wire"));
    solidWireAction->setCheckable(true);
    solidWireAction->setChecked(true);
    renderGroup->addAction(solidWireAction);

    connect(wireframeAction, &QAction::triggered, this, &MainWindow::setWireframeMode);
    connect(solidAction, &QAction::triggered, this, &MainWindow::setSolidMode);
    connect(solidWireAction, &QAction::triggered, this, &MainWindow::setSolidWireMode);

    viewMenu->addSeparator();
    viewMenu->addAction(QStringLiteral("Fit view"), this, &MainWindow::fitView);
    viewMenu->addSeparator();
    if (m_cutDock) viewMenu->addAction(m_cutDock->toggleViewAction());

    QMenu* meshMenu = menuBar()->addMenu(QStringLiteral("&Mesh"));
    QAction* flipNormalsAction = meshMenu->addAction(QStringLiteral("Flip Normals"));
    connect(flipNormalsAction, &QAction::triggered, this, &MainWindow::flipNormals);

    QMenu* objectMenu = menuBar()->addMenu(QStringLiteral("&Object"));
    objectMenu->addAction(QStringLiteral("Center / fit camera"), this, &MainWindow::fitView);

    QMenu* drawMenu = menuBar()->addMenu(QStringLiteral("&Draw"));
    drawMenu->addAction(QStringLiteral("Refresh"), [this]() { m_viewport->update(); });

    QMenu* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    QAction* aboutAction = helpMenu->addAction(QStringLiteral("About"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::openMesh()
{
    if (!askModelUnitForImport()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Open Mesh"),
        QString(),
        QStringLiteral("Mesh Files (*.stl *.obj);;STL (*.stl);;OBJ (*.obj)"));
    if (path.isEmpty()) {
        return;
    }
    loadMeshFile(path, false);
}

void MainWindow::importMesh()
{
    if (!askModelUnitForImport()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Import Mesh"),
        QString(),
        QStringLiteral("Mesh Files (*.stl *.obj);;STL (*.stl);;OBJ (*.obj)"));
    if (path.isEmpty()) {
        return;
    }
    loadMeshFile(path, true);
}

void MainWindow::applyModelUnitToNewGeometry(MeshObject& mesh)
{
    const float s = static_cast<float>(Units::metersPerModelUnit(m_modelUnit));
    mesh.uniformScale(s);
    mesh.layOnGroundCenterXZ();
}

void MainWindow::onModelUnitChanged(int index)
{
    Q_UNUSED(index);
    const LinearUnit newUnit = static_cast<LinearUnit>(m_unitCombo->currentData().toInt());
    const LinearUnit oldUnit = m_modelUnit;
    if (!m_mesh.isEmpty() && newUnit != oldUnit) {
        const double oldPer = Units::metersPerModelUnit(oldUnit);
        const double newPer = Units::metersPerModelUnit(newUnit);
        if (oldPer > 0.0 && newPer > 0.0) {
            const float ratio = static_cast<float>(newPer / oldPer);
            m_mesh.uniformScale(ratio);
            m_mesh.layOnGroundCenterXZ();
            refreshMeshInViewport();
            statusBar()->showMessage(
                QStringLiteral("Model units updated — geometry rescaled and placed on ground grid"), 5000);
        }
    }
    m_modelUnit = newUnit;
    if (!m_mesh.isEmpty() && newUnit != oldUnit)
        pushHistory(HistoryEntry::Type::UnitChange,
                    QStringLiteral("Unit: ") + m_unitCombo->currentText().section(QLatin1Char(' '), -1));
    updateMeshInfoStatus();
}

void MainWindow::onCutMethodChanged(int /*index*/)
{
    if (!m_methodCombo || !m_viewport) return;
    const auto method = static_cast<GLViewport::CutMethod>(m_methodCombo->currentData().toInt());
    m_viewport->setCutMethod(method);
    const QString name = m_methodCombo->currentText();
    statusBar()->showMessage(QStringLiteral("Cut method: %1").arg(name), 2000);
}

void MainWindow::onCuttingPlaneChanged()
{
    if (!m_cutXSpin || !m_cutYSpin || !m_cutZSpin || !m_adjustAxisCombo || !m_adjustIndexSpin || !m_adjustValueSpin) {
        return;
    }
    const int cx = m_cutXSpin->value();
    const int cy = m_cutYSpin->value();
    const int cz = m_cutZSpin->value();
    const GLViewport::Axis axis = static_cast<GLViewport::Axis>(m_adjustAxisCombo->currentData().toInt());
    int maxForAxis = cx;
    if (axis == GLViewport::Axis::Y) {
        maxForAxis = cy;
    } else if (axis == GLViewport::Axis::Z) {
        maxForAxis = cz;
    }
    m_adjustIndexSpin->setMaximum(qMax(0, maxForAxis - 1));
    const int idx = qMin(m_adjustIndexSpin->value(), qMax(0, maxForAxis - 1));
    m_viewport->setCuttingPlaneConfig(cx, cy, cz, axis, idx);
    statusBar()->showMessage(
        QStringLiteral("Cutting planes: X=%1 Y=%2 Z=%3 | Adjust=%4:%5 (%6)")
            .arg(cx)
            .arg(cy)
            .arg(cz)
            .arg(m_adjustAxisCombo->currentText())
            .arg(idx + 1)
            .arg(QString::number(m_adjustValueSpin->value(), 'f', 2)),
        1500);
}

void MainWindow::onConnectorSettingsChanged()
{
    if (!m_connectorShapeCombo || !m_connectorNumberCombo || !m_connectorDepthRatio || !m_connectorSize || !m_connectorTolerance) {
        return;
    }
    const QString type = m_connectorNone->isChecked()   ? QStringLiteral("None")
                         : m_connectorDowel->isChecked() ? QStringLiteral("Dowel")
                                                         : QStringLiteral("Plug");
    const QString subtype = m_connectorFrustum->isChecked() ? QStringLiteral("Frustum") : QStringLiteral("Prism");
    statusBar()->showMessage(
        QStringLiteral("Connector: %1/%2, shape=%3, number=%4, depth=%5, size=%6, tol=%7")
            .arg(type)
            .arg(subtype)
            .arg(m_connectorShapeCombo->currentText())
            .arg(m_connectorNumberCombo->currentText())
            .arg(QString::number(m_connectorDepthRatio->value(), 'f', 2))
            .arg(QString::number(m_connectorSize->value(), 'f', 2))
            .arg(QString::number(m_connectorTolerance->value(), 'f', 2)),
        1800);
}

void MainWindow::performCut()
{
    if (m_mesh.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Cut"), QStringLiteral("Load a mesh first."));
        return;
    }

    const QVector3D mn   = m_mesh.minBounds();
    const QVector3D mx   = m_mesh.maxBounds();
    const QVector3D size = mx - mn;

    if (qFuzzyIsNull(size.x()) || qFuzzyIsNull(size.y()) || qFuzzyIsNull(size.z())) {
        QMessageBox::warning(this, QStringLiteral("Cut"), QStringLiteral("Mesh bounds are degenerate."));
        return;
    }

    const int sx = qMax(1, m_cutXSpin->value());
    const int sy = qMax(1, m_cutYSpin->value());
    const int sz = qMax(1, m_cutZSpin->value());
    const float cx  = (mn.x() + mx.x()) * 0.5f;
    const float czc = (mn.z() + mx.z()) * 0.5f;

    const GLViewport::CutMethod method = m_viewport->cutMethod();

    int totalParts = 1;
    std::function<int(const QVector3D&)> assignBin;

    auto gridBin = [&](float p, float minV, float span, int N) {
        return qBound(0, int(std::floor((p - minV) / span * float(N))), N - 1);
    };

    switch (method) {
    case GLViewport::CutMethod::Grid:
        totalParts = sx * sy * sz;
        assignBin  = [=](const QVector3D& c) {
            int ix = gridBin(c.x(), mn.x(), size.x(), sx);
            int iy = gridBin(c.y(), mn.y(), size.y(), sy);
            int iz = gridBin(c.z(), mn.z(), size.z(), sz);
            return (iz * sy + iy) * sx + ix;
        };
        break;

    case GLViewport::CutMethod::Planar: {
        const GLViewport::Axis axis = static_cast<GLViewport::Axis>(m_adjustAxisCombo->currentData().toInt());
        const int N = (axis == GLViewport::Axis::X) ? sx : (axis == GLViewport::Axis::Y ? sy : sz);
        totalParts  = N + 1;
        assignBin   = [=](const QVector3D& c) {
            float val  = (axis == GLViewport::Axis::X) ? c.x() : (axis == GLViewport::Axis::Y ? c.y() : c.z());
            float minV = (axis == GLViewport::Axis::X) ? mn.x() : (axis == GLViewport::Axis::Y ? mn.y() : mn.z());
            float span = (axis == GLViewport::Axis::X) ? size.x() : (axis == GLViewport::Axis::Y ? size.y() : size.z());
            for (int i = 0; i < N; ++i)
                if (val < minV + float(i + 1) / float(N + 1) * span)
                    return i;
            return N;
        };
        break;
    }

    case GLViewport::CutMethod::Radial:
        totalParts = sx;
        assignBin  = [=](const QVector3D& c) {
            float angle = std::atan2(c.z() - czc, c.x() - cx);
            if (angle < 0.0f) angle += 2.0f * float(M_PI);
            return qBound(0, int(angle / (2.0f * float(M_PI)) * float(sx)), sx - 1);
        };
        break;

    case GLViewport::CutMethod::Conic: {
        const float maxR = qMax(size.x(), size.z()) * 0.5f + 0.001f;
        totalParts       = sx;
        assignBin        = [=](const QVector3D& c) {
            float r = std::sqrt((c.x() - cx) * (c.x() - cx) + (c.z() - czc) * (c.z() - czc));
            return qBound(0, int(r / maxR * float(sx)), sx - 1);
        };
        break;
    }

    case GLViewport::CutMethod::Flexi:
        totalParts = sx * sy * sz;
        assignBin  = [=](const QVector3D& c) {
            float tx = (c.x() - mn.x()) / size.x();
            float ty = (c.y() - mn.y()) / size.y();
            float tz = (c.z() - mn.z()) / size.z();
            float warpedTx = tx + 0.12f * std::sin(tz * float(M_PI) * 2.0f);
            int ix = qBound(0, int(warpedTx * float(sx)), sx - 1);
            int iy = qBound(0, int(ty * float(sy)), sy - 1);
            int iz = qBound(0, int(tz * float(sz)), sz - 1);
            return (iz * sy + iy) * sx + ix;
        };
        break;

    case GLViewport::CutMethod::Modular:
        totalParts = sx * sy * sz;
        assignBin  = [=](const QVector3D& c) {
            int ix = gridBin(c.x(), mn.x(), size.x(), sx);
            int iy = gridBin(c.y(), mn.y(), size.y(), sy);
            int iz = gridBin(c.z(), mn.z(), size.z(), sz);
            if (iz % 2 == 1) ix = (ix + sx / 2) % sx;
            return (iz * sy + iy) * sx + ix;
        };
        break;
    }

    QVector<MeshObject> parts(totalParts);
    const auto& verts = m_mesh.vertices();
    const auto& tris  = m_mesh.triangles();

    for (const auto& tri : tris) {
        const QVector3D centroid = (verts[tri.v0] + verts[tri.v1] + verts[tri.v2]) / 3.0f;
        MeshObject& part = parts[assignBin(centroid)];
        const int a = part.addVertex(verts[tri.v0]);
        const int b = part.addVertex(verts[tri.v1]);
        const int c = part.addVertex(verts[tri.v2]);
        part.addTriangle(a, b, c);
    }

    int nonEmpty = 0;
    for (MeshObject& p : parts) {
        if (!p.triangles().isEmpty()) { p.recomputeNormals(); ++nonEmpty; }
    }
    if (nonEmpty == 0) {
        QMessageBox::warning(this, QStringLiteral("Cut"), QStringLiteral("No parts were generated."));
        return;
    }

    const QString outDir = QFileDialog::getExistingDirectory(this, QStringLiteral("Select output folder for cut parts"));
    if (outDir.isEmpty()) return;

    const bool   numbering = m_partNumberYes->isChecked();
    const QString stem     = safeStem(m_currentFilePath);
    const QString methodName = m_methodCombo ? m_methodCombo->currentText() : QStringLiteral("cut");
    int written = 0;
    for (int i = 0; i < totalParts; ++i) {
        if (parts[i].triangles().isEmpty()) continue;
        QString fileName = QStringLiteral("%1_%2_part%3.stl")
                               .arg(stem)
                               .arg(methodName.section(QLatin1Char(' '), 0, 0).toLower())
                               .arg(i + 1);
        if (numbering)
            fileName = QStringLiteral("%1_").arg(written + 1, 3, 10, QLatin1Char('0')) + fileName;
        if (writeAsciiStl(QDir(outDir).filePath(fileName), parts[i]))
            ++written;
    }

    QMessageBox::information(
        this,
        QStringLiteral("Cut complete"),
        QStringLiteral("Method: %1\nExported %2 part files to:\n%3")
            .arg(methodName).arg(written).arg(outDir));

    pushHistory(HistoryEntry::Type::Cut,
                QStringLiteral("%1 %2x%3").arg(methodName.section(QLatin1Char(' '), 0, 0)).arg(sx).arg(sy));
}

bool MainWindow::askModelUnitForImport()
{
    QStringList items;
    items << QStringLiteral("Millimeter")
          << QStringLiteral("Centimeter")
          << QStringLiteral("Meter")
          << QStringLiteral("Inch");
    const int currentIndex = m_modelUnit == LinearUnit::Millimeters ? 0
                            : m_modelUnit == LinearUnit::Centimeters ? 1
                            : m_modelUnit == LinearUnit::Meters      ? 2
                                                                     : 3;

    bool ok = false;
    const QString selected = QInputDialog::getItem(
        this,
        QStringLiteral("Select unit"),
        QStringLiteral("Press Cancel to apply the last used unit."),
        items,
        currentIndex,
        false,
        &ok);
    if (!ok) {
        return true;
    }

    LinearUnit chosen = LinearUnit::Millimeters;
    if (selected == QStringLiteral("Centimeter")) {
        chosen = LinearUnit::Centimeters;
    } else if (selected == QStringLiteral("Meter")) {
        chosen = LinearUnit::Meters;
    } else if (selected == QStringLiteral("Inch")) {
        chosen = LinearUnit::Inches;
    }
    const int idx = m_unitCombo->findData(static_cast<int>(chosen));
    if (idx >= 0) {
        m_unitCombo->setCurrentIndex(idx);
    }
    return true;
}

void MainWindow::fitView()
{
    m_viewport->fitView();
    m_viewport->update();
}

void MainWindow::refreshMeshInViewport()
{
    m_viewport->setMesh(m_mesh);
}

void MainWindow::updateMeshInfoStatus()
{
    if (m_mesh.isEmpty()) {
        m_meshInfoLabel->setText(QStringLiteral("No mesh"));
        return;
    }
    const QVector3D mn = m_mesh.minBounds();
    const QVector3D mx = m_mesh.maxBounds();
    const double dx = mx.x() - mn.x();
    const double dy = mx.y() - mn.y();
    const double dz = mx.z() - mn.z();
    const QString dims = Units::formatSize3D(dx, dy, dz, m_modelUnit);
    m_meshInfoLabel->setText(
        QStringLiteral("%1 · %2 triangles")
            .arg(dims)
            .arg(m_mesh.triangles().size()));
}

void MainWindow::updateSceneBrowser()
{
    if (!m_sceneBrowser) return;
    const QString docName = m_currentFilePath.isEmpty()
        ? QStringLiteral("Untitled")
        : QFileInfo(m_currentFilePath).baseName();
    m_sceneBrowser->refresh(docName, !m_mesh.isEmpty(), true);
}

bool MainWindow::loadMeshFile(const QString& path, bool merge)
{
    MeshObject loaded;
    QString error;
    const QString lower = path.toLower();

    bool ok = false;
    if (lower.endsWith(QStringLiteral(".stl"))) {
        ok = StlLoader::load(path, loaded, &error);
    } else if (lower.endsWith(QStringLiteral(".obj"))) {
        ok = ObjLoader::load(path, loaded, &error);
    } else {
        error = QStringLiteral("Unsupported file extension.");
    }

    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("Import Error"), error);
        return false;
    }

    m_modelUnit = static_cast<LinearUnit>(m_unitCombo->currentData().toInt());

    const bool mergeIntoExisting = merge && !m_mesh.isEmpty();
    if (mergeIntoExisting) {
        applyModelUnitToNewGeometry(loaded);
        m_mesh.merge(loaded);
        m_mesh.layOnGroundCenterXZ();
    } else {
        m_mesh = loaded;
        applyModelUnitToNewGeometry(m_mesh);
    }

    refreshMeshInViewport();
    onCuttingPlaneChanged();
    m_currentFilePath = path;
    updateWindowTitle(path);
    updateMeshInfoStatus();
    updateSceneBrowser();

    // History: opening a new file resets history; importing appends
    if (!merge) {
        m_history.clear();
        m_histIdx = -1;
    }
    pushHistory(merge ? HistoryEntry::Type::ImportMesh : HistoryEntry::Type::LoadMesh,
                QFileInfo(path).baseName());

    statusBar()->showMessage(QStringLiteral("Loaded %1").arg(QFileInfo(path).fileName()), 4000);
    return true;
}

void MainWindow::onTransformChanged()
{
    if (!m_viewport || !m_xfTX) return;
    GLViewport::MeshTransform xf;
    xf.tx    = static_cast<float>(m_xfTX->value());
    xf.ty    = static_cast<float>(m_xfTY->value());
    xf.tz    = static_cast<float>(m_xfTZ->value());
    xf.rx    = static_cast<float>(m_xfRX->value());
    xf.ry    = static_cast<float>(m_xfRY->value());
    xf.rz    = static_cast<float>(m_xfRZ->value());
    xf.scale = static_cast<float>(m_xfScale->value());
    m_viewport->setMeshTransform(xf);
}

void MainWindow::applyTransform()
{
    if (!m_viewport || m_mesh.isEmpty() || !m_xfTX) return;
    const GLViewport::MeshTransform xf = m_viewport->meshTransform();
    if (xf.isIdentity()) return;

    QMatrix4x4 mat;
    const QVector3D c = m_mesh.center();
    mat.translate(xf.tx, xf.ty, xf.tz);
    mat.translate(c);
    mat.rotate(xf.rx, {1, 0, 0});
    mat.rotate(xf.ry, {0, 1, 0});
    mat.rotate(xf.rz, {0, 0, 1});
    mat.scale(xf.scale);
    mat.translate(-c);

    m_mesh.applyMatrix(mat);
    m_viewport->resetMeshTransform();
    resetTransformUI();
    refreshMeshInViewport();
    pushHistory(HistoryEntry::Type::Transform, QStringLiteral("Transform"));
    statusBar()->showMessage(QStringLiteral("Transform applied and baked into mesh"), 3000);
}

void MainWindow::resetTransformUI()
{
    if (!m_xfTX) return;
    const QList<QDoubleSpinBox*> txSpins{m_xfTX, m_xfTY, m_xfTZ, m_xfRX, m_xfRY, m_xfRZ};
    for (QDoubleSpinBox* s : txSpins) {
        QSignalBlocker blocker(s);
        s->setValue(0.0);
    }
    { QSignalBlocker blocker(m_xfScale); m_xfScale->setValue(1.0); }
    if (m_viewport) m_viewport->resetMeshTransform();
}

void MainWindow::pushHistory(HistoryEntry::Type type, const QString& label)
{
    // Truncate any rolled-back future before pushing
    if (m_histIdx < m_history.size() - 1)
        m_history.erase(m_history.begin() + m_histIdx + 1, m_history.end());

    HistoryEntry entry;
    entry.type  = type;
    entry.label = label;
    entry.mesh  = m_mesh;
    m_history.append(entry);
    m_histIdx = m_history.size() - 1;

    if (m_timeline)
        m_timeline->setEntries(m_history, m_histIdx);
}

void MainWindow::clearHistory()
{
    m_history.clear();
    m_histIdx = -1;
    if (m_timeline)
        m_timeline->setEntries(m_history, m_histIdx);
}

void MainWindow::applyHistoryAt(int index)
{
    m_histIdx = index;
    if (index < 0) {
        m_mesh.clear();
        m_viewport->clearMesh();
    } else {
        m_mesh = m_history[index].mesh;
        refreshMeshInViewport();
    }
    updateMeshInfoStatus();
    if (m_timeline)
        m_timeline->setEntries(m_history, m_histIdx);
}

void MainWindow::clearScene()
{
    m_mesh.clear();
    m_viewport->clearMesh();
    m_currentFilePath.clear();
    clearHistory();
    updateWindowTitle();
    updateMeshInfoStatus();
    updateSceneBrowser();
    statusBar()->showMessage(QStringLiteral("Scene cleared"), 3000);
}

void MainWindow::exitApp()
{
    close();
}

void MainWindow::setWireframeMode()
{
    m_viewport->setRenderMode(GLViewport::RenderMode::Wireframe);
}

void MainWindow::setSolidMode()
{
    m_viewport->setRenderMode(GLViewport::RenderMode::Solid);
}

void MainWindow::setSolidWireMode()
{
    m_viewport->setRenderMode(GLViewport::RenderMode::SolidWire);
}

void MainWindow::flipNormals()
{
    if (m_mesh.isEmpty()) {
        return;
    }
    m_mesh.flipNormals();
    refreshMeshInViewport();
    pushHistory(HistoryEntry::Type::FlipNormals, QStringLiteral("Flip"));
    statusBar()->showMessage(QStringLiteral("Normals flipped"), 2500);
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this,
        QStringLiteral("About Creation Theory Morph"),
        QStringLiteral("%1\nby %2\nVersion %3")
            .arg(QString::fromUtf8(CTM_PRODUCT_NAME))
            .arg(QString::fromUtf8(CTM_COMPANY_NAME))
            .arg(QString::fromUtf8(CTM_VERSION_STRING)));
}

void MainWindow::updateWindowTitle(const QString& filePath)
{
    const QString base = QStringLiteral("%1 %2")
                             .arg(QString::fromUtf8(CTM_PRODUCT_NAME))
                             .arg(QString::fromUtf8(CTM_VERSION_STRING));
    if (!filePath.isEmpty()) {
        setWindowTitle(QStringLiteral("%1 — %2").arg(base, QFileInfo(filePath).fileName()));
    } else {
        setWindowTitle(base);
    }
}
