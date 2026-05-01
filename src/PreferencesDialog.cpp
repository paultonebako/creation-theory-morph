#include "PreferencesDialog.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(Theme current, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Preferences"));
    setMinimumWidth(300);

    auto* lay = new QVBoxLayout(this);
    lay->setSpacing(14);
    lay->setContentsMargins(18, 18, 18, 18);

    auto* themeGroup = new QGroupBox(QStringLiteral("Theme"), this);
    auto* tLay = new QVBoxLayout(themeGroup);
    m_darkBtn  = new QRadioButton(QStringLiteral("Dark"), themeGroup);
    m_lightBtn = new QRadioButton(QStringLiteral("Light"), themeGroup);
    tLay->addWidget(m_darkBtn);
    tLay->addWidget(m_lightBtn);
    (current == Theme::Light ? m_lightBtn : m_darkBtn)->setChecked(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    lay->addWidget(themeGroup);
    lay->addStretch(1);
    lay->addWidget(buttons);
}

PreferencesDialog::Theme PreferencesDialog::selectedTheme() const
{
    return m_lightBtn->isChecked() ? Theme::Light : Theme::Dark;
}
