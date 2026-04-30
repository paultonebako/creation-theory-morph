#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStyleFactory>
#include <QSurfaceFormat>

#include "MainWindow.h"

static void applyFusionDarkTheme(QApplication& app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette dark;
    dark.setColor(QPalette::Window, QColor(45, 45, 48));
    dark.setColor(QPalette::WindowText, Qt::white);
    dark.setColor(QPalette::Base, QColor(30, 30, 30));
    dark.setColor(QPalette::AlternateBase, QColor(45, 45, 48));
    dark.setColor(QPalette::ToolTipBase, QColor(45, 45, 48));
    dark.setColor(QPalette::ToolTipText, Qt::white);
    dark.setColor(QPalette::Text, Qt::white);
    dark.setColor(QPalette::Button, QColor(60, 60, 64));
    dark.setColor(QPalette::ButtonText, Qt::white);
    dark.setColor(QPalette::BrightText, Qt::red);
    dark.setColor(QPalette::Highlight, QColor(14, 99, 156));
    dark.setColor(QPalette::HighlightedText, Qt::white);
    dark.setColor(QPalette::Link, QColor(100, 184, 255));
    app.setPalette(dark);
}

int main(int argc, char* argv[])
{
    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setVersion(2, 1);
    fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    applyFusionDarkTheme(app);

    MainWindow w;
    w.show();
    return app.exec();
}
