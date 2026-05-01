#pragma once

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;

class SceneBrowserWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SceneBrowserWidget(QWidget* parent = nullptr);
    void refresh(const QString& docName, bool hasMesh, bool meshVisible);

signals:
    void meshVisibilityChanged(bool visible);

private:
    void onItemClicked(QTreeWidgetItem* item, int col);
    void applyTreeStylesheet();
    static QIcon eyeIcon(bool open, bool darkMode);
    static QIcon dotIcon(const QColor& col);

    QTreeWidget*     m_tree       = nullptr;
    QTreeWidgetItem* m_bodiesItem = nullptr;
    QTreeWidgetItem* m_meshItem   = nullptr;
    bool             m_meshVis    = true;
    bool             m_darkMode   = true;
    QString          m_docName;
    bool             m_hasMesh    = false;

public:
    void setDarkMode(bool dark);
};
