#ifndef SEQUENTIALDOWNLOADGUARD_H
#define SEQUENTIALDOWNLOADGUARD_H

#include <QObject>

class QTimer;

namespace BitTorrent
{
    class Session;
    class TorrentHandle;

    /**
       @brief Enable / Disable Sequential download by torrents availability.

       Enable / Disable Sequential download by torrents availability, torrent has to be previewable
       to enable sequential download.
       Sequential download can be enabled globally, in main options and per torrent
       in Add new Torrent dialog.
     */
    class SequentialDownloadsGuard : public QObject
    {
        Q_OBJECT

    public:
        explicit SequentialDownloadsGuard(Session *const session, QTimer *const sequentialGuardTimer);

        static const int SEQ_DOWNLOAD_FIRST_REFRESH_INTERVAL = 5 * 1000; // 5sec

    public slots:
        void recheckSequentialDownloads();

    private:
        void updateSequentialDownload(TorrentHandle *const torrentHandle);
        bool isSequentialDownloadEnabled(TorrentHandle *const torrentHandle) const;
        void increaseTimerInterval();
        void enableSequentialDownload(TorrentHandle *const torrentHandle);
        void disableSequentialDownload(TorrentHandle *const torrentHandle);
        void logResult(TorrentHandle *const torrentHandle) const;

        Session *m_session;
        QTimer *m_sequentialGuardTimer;
        bool m_intervalIncreased = false;

        /**
           @brief If availability is higher than this value, sequential download for a torrent
           will be enabled.
         */
        static const int AVAILABILITY_LIMIT = 10;
        /**
           @brief Sequential download timer will be set to this value after first shot.
         */
        static const int SEQ_DOWNLOAD_REFRESH_INTERVAL = 15 * 1000; // 15sec
        // TODO: this could be configurable in advanced settings, will see what contributors says to QTimer implementation :) silver
    };
}

#endif // SEQUENTIALDOWNLOADGUARD_H
