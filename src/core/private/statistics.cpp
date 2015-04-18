#include <QDateTime>

#include <libtorrent/session.hpp>

#include "core/qinisettings.h"
#include "core/preferences.h"
#include "core/bittorrent/sessionstatus.h"
#include "core/bittorrent/session.h"
#include "statistics.h"

static const qint64 SAVE_INTERVAL = 15 * 60 * 1000;

namespace libt = libtorrent;
using namespace BitTorrent;

Statistics::Statistics(Session *session)
    : QObject(session)
    , m_session(session)
    , m_sessionUL(0)
    , m_sessionDL(0)
    , m_lastWrite(0)
    , m_dirty(false)
{
    load();
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(gather()));
    m_timer.start(60 * 1000);
}

Statistics::~Statistics()
{
    if (m_dirty)
        m_lastWrite = 0;
    save();
}

quint64 Statistics::getAlltimeDL() const
{
    return m_alltimeDL + m_sessionDL;
}

quint64 Statistics::getAlltimeUL() const
{
    return m_alltimeUL + m_sessionUL;
}

void Statistics::gather()
{
    SessionStatus ss = m_session->status();
    if (ss.totalDownload() > m_sessionDL) {
        m_sessionDL = ss.totalDownload();
        m_dirty = true;
    }
    if (ss.totalUpload() > m_sessionUL) {
        m_sessionUL = ss.totalUpload();
        m_dirty = true;
    }

    save();
}

void Statistics::save() const
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (!m_dirty || ((now - m_lastWrite) < SAVE_INTERVAL))
        return;

    QIniSettings s("qBittorrent", "qBittorrent-data");
    QVariantHash v;
    v.insert("AlltimeDL", m_alltimeDL + m_sessionDL);
    v.insert("AlltimeUL", m_alltimeUL + m_sessionUL);
    s.setValue("Stats/AllStats", v);
    m_dirty = false;
    m_lastWrite = now;
}

void Statistics::load()
{
    // Temp code. Versions v3.1.4 and v3.1.5 saved the data in the qbittorrent.ini file.
    // This code reads the data from there, writes it to the new file, and removes the keys
    // from the old file. This code should be removed after some time has passed.
    // e.g. When we reach v3.3.0
    // Don't forget to remove:
    // 1. Preferences::getStats()
    // 2. Preferences::removeStats()
    // 3. #include "core/preferences.h"
    Preferences* const pref = Preferences::instance();
    QIniSettings s("qBittorrent", "qBittorrent-data");
    QVariantHash v = pref->getStats();

    // Let's test if the qbittorrent.ini holds the key
    if (!v.isEmpty()) {
        m_dirty = true;

        // If the user has used qbt > 3.1.5 and then reinstalled/used
        // qbt < 3.1.6, there will be stats in qbittorrent-data.ini too
        // so we need to merge those 2.
        if (s.contains("Stats/AllStats")) {
            QVariantHash tmp = s.value("Stats/AllStats").toHash();
            v["AlltimeDL"] = v["AlltimeDL"].toULongLong() + tmp["AlltimeDL"].toULongLong();
            v["AlltimeUL"] = v["AlltimeUL"].toULongLong() + tmp["AlltimeUL"].toULongLong();
        }
    }
    else {
        v = s.value("Stats/AllStats").toHash();
    }

    m_alltimeDL = v["AlltimeDL"].toULongLong();
    m_alltimeUL = v["AlltimeUL"].toULongLong();

    if (m_dirty) {
        save();
        pref->removeStats();
    }
}
