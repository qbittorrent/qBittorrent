#ifndef BANDWIDTHSCHEDULER_H
#define BANDWIDTHSCHEDULER_H

#include <QTimer>

class BandwidthScheduler : public QTimer
{
    Q_OBJECT

public:
    BandwidthScheduler(QObject *parent = 0);

public slots:
    void start();

signals:
    void switchToAlternativeMode(bool alternative);
};

#endif // BANDWIDTHSCHEDULER_H
