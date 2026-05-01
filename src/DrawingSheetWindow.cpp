#include "DrawingSheetWindow.h"
#include "DrawingSheetWidget.h"
#include "Version.h"

#include <QAction>
#include <QComboBox>
#include <QFileDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>

DrawingSheetWindow::DrawingSheetWindow(const MeshObject& mesh,
                                       const QString& docName,
                                       QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Drawing — ") + docName +
                   QStringLiteral(" — " CTM_PRODUCT_NAME));
    resize(1100, 780);

    m_sheet = new DrawingSheetWidget(this);
    m_sheet->setMesh(mesh, docName);
    setCentralWidget(m_sheet);

    // ── Toolbar ──────────────────────────────────────────────────────────────
    QToolBar* tb = addToolBar(QStringLiteral("Drawing"));
    tb->setMovable(false);

    auto* exportAct = tb->addAction(QStringLiteral("Export PDF..."));
    connect(exportAct, &QAction::triggered, this, &DrawingSheetWindow::exportPDF);

    tb->addSeparator();
    tb->addWidget(new QLabel(QStringLiteral("  Size: "), tb));

    m_sizeBox = new QComboBox(tb);
    m_sizeBox->addItems({ QStringLiteral("A3"), QStringLiteral("A4") });
    tb->addWidget(m_sizeBox);

    tb->addWidget(new QLabel(QStringLiteral("  Orientation: "), tb));

    m_oriBox = new QComboBox(tb);
    m_oriBox->addItems({ QStringLiteral("Landscape"), QStringLiteral("Portrait") });
    tb->addWidget(m_oriBox);

    connect(m_sizeBox, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_sheet->setPaper(i == 0 ? DrawingSheetWidget::PaperSize::A3
                                 : DrawingSheetWidget::PaperSize::A4,
                          m_oriBox->currentIndex() == 0
                                 ? DrawingSheetWidget::Orientation::Landscape
                                 : DrawingSheetWidget::Orientation::Portrait);
    });
    connect(m_oriBox, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_sheet->setPaper(m_sizeBox->currentIndex() == 0
                                 ? DrawingSheetWidget::PaperSize::A3
                                 : DrawingSheetWidget::PaperSize::A4,
                          i == 0 ? DrawingSheetWidget::Orientation::Landscape
                                 : DrawingSheetWidget::Orientation::Portrait);
    });

    // ── Menu ─────────────────────────────────────────────────────────────────
    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(QStringLiteral("Export PDF..."), this,
                        &DrawingSheetWindow::exportPDF);
    fileMenu->addSeparator();
    auto* closeAct = fileMenu->addAction(QStringLiteral("Close"));
    closeAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));
    connect(closeAct, &QAction::triggered, this, &QWidget::close);

    statusBar()->showMessage(
        QStringLiteral("Scroll to zoom · Middle-drag to pan"));
}

void DrawingSheetWindow::exportPDF()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export Drawing as PDF"), QString(),
        QStringLiteral("PDF Files (*.pdf)"));
    if (path.isEmpty()) return;

    if (!m_sheet->exportPDF(path)) {
        QMessageBox::warning(this, QStringLiteral("Export Failed"),
                             QStringLiteral("Could not write PDF to:\n") + path);
    } else {
        statusBar()->showMessage(
            QStringLiteral("Exported: ") + path, 4000);
    }
}
