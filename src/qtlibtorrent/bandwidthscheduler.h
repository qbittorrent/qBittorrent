#ifndef BANDWIDTHSCHEDULER_H
#define BANDWIDTHSCHEDULER_H

#include <QTimer>
#include <QTime>
#include <QDateTime>
#include "preferences.h"
#include <iostream>

#define SCHED_INTERVAL 2500

class BandwidthScheduler: public QTimer {
  Q_OBJECT

private:
  bool in_alternative_mode;

public:
  BandwidthScheduler(QObject *parent): QTimer(parent), in_alternative_mode(false) {
    Q_ASSERT(Preferences().isSchedulerEnabled());
    // Signal shot, we call start() again manually
    setSingleShot(true);
    // Connect Signals/Slots
    connect(this, SIGNAL(timeout()), this, SLOT(start()));
  }

public slots:
  void start() {
    const Preferences pref;
    Q_ASSERT(pref.isSchedulerEnabled());

    QTime start = pref.getSchedulerStartTime();
    QTime end = pref.getSchedulerEndTime();
    QTime now = QTime::currentTime();
    int sched_days = pref.getSchedulerDays();
    int day = QDateTime::currentDateTime().toLocalTime().date().dayOfWeek();
    bool new_mode = false;

    if (start < end && start <= now && end >= now) {
      switch(sched_days) {
      case EVERY_DAY:
        new_mode = true;
        break;
      case WEEK_ENDS:
        if (day == 6 || day == 7)
          new_mode = true;
        break;
      case WEEK_DAYS:
        if (day != 6 && day != 7)
          new_mode = true;
        break;
      default:
        if (day == sched_days - 2)
          new_mode = true;
        break;
      }
    }

    if (new_mode != in_alternative_mode) {
      in_alternative_mode = new_mode;
      emit switchToAlternativeMode(new_mode);
    }

    // Timeout regularly to accomodate for external system clock changes
    // eg from the user or from a timesync utility
    QTimer::start(SCHED_INTERVAL);
  }

signals:
  void switchToAlternativeMode(bool alternative);
};

#endif // BANDWIDTHSCHEDULER_H
