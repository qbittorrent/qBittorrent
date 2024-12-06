/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "guiaddtorrentmanager.h"

#include <QScreen>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrentdescriptor.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/torrentfileguard.h"
#include "addnewtorrentdialog.h"
#include "interfaces/iguiapplication.h"
#include "mainwindow.h"
#include "raisedmessagebox.h"

namespace
{
    void adjustDialogGeometry(QWidget *dialog, const QWidget *parentWindow)
    {
        // It is preferable to place the dialog in the center of the parent window.
        // However, if it goes beyond the current screen, then move it so that it fits there
        // (or, if the dialog is larger than the current screen, at least make sure that
        // the upper/left coordinates of the dialog are inside it).

        QRect dialogGeometry = dialog->geometry();

        dialogGeometry.moveCenter(parentWindow->geometry().center());

        const QRect screenGeometry = parentWindow->screen()->availableGeometry();

        QPoint delta = screenGeometry.bottomRight() - dialogGeometry.bottomRight();
        if (delta.x() > 0)
            delta.setX(0);
        if (delta.y() > 0)
            delta.setY(0);
        dialogGeometry.translate(delta);

        const QPoint frameOffset {10, 40};
        delta = screenGeometry.topLeft() - dialogGeometry.topLeft() + frameOffset;
        if (delta.x() < 0)
            delta.setX(0);
        if (delta.y() < 0)
            delta.setY(0);
        dialogGeometry.translate(delta);

        dialog->setGeometry(dialogGeometry);
    }
}

GUIAddTorrentManager::GUIAddTorrentManager(IGUIApplication *app, BitTorrent::Session *session, QObject *parent)
    : GUIApplicationComponent(app, session, parent)
{
    connect(btSession(), &BitTorrent::Session::metadataDownloaded, this, &GUIAddTorrentManager::onMetadataDownloaded);
}

bool GUIAddTorrentManager::addTorrent(const QString &source, const BitTorrent::AddTorrentParams &params, const AddTorrentOption option)
{
    // `source`: .torrent file path,  magnet URI or URL

    if (source.isEmpty())
        return false;

    const auto *pref = Preferences::instance();

    if ((option == AddTorrentOption::SkipDialog)
            || ((option == AddTorrentOption::Default) && !pref->isAddNewTorrentDialogEnabled()))
    {
        return AddTorrentManager::addTorrent(source, params);
    }

    if (Net::DownloadManager::hasSupportedScheme(source))
    {
        LogMsg(tr("Downloading torrent... Source: \"%1\"").arg(source));
        // Launch downloader
        Net::DownloadManager::instance()->download(Net::DownloadRequest(source).limit(pref->getTorrentFileSizeLimit())
                , pref->useProxyForGeneralPurposes(), this, &GUIAddTorrentManager::onDownloadFinished);
        m_downloadedTorrents[source] = params;

        return true;
    }

    if (const auto parseResult = BitTorrent::TorrentDescriptor::parse(source))
    {
        return processTorrent(source, parseResult.value(), params);
    }
    else if (source.startsWith(u"magnet:", Qt::CaseInsensitive))
    {
        handleAddTorrentFailed(source, parseResult.error());
        return false;
    }

    const Path decodedPath {source.startsWith(u"file://", Qt::CaseInsensitive)
            ? QUrl::fromEncoded(source.toLocal8Bit()).toLocalFile() : source};
    auto torrentFileGuard = std::make_shared<TorrentFileGuard>(decodedPath);
    if (const auto loadResult = BitTorrent::TorrentDescriptor::loadFromFile(decodedPath))
    {
        const BitTorrent::TorrentDescriptor &torrentDescriptor = loadResult.value();
        const bool isProcessing = processTorrent(source, torrentDescriptor, params);
        if (isProcessing)
            setTorrentFileGuard(source, torrentFileGuard);
        return isProcessing;
    }
    else
    {
        handleAddTorrentFailed(decodedPath.toString(), loadResult.error());
        return false;
    }

    return false;
}

