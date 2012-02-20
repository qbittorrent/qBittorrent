#ifndef BANDWIDTHSCHEDULER_H
#define BANDWIDTHSCHEDULER_H

#include <QTimer>
#include <QTime>
#include <QDateTime>
#include "preferences.h"
#include <iostream>

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
    connect(this, SIGNAL(timeout()), this, SLOT(switchMode()));
  }

public slots:
  void start() {
    const Preferences pref;
    Q_ASSERT(pref.isSchedulerEnabled());

    QTime startAltSpeeds = pref.getSchedulerStartTime();
    QTime endAltSpeeds = pref.getSchedulerEndTime();
    if (startAltSpeeds == endAltSpeeds) {
      std::cerr << "Error: bandwidth scheduler have the same start time and end time." << std::endl;
      std::cerr << "The bandwidth scheduler will be disabled" << std::endl;
      stop();
      emit switchToAlternativeMode(false);
      return;
    }

    // Determine what the closest QTime is
    QTime now = QTime::currentTime();
    uint time_to_start = secsTo(now, startAltSpeeds);
    uint time_to_end = secsTo(now, endAltSpeeds);
    if (time_to_end < time_to_start) {
      // We should be in alternative mode
      in_alternative_mode = true;
      // Start counting
      QTimer::start(time_to_end*1000);
    } else {
      // We should be in normal mode
      in_alternative_mode = false;
      // Start counting
      QTimer::start(time_to_start*1000);
    }
    // Send signal to notify BTSession
    emit switchToAlternativeMode(in_alternative_mode);
  }

  void switchMode() {
    const Preferences pref;
    // Get the day this mode was started (either today or yesterday)
    QDate current_date = QDateTime::currentDateTime().toLocalTime().date();
    int day = current_date.dayOfWeek();
    if (in_alternative_mode) {
      // It is possible that starttime was yesterday
      if (QTime::currentTime().secsTo(pref.getSchedulerStartTime()) > 0) {
        current_date.addDays(-1); // Go to yesterday
        day = current_date.day();
      }
    }
    // Check if the day is in scheduler days
    // Notify BTSession only if necessary
    switch(pref.getSchedulerDays()) {
    case EVERY_DAY:
      emit switchToAlternativeMode(!in_alternative_mode);
      break;
    case WEEK_ENDS:
      if (day == Qt::Saturday || day == Qt::Sunday)
        emit switchToAlternativeMode(!in_alternative_mode);
      break;
    case WEEK_DAYS:
      if (day != Qt::Saturday && day != Qt::Sunday)
        emit switchToAlternativeMode(!in_alternative_mode);
      break;
    default:
      // Convert our enum index to Qt enum index
      int scheduler_day = ((int)pref.getSchedulerDays()) - 2;
      if (day == scheduler_day)
        emit switchToAlternativeMode(!in_alternative_mode);
      break;
    }
    // Call start again
    start();
  }

signals:
  void switchToAlternativeMode(bool alternative);

private:
  // Qt function can return negative values and we
  // don't want that
  uint secsTo(QTime now, QTime t) {
    int diff = now.secsTo(t);
    if (diff < 0) {
      // 86400 seconds in a day
      diff += 86400;
    }
    Q_ASSERT(diff >= 0);
    return diff;
  }
};

#endif // BANDWIDTHSCHEDULER_H
