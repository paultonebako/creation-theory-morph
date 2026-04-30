#pragma once

#include <QFrame>
#include <QMainWindow>

#include "MeshObject.h"
#include "Units.h"

class GLViewport;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QRadioButton;
class QSlider;
class QSpinBox;
class QLabel;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void openMesh();
    void importMesh();
    void clearScene();
    void exitApp();
    void setWireframeMode();
    void setSolidMode();
    void setSolidWireMode();
    void flipNormals();
    void showAbout();
    void fitView();
    void onModelUnitChanged(int index);
    void updateMeshInfoStatus();
    void onCuttingPlaneChanged();
    void onConnectorSettingsChanged();
    void performCut();

private:
    bool loadMeshFile(const QString& path, bool merge);
    bool askModelUnitForImport();
    void applyModelUnitToNewGeometry(MeshObject& mesh);
    void createMenus();
    void createToolBar();
    void createDockPanels();
    void createStatusWidgets();
    void refreshMeshInViewport();
    void updateWindowTitle(const QString& filePath = {});
    void updateSceneBrowser();

    GLViewport* m_viewport = nullptr;
    QFrame* m_viewportFrame = nullptr;
    QComboBox* m_unitCombo = nullptr;
    QLabel* m_meshInfoLabel = nullptr;
    QWidget* m_cutPanel = nullptr;
    QSpinBox* m_cutXSpin = nullptr;
    QSpinBox* m_cutYSpin = nullptr;
    QSpinBox* m_cutZSpin = nullptr;
    QComboBox* m_adjustAxisCombo = nullptr;
    QSpinBox* m_adjustIndexSpin = nullptr;
    QCheckBox* m_staggeredCheck = nullptr;
    QDoubleSpinBox* m_adjustValueSpin = nullptr;
    QSlider* m_adjustSlider = nullptr;
    QDoubleSpinBox* m_cutChamferSpin = nullptr;
    QRadioButton* m_closeCutYes = nullptr;
    QRadioButton* m_closeCutNo = nullptr;
    QRadioButton* m_partNumberYes = nullptr;
    QRadioButton* m_partNumberNo = nullptr;

    QRadioButton* m_connectorPlug = nullptr;
    QRadioButton* m_connectorDowel = nullptr;
    QRadioButton* m_connectorNone = nullptr;
    QRadioButton* m_connectorPrism = nullptr;
    QRadioButton* m_connectorFrustum = nullptr;
    QComboBox* m_connectorShapeCombo = nullptr;
    QComboBox* m_connectorNumberCombo = nullptr;
    QDoubleSpinBox* m_connectorDepthRatio = nullptr;
    QDoubleSpinBox* m_connectorSize = nullptr;
    QDoubleSpinBox* m_connectorTolerance = nullptr;

    MeshObject m_mesh;
    QString m_currentFilePath;
    LinearUnit m_modelUnit = LinearUnit::Millimeters;
};
