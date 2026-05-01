#include "SplashScreen.h"
#include "Version.h"

#include <QApplication>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QProgressBar>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

SplashScreen::SplashScreen()
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setFixedSize(580, 300);

    if (QScreen* scr = QApplication::primaryScreen())
        move(scr->geometry().center() - rect().center());

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(44, 44, 44, 28);
    lay->addStretch();

    m_status = new QLabel(QStringLiteral("Initializing..."), this);
    m_status->setStyleSheet(QStringLiteral(
        "color: rgba(180,200,230,170); font-size: 10px; background: transparent;"));
    lay->addWidget(m_status);
    lay->addSpacing(8);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(3);
    m_progress->setStyleSheet(QStringLiteral(R"(
        QProgressBar {
            background: rgba(255,255,255,25);
            border: none;
            border-radius: 1px;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                stop:0 #3A7BD5, stop:1 #6FB0F5);
            border-radius: 1px;
        }
    )"));
    lay->addWidget(m_progress);
    lay->addSpacing(6);

    auto* ver = new QLabel(QStringLiteral("v" CTM_VERSION_STRING), this);
    ver->setStyleSheet(QStringLiteral(
        "color: rgba(140,160,195,110); font-size: 9px; background: transparent;"));
    lay->addWidget(ver);

    m_timer = new QTimer(this);
    m_timer->setInterval(180);
    connect(m_timer, &QTimer::timeout, this, [this] {
        static const char* msgs[] = {
            "Loading OpenGL context...",
            "Initializing scene graph...",
            "Building shader pipeline...",
            "Restoring workspace...",
            "Ready."
        };
        ++m_step;
        m_progress->setValue(m_step * 20);
        if (m_step < 5)
            m_status->setText(QLatin1String(msgs[m_step]));
        if (m_step >= 5) {
            m_timer->stop();
            emit finished();
        }
    });
}

void SplashScreen::start()
{
    show();
    m_timer->start();
}

void SplashScreen::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    // Background panel
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor(14, 18, 34, 248));
    bg.setColorAt(1.0, QColor( 8, 11, 22, 248));
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(r, 10, 10);

    // Top highlight sheen
    QLinearGradient sheen(0, 0, 0, 50);
    sheen.setColorAt(0, QColor(255, 255, 255, 16));
    sheen.setColorAt(1, QColor(255, 255, 255,  0));
    p.setBrush(sheen);
    p.drawRoundedRect(r, 10, 10);

    // Border
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 28), 1));
    p.drawRoundedRect(r, 10, 10);

    // Left accent bar
    p.setPen(Qt::NoPen);
    QLinearGradient bar(44, 52, 44, 52 + 70);
    bar.setColorAt(0, QColor(58, 123, 213, 220));
    bar.setColorAt(1, QColor(58, 123, 213,   0));
    p.setBrush(bar);
    p.drawRect(QRectF(44, 52, 3, 70));

    // Company name
    QFont f = font();
    f.setPointSize(10);
    f.setWeight(QFont::Light);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 4);
    p.setFont(f);
    p.setPen(QColor(110, 145, 200));
    p.drawText(QRect(56, 52, 460, 22), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("CREATION THEORY"));

    // App name
    f.setPointSize(26);
    f.setWeight(QFont::Thin);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 2);
    p.setFont(f);
    p.setPen(QColor(215, 225, 240));
    p.drawText(QRect(56, 74, 460, 48), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("MORPH"));

    // Horizontal rule
    QLinearGradient rule(56, 0, 400, 0);
    rule.setColorAt(0,   QColor(58, 123, 213, 160));
    rule.setColorAt(0.5, QColor(58, 123, 213,  60));
    rule.setColorAt(1,   QColor(58, 123, 213,   0));
    p.setPen(QPen(QBrush(rule), 1));
    p.drawLine(56, 128, 400, 128);
}
