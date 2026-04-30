#include "MainWindow.h"

#include "GLViewport.h"
#include "ObjLoader.h"
#include "StlLoader.h"
#include "Version.h"

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
    m_viewport = new GLViewport(this);
    m_viewportFrame = new QFrame(this);
    m_viewportFrame->setObjectName(QStringLiteral("viewportFrame"));
    m_viewportFrame->setFrameShape(QFrame::StyledPanel);
    m_viewportFrame->setFrameShadow(QFrame::Plain);
    auto* centralLay = new QVBoxLayout(m_viewportFrame);
    centralLay->setContentsMargins(2, 2, 2, 2);
    centralLay->setSpacing(0);
    centralLay->addWidget(m_viewport);
    setCentralWidget(m_viewportFrame);

    createDockPanels();
    createToolBar();
    createMenus();
    createStatusWidgets();

    resize(1400, 880);
    updateMeshInfoStatus();
    updateWindowTitle();
    updateSceneBrowser();
    onCuttingPlaneChanged();

    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #2d2d30; }"
        "QMenuBar { background: #3e3e42; color: #e0e0e0; padding: 2px; }"
        "QMenuBar::item:selected { background: #505052; }"
        "QMenu { background: #3e3e42; color: #e0e0e0; }"
        "QMenu::item:selected { background: #0e639c; }"
        "QToolBar { background: #3c3c40; border: none; spacing: 8px; padding: 6px; }"
        "QToolButton { background: transparent; color: #e0e0e0; padding: 4px 8px; border-radius: 3px; }"
        "QToolButton:hover { background: #505052; }"
        "QDockWidget { color: #e0e0e0; titlebar-close-icon: none; titlebar-normal-icon: none; }"
        "QDockWidget::title { background: #3e3e42; padding: 6px; font-weight: 600; }"
        "QGroupBox { border: 1px solid #4b4b4f; margin-top: 10px; padding-top: 8px; color: #e0e0e0; font-weight: 600; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QSpinBox { background: #3c3c40; color: #e0e0e0; border: 1px solid #555; padding: 1px 6px; }"
        "QRadioButton { color: #e0e0e0; }"
        "QStatusBar { background: #007acc; color: #ffffff; }"
        "QStatusBar QLabel { color: #ffffff; padding: 0 8px; }"
        "QComboBox { background: #3c3c40; color: #e0e0e0; border: 1px solid #555; padding: 2px 8px; min-width: 6em; }"
        "QFrame#viewportFrame { background: #1e1e1e; border: 1px solid #3f3f46; }"));
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
}

void MainWindow::createDockPanels()
{
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

    auto* dock = new QDockWidget(QStringLiteral("Cut Designer"), this);
    dock->setObjectName(QStringLiteral("dockCutDesigner"));
    dock->setWidget(m_cutPanel);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setMinimumWidth(280);
    addDockWidget(Qt::LeftDockWidgetArea, dock);

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
    connect(closeBtn, &QPushButton::clicked, this, &MainWindow::clearScene);
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
    editMenu->addAction(QStringLiteral("Undo"));
    editMenu->addAction(QStringLiteral("Redo"));
    editMenu->addSeparator();
    editMenu->addAction(QStringLiteral("Preferences"));

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
    // Scene browser was replaced by the LuBan-like cut designer dock.
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
    statusBar()->showMessage(QStringLiteral("Loaded %1").arg(QFileInfo(path).fileName()), 4000);
    return true;
}

void MainWindow::clearScene()
{
    m_mesh.clear();
    m_viewport->clearMesh();
    m_currentFilePath.clear();
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
