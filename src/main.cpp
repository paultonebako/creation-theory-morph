#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleFactory>
#include <QSurfaceFormat>

#include "MainWindow.h"

static void applyAzeretMono(QApplication& app)
{
    // Load from bundled file if present, otherwise rely on system installation.
    // To bundle: place AzeretMono-Regular.ttf in resources/fonts/ and add to a .qrc.
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/AzeretMono-Regular.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/AzeretMono-Bold.ttf"));

    const QString family = QStringLiteral("Azeret Mono");
    const QStringList families = QFontDatabase::families();
    if (families.contains(family)) {
        QFont f(family, 9);
        app.setFont(f);
    }
    // If not found, leave the Fusion default font (no hard crash).
}

static void applyFusionDarkTheme(QApplication& app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette p;
    const QColor black(0, 0, 0);
    const QColor white(255, 255, 255);
    const QColor nearBlack(6, 6, 6);
    const QColor subtle(20, 20, 20);
    const QColor midGray(80, 80, 80);
    p.setColor(QPalette::Window,          black);
    p.setColor(QPalette::WindowText,      white);
    p.setColor(QPalette::Base,            nearBlack);
    p.setColor(QPalette::AlternateBase,   QColor(10, 10, 10));
    p.setColor(QPalette::ToolTipBase,     nearBlack);
    p.setColor(QPalette::ToolTipText,     white);
    p.setColor(QPalette::Text,            white);
    p.setColor(QPalette::BrightText,      white);
    p.setColor(QPalette::Button,          QColor(12, 12, 12));
    p.setColor(QPalette::ButtonText,      white);
    p.setColor(QPalette::Highlight,       subtle);
    p.setColor(QPalette::HighlightedText, white);
    p.setColor(QPalette::Link,            QColor(180, 180, 255));
    p.setColor(QPalette::Mid,             QColor(18, 18, 18));
    p.setColor(QPalette::Midlight,        QColor(25, 25, 25));
    p.setColor(QPalette::Dark,            black);
    p.setColor(QPalette::Shadow,          black);
    p.setColor(QPalette::PlaceholderText, midGray);
    app.setPalette(p);
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
    applyAzeretMono(app);

    MainWindow w;
    w.show();
    return app.exec();
}
