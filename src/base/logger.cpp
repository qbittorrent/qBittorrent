#include "logger.h"

#include <QDateTime>
#include "base/utils/string.h"

Logger* Logger::m_instance = 0;

Logger::Logger()
    : lock(QReadWriteLock::Recursive)
    , msgCounter(0)
    , peerCounter(0)
{
}

Logger::~Logger() {}

Logger *Logger::instance()
{
    return m_instance;
}

void Logger::initInstance()
{
    if (!m_instance)
        m_instance = new Logger;
}

void Logger::freeInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = 0;
    }
}

void Logger::addMessage(const QString &message, const Log::MsgType &type)
{
    QWriteLocker locker(&lock);

    Log::Msg temp = { msgCounter++, QDateTime::currentMSecsSinceEpoch(), type, Utils::String::toHtmlEscaped(message) };
    m_messages.push_back(temp);

    if (m_messages.size() >= MAX_LOG_MESSAGES)
        m_messages.pop_front();

    emit newLogMessage(temp);
}

void Logger::addPeer(const QString &ip, bool blocked, const QString &reason)
{
    QWriteLocker locker(&lock);

    Log::Peer temp = { peerCounter++, QDateTime::currentMSecsSinceEpoch(), Utils::String::toHtmlEscaped(ip), blocked, Utils::String::toHtmlEscaped(reason) };
    m_peers.push_back(temp);

    if (m_peers.size() >= MAX_LOG_MESSAGES)
        m_peers.pop_front();

    emit newLogPeer(temp);
}

QVector<Log::Msg> Logger::getMessages(int lastKnownId) const
{
    QReadLocker locker(&lock);

    int diff = msgCounter - lastKnownId - 1;
    int size = m_messages.size();

    if ((lastKnownId == -1) || (diff >= size))
        return m_messages;

    if (diff <= 0)
        return QVector<Log::Msg>();

    return m_messages.mid(size - diff);
}

QVector<Log::Peer> Logger::getPeers(int lastKnownId) const
{
    QReadLocker locker(&lock);

    int diff = peerCounter - lastKnownId - 1;
    int size = m_peers.size();

    if ((lastKnownId == -1) || (diff >= size))
        return m_peers;

    if (diff <= 0)
        return QVector<Log::Peer>();

    return m_peers.mid(size - diff);
}
