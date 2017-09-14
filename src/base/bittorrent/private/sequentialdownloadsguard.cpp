#include "sequentialdownloadsguard.h"

#include <QDebug>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/logger.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"

using namespace BitTorrent;

/**
   @brief SequentialDownloadsGuard explicit Constructor.
 */
SequentialDownloadsGuard::SequentialDownloadsGuard(Session *const session, QTimer *const sequentialGuardTimer)
    : QObject(session)
    , m_session(session)
    , m_sequentialGuardTimer(sequentialGuardTimer)
{
}

/**
   @brief Recheck Sequential download for all active torrents.
 */
void SequentialDownloadsGuard::recheckSequentialDownloads()
{
    // Increase QTimer interval after first shot
    increaseTimerInterval();

    foreach (TorrentHandle *const torrentHandle, m_session->torrents())
        if (torrentHandle->isActive() && torrentHandle->isDownloading())
            updateSequentialDownload(torrentHandle);
}

/**
   @brief Enable / Disable Sequential download by torrents availability, torrent has to be previewable
   to enable sequential download.
 */
void SequentialDownloadsGuard::updateSequentialDownload(TorrentHandle *const torrentHandle)
{
    // TODO: tmp to remove before merge PR, so other contributors can check the behavior silver
    qDebug() << torrentHandle->name() << ":"
             << "sd" << (torrentHandle->isSequentialDownload() ? "[ON]" : "[OFF]")
             << "flp" << (torrentHandle->hasFirstLastPiecePriority() ? "[ON]" : "[OFF]")
             << "sdo" << (torrentHandle->isSequentialDownloadOverride() ? "[ON]" : "[OFF]");

    if (isSequentialDownloadEnabled(torrentHandle)
        && (torrentHandle->distributedCopies() > AVAILABILITY_LIMIT)
        && torrentHandle->isPreviewable())
        enableSequentialDownload(torrentHandle);
    else
        disableSequentialDownload(torrentHandle);
}

/**
   @brief Is Sequential download enabled?

   Takes into account global setting in Options dialog and possible override
   in Add new Torrent dialog.
 */
bool SequentialDownloadsGuard::isSequentialDownloadEnabled(TorrentHandle *const torrentHandle) const
{
    // If Sequential download was overriden in Add new Torrent dialog, return
    // that overriden value
    if (m_session->isSequentialDownload() != torrentHandle->isSequentialDownloadOverride())
        return torrentHandle->isSequentialDownloadOverride();

    return m_session->isSequentialDownload();
}

/**
   @brief Increase QTimer interval after first shot.
 */
void SequentialDownloadsGuard::increaseTimerInterval()
{
    if (!m_intervalIncreased) {
        m_sequentialGuardTimer->start(SEQ_DOWNLOAD_REFRESH_INTERVAL);
        m_intervalIncreased = true;
    }
}

/**
   @brief Enable Sequential download and first/last pieces and log message.
 */
void SequentialDownloadsGuard::enableSequentialDownload(TorrentHandle *const torrentHandle)
{
    const bool isSequentialDownload = torrentHandle->isSequentialDownload();
    const bool hasFirstLastPiecePriority = torrentHandle->hasFirstLastPiecePriority();

    // Separated if needed, because of fastresume
    if (!isSequentialDownload)
        torrentHandle->setSequentialDownload(true);
    if (!hasFirstLastPiecePriority)
        torrentHandle->setFirstLastPiecePriority(true);

    // Log Sequential download Guard's result, only when status changed
    if (!isSequentialDownload || !hasFirstLastPiecePriority)
        logResult(torrentHandle);
}

/**
   @brief Disable Sequential download and first/last pieces and log message.
 */
void SequentialDownloadsGuard::disableSequentialDownload(TorrentHandle *const torrentHandle)
{
    if (torrentHandle->isSequentialDownload()) {
        torrentHandle->setSequentialDownload(false);
        torrentHandle->setFirstLastPiecePriority(false);

        // Log Sequential download Guard's result, only when status changed
        logResult(torrentHandle);
    }
}

/**
   @brief Log Sequential download Guard's result.
 */
void SequentialDownloadsGuard::logResult(TorrentHandle *const torrentHandle) const
{
    QString logMessage = tr("Torrent '%1' : sequential download %2, first/last pieces %3")
        .arg(torrentHandle->name())
        .arg(torrentHandle->isSequentialDownload() ? tr("[ON]") : tr("[OFF]"))
        .arg(torrentHandle->hasFirstLastPiecePriority() ? tr("[ON]") : tr("[OFF]"));

    Logger::instance()->addMessage(logMessage, Log::NORMAL);
    qDebug("%s", qUtf8Printable(logMessage));
}
