#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QVector>
#include <QReadWriteLock>
#include <QObject>

const int MAX_LOG_MESSAGES = 1000;

namespace Log
{
    enum MsgType
    {
        NORMAL,
        INFO,
        WARNING,
        CRITICAL //ERROR is defined by libtorrent and results in compiler error
    };

    struct Msg
    {
        Msg();
        Msg(int id, MsgType type, const QString &message);
        int id;
        qint64 timestamp;
        MsgType type;
        QString message;
    };

    struct Peer
    {
        Peer(int id, const QString &ip, bool blocked, const QString &reason);
        Peer();
        int id;
        qint64 timestamp;
        QString ip;
        bool blocked;
        QString reason;
    };
}

class Logger : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Logger)

public:
    static void initInstance();
    static void freeInstance();
    static Logger *instance();

    void addMessage(const QString &message, const Log::MsgType &type = Log::NORMAL);
    void addPeer(const QString &ip, bool blocked, const QString &reason = QString());
    QVector<Log::Msg> getMessages(int lastKnownId = -1) const;
    QVector<Log::Peer> getPeers(int lastKnownId = -1) const;

signals:
    void newLogMessage(const Log::Msg &message);
    void newLogPeer(const Log::Peer &peer);

private:
    Logger();
    ~Logger();

    static Logger* m_instance;
    QVector<Log::Msg> m_messages;
    QVector<Log::Peer> m_peers;
    mutable QReadWriteLock lock;
    int msgCounter;
    int peerCounter;
};

#endif // LOGGER_H
