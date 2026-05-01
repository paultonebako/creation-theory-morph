#pragma once

#include <QDialog>

class QRadioButton;

class PreferencesDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Theme { Dark, Light };

    explicit PreferencesDialog(Theme current, QWidget* parent = nullptr);
    Theme selectedTheme() const;

private:
    QRadioButton* m_darkBtn  = nullptr;
    QRadioButton* m_lightBtn = nullptr;
};
