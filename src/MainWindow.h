#pragma once

#include <QFrame>
#include <QList>
#include <QMainWindow>

#include "HistoryEntry.h"
#include "MeshObject.h"
#include "Units.h"

class GLViewport;
class QCheckBox;
class QDockWidget;
class SceneBrowserWidget;
class TimelineWidget;
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
    enum class ThemeMode { Dark, Light };

    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void openMesh();
    void showPreferences();
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
    void onCutMethodChanged(int index);
    void onConnectorSettingsChanged();
    void performCut();
    void applyHistoryAt(int index);
    void onTransformChanged();
    void applyTransform();
    void resetTransformUI();

private:
    void applyTheme(ThemeMode mode);
    static QString darkStyleSheet();
    static QString lightStyleSheet();
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
    void pushHistory(HistoryEntry::Type type, const QString& label);
    void clearHistory();

    GLViewport*          m_viewport      = nullptr;
    TimelineWidget*      m_timeline      = nullptr;
    SceneBrowserWidget*  m_sceneBrowser  = nullptr;
    QDockWidget*         m_browserDock   = nullptr;
    QDockWidget*         m_cutDock       = nullptr;
    QDockWidget*         m_xfDock        = nullptr;
    QList<HistoryEntry> m_history;
    int m_histIdx = -1;

    // Transform panel widgets
    QDoubleSpinBox* m_xfTX = nullptr;
    QDoubleSpinBox* m_xfTY = nullptr;
    QDoubleSpinBox* m_xfTZ = nullptr;
    QDoubleSpinBox* m_xfRX = nullptr;
    QDoubleSpinBox* m_xfRY = nullptr;
    QDoubleSpinBox* m_xfRZ = nullptr;
    QDoubleSpinBox* m_xfScale = nullptr;
    QComboBox* m_methodCombo = nullptr;
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
    ThemeMode m_themeMode = ThemeMode::Dark;
};
