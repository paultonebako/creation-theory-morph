#pragma once
#include <QMainWindow>
#include "MeshObject.h"

class DrawingSheetWidget;
class QComboBox;

class DrawingSheetWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit DrawingSheetWindow(const MeshObject& mesh,
                                const QString& docName,
                                QWidget* parent = nullptr);
private:
    void exportPDF();
    DrawingSheetWidget* m_sheet   = nullptr;
    QComboBox*          m_sizeBox = nullptr;
    QComboBox*          m_oriBox  = nullptr;
};
