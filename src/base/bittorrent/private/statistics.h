#ifndef STATISTICS_H
#define STATISTICS_H

#include <QObject>
#include <QTimer>

namespace BitTorrent { class Session; }

class Statistics : QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Statistics)

public:
    Statistics(BitTorrent::Session *session);
    ~Statistics();

    quint64 getAlltimeDL() const;
    quint64 getAlltimeUL() const;

private slots:
    void gather();

private:
    void save() const;
    void load();

private:
    BitTorrent::Session *m_session;
    // Will overflow at 15.9 EiB
    quint64 m_alltimeUL;
    quint64 m_alltimeDL;
    qint64 m_sessionUL;
    qint64 m_sessionDL;
    mutable qint64 m_lastWrite;
    mutable bool m_dirty;

    QTimer m_timer;
};

#endif // STATISTICS_H