void GUIAddTorrentManager::onDownloadFinished(const Net::DownloadResult &result)
{
    const QString &source = result.url;
    const BitTorrent::AddTorrentParams addTorrentParams = m_downloadedTorrents.take(source);

    switch (result.status)
    {
    case Net::DownloadStatus::Success:
        if (const auto loadResult = BitTorrent::TorrentDescriptor::load(result.data))
            processTorrent(source, loadResult.value(), addTorrentParams);
        else
            handleAddTorrentFailed(source, loadResult.error());
        break;
    case Net::DownloadStatus::RedirectedToMagnet:
        if (const auto parseResult = BitTorrent::TorrentDescriptor::parse(result.magnetURI))
            processTorrent(source, parseResult.value(), addTorrentParams);
        else
            handleAddTorrentFailed(source, parseResult.error());
        break;
    default:
        handleAddTorrentFailed(source, result.errorString);
    }
}

void GUIAddTorrentManager::onMetadataDownloaded(const BitTorrent::TorrentInfo &metadata)
{
    Q_ASSERT(metadata.isValid());
    if (!metadata.isValid()) [[unlikely]]
        return;

    for (const auto &[infoHash, dialog] : m_dialogs.asKeyValueRange())
    {
        if (metadata.matchesInfoHash(infoHash))
            dialog->updateMetadata(metadata);
    }
}

bool GUIAddTorrentManager::processTorrent(const QString &source
        , const BitTorrent::TorrentDescriptor &torrentDescr, const BitTorrent::AddTorrentParams &params)
{
    const bool hasMetadata = torrentDescr.info().has_value();
    const BitTorrent::InfoHash infoHash = torrentDescr.infoHash();

    // Prevent showing the dialog if download is already present
    if (BitTorrent::Torrent *torrent = btSession()->findTorrent(infoHash))
    {
        if (Preferences::instance()->confirmMergeTrackers())
        {
            if (hasMetadata)
            {
                // Trying to set metadata to existing torrent in case if it has none
                torrent->setMetadata(*torrentDescr.info());
            }

            const bool isPrivate = torrent->isPrivate() || (hasMetadata && torrentDescr.info()->isPrivate());
            const QString dialogCaption = tr("Torrent is already present");
            if (isPrivate)
            {
                // We cannot merge trackers for private torrent but we still notify user
                // about duplicate torrent if confirmation dialog is enabled.
                RaisedMessageBox::warning(app()->mainWindow(), dialogCaption
                        , tr("Trackers cannot be merged because it is a private torrent."));
            }
            else
            {
                const bool mergeTrackers = btSession()->isMergeTrackersEnabled();
                const QMessageBox::StandardButton btn = RaisedMessageBox::question(app()->mainWindow(), dialogCaption
                        , tr("Torrent '%1' is already in the transfer list. Do you want to merge trackers from new source?").arg(torrent->name())
                        , (QMessageBox::Yes | QMessageBox::No), (mergeTrackers ? QMessageBox::Yes : QMessageBox::No));
                if (btn == QMessageBox::Yes)
                {
                    torrent->addTrackers(torrentDescr.trackers());
                    torrent->addUrlSeeds(torrentDescr.urlSeeds());
                }
            }
        }
        else
        {
            handleDuplicateTorrent(source, torrentDescr, torrent);
        }

        return false;
    }

    if (!hasMetadata)
        btSession()->downloadMetadata(torrentDescr);

    // By not setting a parent to the "AddNewTorrentDialog", all those dialogs
    // will be displayed on top and will not overlap with the main window.
    auto *dlg = new AddNewTorrentDialog(torrentDescr, params, nullptr);
    // Qt::Window is required to avoid showing only two dialog on top (see #12852).
    // Also improves the general convenience of adding multiple torrents.
    dlg->setWindowFlags(Qt::Window);

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    m_dialogs[infoHash] = dlg;
    connect(dlg, &AddNewTorrentDialog::torrentAccepted, this
            , [this, source, dlg](const BitTorrent::TorrentDescriptor &torrentDescr, const BitTorrent::AddTorrentParams &addTorrentParams)
    {
        if (dlg->isDoNotDeleteTorrentChecked())
        {
            if (auto torrentFileGuard = releaseTorrentFileGuard(source))
                torrentFileGuard->setAutoRemove(false);
        }

        addTorrentToSession(source, torrentDescr, addTorrentParams);
    });
    connect(dlg, &AddNewTorrentDialog::torrentRejected, this, [this, source]
    {
        releaseTorrentFileGuard(source);
    });
    connect(dlg, &QDialog::finished, this, [this, infoHash]
    {
        m_dialogs.remove(infoHash);
    });

    adjustDialogGeometry(dlg, app()->mainWindow());
    dlg->show();

    return true;
}
