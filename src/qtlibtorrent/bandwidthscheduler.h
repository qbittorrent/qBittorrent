#ifndef BANDWIDTHSCHEDULER_H
#define BANDWIDTHSCHEDULER_H

#include <QTimer>
#include <QTime>
#include <QDateTime>
#include "preferences.h"
#include <iostream>


class BandwidthScheduler: public QTimer {
  Q_OBJECT

public:
  BandwidthScheduler(QObject *parent): QTimer(parent) {
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
    bool alt_bw_enabled = pref.isAltBandwidthEnabled();

    QTime start = pref.getSchedulerStartTime();
    QTime end = pref.getSchedulerEndTime();
    QTime now = QTime::currentTime();
    int sched_days = pref.getSchedulerDays();
    int day = QDateTime::currentDateTime().toLocalTime().date().dayOfWeek();
    bool new_mode = false;
    bool reverse = false;

    if (start > end) {
      QTime temp = start;
      start = end;
      end = temp;
      reverse = true;
    }

    if (start <= now && end >= now) {
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

    if (reverse)
      new_mode = !new_mode;

    if (new_mode != alt_bw_enabled)
      emit switchToAlternativeMode(new_mode);

    // Timeout regularly to accomodate for external system clock changes
    // eg from the user or from a timesync utility
    QTimer::start(1500);
  }

signals:
  void switchToAlternativeMode(bool alternative);
};

#endif // BANDWIDTHSCHEDULER_H
