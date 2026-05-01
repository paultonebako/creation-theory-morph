#pragma once
#include <QWidget>

class QLabel;
class QProgressBar;
class QTimer;

class SplashScreen : public QWidget
{
    Q_OBJECT
public:
    explicit SplashScreen();
    void start(); // show and begin progress

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    QProgressBar* m_progress = nullptr;
    QLabel*       m_status   = nullptr;
    QTimer*       m_timer    = nullptr;
    int           m_step     = 0;
};
