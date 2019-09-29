/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Prince Gupta <jagannatharjun11@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If
 * you modify file(s), you may extend this exception to your version of the
 * file(s), but you are not obligated to do so. If you do not wish to do so,
 * delete this exception statement from your version.
 */

#include "uithememanager.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QResource>
#include <QSet>

#include "base/iconprovider.h"
#include "base/exceptions.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/utils/fs.h"
#include "utils.h"

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
#define QBT_SYSTEMICONS
#endif

namespace
{
    static const QHash<QString, QString> iconConfig = {
        {"AboutDialog.Logo", ":icons/skin/qbittorrent-tray.svg"},
        {"AddNewTorrentDialog.TorrentContentView.RenameAction", ":icons/qbt-theme/edit-rename.svg"},
        {"CategoryFilter.ContextMenu.AddAction", ":icons/qbt-theme/list-add.svg"},
        {"CategoryFilter.ContextMenu.AddSubcategoryAction", ":icons/qbt-theme/list-add.svg"},
        {"CategoryFilter.ContextMenu.EditAction", ":icons/qbt-theme/document-edit.svg"},
        {"CategoryFilter.ContextMenu.RemoveAction", ":icons/qbt-theme/list-remove.svg"},
        {"CategoryFilter.ContextMenu.Torrent.DeleteAction", ":icons/qbt-theme/edit-delete.svg"},
        {"CategoryFilter.ContextMenu.Torrent.PauseAction", ":icons/qbt-theme/media-playback-pause.svg"},
        {"CategoryFilter.ContextMenu.Torrent.ResumeAction", ":icons/qbt-theme/media-playback-start.svg"},
        {"CategoryFilter.ContextMenu.Unused.RemoveAction", ":icons/qbt-theme/list-remove.svg"},
        {"CategoryFilter.SubCategory", ":icons/qbt-theme/inode-directory.svg"},
        {"CookiesDialog.AddAction", ":icons/qbt-theme/list-add.svg"},
        {"CookiesDialog.RemoveAction", ":icons/qbt-theme/list-remove.svg"},
        {"CookiesDialog", ":icons/qbt-theme/preferences-web-browser-cookies.svg"},
        {"DownloadedEpisodes.RemoveAction", ":icons/qbt-theme/edit-clear.svg"},
        {"ExecutionLog.BlockedIPsTab", ":icons/qbt-theme/view-filter.svg"},
        {"ExecutionLog.GeneralTab", ":icons/qbt-theme/view-calendar-journal.svg"},
        {"FileIconProvider.Cache", ":icons/qbt-theme/text-plain.svg"},
        {"LineEdit.FindAction", ":icons/qbt-theme/edit-find.svg"},
        {"LogList.ContextMenu.ClearAction", ":icons/qbt-theme/edit-clear.svg"},
        {"LogList.ContextMenu.CopyAction", ":icons/qbt-theme/edit-copy.svg"},
        {"MainMenu.ConfigureAction", ":icons/qbt-theme/configure.svg"},
        {"MainMenu.DonateAction", ":icons/qbt-theme/wallet-open.svg"},
        {"MainMenu.DownAction", ":icons/qbt-theme/go-up.svg"},
        {"MainMenu.DownloadLimit", ":icons/qbt-theme/kt-set-max-download-speed.svg"},
        {"MainMenu.ExecutionLogTab", ":icons/qbt-theme/view-calendar-journal.svg"},
        {"MainMenu.ExitAction", ":icons/qbt-theme/application-exit.svg"},
        {"MainMenu.GlobalDownloadLimit", ":icons/qbt-theme/kt-set-max-download-speed.svg"},
        {"MainMenu.GlobalUploadLimit", ":icons/qbt-theme/kt-set-max-upload-speed.svg"},
        {"MainMenu.HelpAction", ":icons/qbt-theme/help-about.svg"},
        {"MainMenu.Documentation.HelpAction", ":icons/qbt-theme/help-contents.svg"},
        {"MainMenu.LockAction", ":icons/qbt-theme/object-locked.svg"},
        {"MainMenu.ManageCookies", ":icons/qbt-theme/preferences-web-browser-cookies.svg"},
        {"MainMenu.PauseAction", ":icons/qbt-theme/media-playback-pause.svg"},
        {"MainMenu.PauseAllAction", ":icons/qbt-theme/media-playback-pause.svg"},
        {"MainMenu.Queue.BottomAction", ":icons/qbt-theme/go-bottom.svg"},
        {"MainMenu.Queue.DownAction", ":icons/qbt-theme/go-down.svg"},
        {"MainMenu.RSSTab", ":icons/qbt-theme/application-rss+xml.svg"},
        {"MainMenu.SearchTab", ":icons/qbt-theme/edit-find.svg"},
        {"MainMenu.Shutdown.ExitAction", ":icons/qbt-theme/application-exit.svg"},
        {"MainMenu.StartAction", ":icons/qbt-theme/media-playback-start.svg"},
        {"MainMenu.StartAllAction", ":icons/qbt-theme/media-playback-start.svg"},
        {"MainMenu.Statistics", ":icons/qbt-theme/view-statistics.svg"},
        {"MainMenu.Torrent.CreateAction", ":icons/qbt-theme/document-edit.svg"},
        {"MainMenu.Torrent.RemoveAction", ":icons/qbt-theme/list-remove.svg"},
        {"MainMenu.TorrentFile.OpenAction", ":icons/qbt-theme/list-add.svg"},
        {"MainMenu.TorrentURL.OpenAction", ":icons/qbt-theme/insert-link.svg"},
        {"MainMenu.TransfersTab", ":icons/qbt-theme/folder-remote.svg"},
        {"MainMenu.UpAction", ":icons/qbt-theme/go-top.svg"},
        {"MainMenu.UploadLimit", ":icons/qbt-theme/kt-set-max-upload-speed.svg"},
        {"MainWindow.Logo", ":icons/skin/qbittorrent-tray.svg"},
        {"OptionsDialog.AdvancedTab", ":icons/qbt-theme/preferences-other.svg"},
        {"OptionsDialog.BitTorrentTab", ":icons/qbt-theme/preferences-system-network.svg"},
        {"OptionsDialog.ConnectionTab", ":icons/qbt-theme/network-wired.svg"},
        {"OptionsDialog.ConnectionTab.IPFilter.RefreshAction", ":icons/qbt-theme/view-refresh.svg"},
        {"OptionsDialog.DownloadTab", ":icons/qbt-theme/folder-download.svg"},
        {"OptionsDialog.RSSTab", ":icons/qbt-theme/rss-config.svg"},
        {"OptionsDialog.SpeedTab", ":icons/qbt-theme/speedometer.svg"},
        {"OptionsDialog.UITab", ":icons/qbt-theme/preferences-desktop.svg"},
        {"OptionsDialog.WebUITab", ":icons/qbt-theme/network-server.svg"},
        {"OptionsDialog.GlobalRate", ":icons/slow_off.svg"},
        {"OptionsDialog.AlternativeRate", ":icons/slow.svg"},
        {"OptionsDialog.SSL.LowSecurity", ":icons/qbt-theme/security-low.svg"},
        {"OptionsDialog.SSL.HighSecurity", ":icons/qbt-theme/security-high.svg"},
        {"PeerList.ContextMenu.AddAction", ":icons/qbt-theme/user-group-new.svg"},
        {"PeerList.ContextMenu.BanAction", ":icons/qbt-theme/user-group-delete.svg"},
        {"PeerList.ContextMenu.CopyAction", ":icons/qbt-theme/edit-copy.svg"},
        {"RSS", ":icons/qbt-theme/inode-directory.svg"},
        {"RSS.Available", ":icons/qbt-theme/application-rss+xml.svg"},
        {"RSS.EpisodeFilter.ErrorAction", ":icons/qbt-theme/task-attention.svg"},
        {"RSS.Feed.DownloadAction", ":icons/qbt-theme/download.svg"},
        {"RSS.Feed.MarkAllRead", ":icons/qbt-theme/mail-mark-read.svg"},
        {"RSS.Feed.MarkReadAction", ":icons/qbt-theme/mail-mark-read.svg"},
        {"RSS.Feed.NewFolderAction", ":icons/qbt-theme/folder-new.svg"},
        {"RSS.Feed.NewsURL.OpenAction", ":icons/qbt-theme/application-x-mswinurl.svg"},
        {"RSS.Feed.RefreshAction", ":icons/qbt-theme/view-refresh.svg"},
        {"RSS.Feed.RefreshAllAction", ":icons/qbt-theme/view-refresh.svg"},
        {"RSS.Feed.RemoveAction", ":icons/qbt-theme/edit-delete.svg"},
        {"RSS.Feed.RenameAction", ":icons/qbt-theme/edit-rename.svg"},
        {"RSS.Feed.Subscription.AddAction", ":icons/qbt-theme/list-add.svg"},
        {"RSS.FeedUrl.CopyAction", ":icons/qbt-theme/edit-copy.svg"},
        {"RSS.MustContain.ErrorAction", ":icons/qbt-theme/task-attention.svg"},
        {"RSS.MustNotContain.ErrorAction", ":icons/qbt-theme/task-attention.svg"},
        {"RSS.NewFeed.AddAction", ":icons/qbt-theme/list-add.svg"},
        {"RSS.SelectedFeed.RemoveAction", ":icons/qbt-theme/list-remove.svg"},
        {"RSS.UnAvailable", ":icons/qbt-theme/unavailable.svg"},
        {"RSS.Unread", ":icons/qbt-theme/mail-folder-inbox.svg"},
        {"RSS.ReadArticle", ":icons/sphere.png"},
        {"RSS.UnReadArticle", ":icons/sphere2.png"},
        {"RSS.LoadingFeed", ":icons/loading.png"},
        {"RSSFeedTree", ":icons/qbt-theme/inode-directory.svg"},
        {"RSSRule.AddAction", ":icons/qbt-theme/list-add.svg"},
        {"RSSRule.RemoveAction", ":icons/qbt-theme/list-remove.svg"},
        {"RSSRule.RenameAction", ":icons/qbt-theme/edit-rename.svg"},
        {"RSSRule.Selected.RemoveAction", ":icons/qbt-theme/list-remove.svg"},
        {"Search.CopyAction", ":icons/qbt-theme/edit-copy.svg"},
        {"Search.DescriptionPageURL.CopyAction", ":icons/qbt-theme/edit-copy.svg"},
        {"Search.DownloadAction", ":icons/qbt-theme/download.svg"},
        {"Search.DownloadLink.CopyAction", ":icons/qbt-theme/edit-copy.svg"},
        {"Search.FindAction", ":icons/qbt-theme/edit-find.svg"},
        {"Search.Name.CopyAction", ":icons/qbt-theme/edit-copy.svg"},
        {"Search.OpenDescriptionPage", ":icons/qbt-theme/application-x-mswinurl.svg"},
        {"Search.Plugins", ":icons/qbt-theme/preferences-system-network.svg"},
        {"SearchStatus.Ongoing", ":icons/qbt-theme/task-ongoing.svg"},
        {"SearchStatus.Finished", ":icons/qbt-theme/task-complete.svg"},
        {"SearchStatus.Aborted", ":icons/qbt-theme/task-reject.svg"},
        {"SearchStatus.Error", ":icons/qbt-theme/task-attention.svg"},
        {"SearchStatus.NoResults", ":icons/qbt-theme/task-attention.svg"},
        {"StatusBar.ErrorConnection", ":icons/skin/firewalled.svg"},
        {"StatusBar.DisconnectedConnection", ":icons/skin/disconnected.svg"},
        {"StatusBar.ConnectedConnection", ":icons/skin/connected.svg"},
        {"StatusBar.DownloadSpeed", ":icons/skin/download.svg"},
        {"StatusBar.UploadSpeed", ":icons/skin/seeding.svg"},
        {"StatusBar.AlternateSpeed", ":icons/slow.svg"},
        {"StatusBar.RegularSpeed", ":icons/slow_off.svg"},
        {"StatusFilter.All", ":icons/skin/filterall.svg"},
        {"StatusFilter.Downloading", ":icons/skin/downloading.svg"},
        {"StatusFilter.Uploading", ":icons/skin/uploading.svg"},
        {"StatusFilter.Completed", ":icons/skin/completed.svg"},
        {"StatusFilter.Resumed", ":icons/skin/resumed.svg"},
        {"StatusFilter.Paused", ":icons/skin/paused.svg"},
        {"StatusFilter.Active", ":icons/skin/filteractive.svg"},
        {"StatusFilter.InActive", ":icons/skin/filterinactive.svg"},
        {"StatusFilter.Error", ":icons/skin/error.svg"},
        {"TagFilter.Category.RemoveAction", ":icons/qbt-theme/list-remove.svg"},
        {"TagFilter.Category.RemoveUnsedAction", ":icons/qbt-theme/list-remove.svg"},
        {"TagFilter.CategoryTorrent.PauseAction", ":icons/qbt-theme/media-playback-pause.svg"},
        {"TagFilter.CategoryTorrent.RemoveAction", ":icons/qbt-theme/edit-delete.svg"},
        {"TagFilter.CategoryTorrent.ResumeAction", ":icons/qbt-theme/media-playback-start.svg"},
        {"TagFilter.NewCategory.AddAction", ":icons/qbt-theme/list-add.svg"},
        {"TagFilter.SubCategory", ":icons/qbt-theme/inode-directory.svg"},
        {"Torrent.Paused", ":icons/skin/paused.svg"},
        {"Torrent.Queued", ":icons/skin/queued.svg"},
        {"Torrent.Downloading", ":icons/skin/downloading.svg"},
        {"Torrent.StalledDownloading", ":icons/skin/stalledDL.svg"},
        {"Torrent.Uploading", ":icons/skin/uploading.svg"},
        {"Torrent.StalledUploading", ":icons/skin/stalledUP.svg"},
        {"Torrent.Completed", ":icons/skin/completed.svg"},
        {"Torrent.Checking", ":icons/skin/checking.svg"},
        {"Torrent.Error", ":icons/skin/error.svg"},
        {"TorrentContent.ContainingFolder.OpenAction", ":icons/qbt-theme/inode-directory.svg"},
        {"TorrentContent.OpenAction", ":icons/qbt-theme/folder-documents.svg"},
        {"TorrentContent.RenameAction", ":icons/qbt-theme/edit-rename.svg"},
        {"TorrentInfo.GeneralTab", ":icons/qbt-theme/document-properties.svg"},
        {"TorrentInfo.HttpSourcesTab", ":icons/qbt-theme/network-server.svg"},
        {"TorrentInfo.PeerListTab", ":icons/qbt-theme/edit-find-user.svg"},
        {"TorrentInfo.SpeedGraphTab", ":icons/qbt-theme/office-chart-line.svg"},
        {"TorrentInfo.TorrentContentTab", ":icons/qbt-theme/inode-directory.svg"},
        {"TorrentInfo.TrackersTab", ":icons/qbt-theme/network-server.svg"},
        {"TrackerAddion.DownloadAction", ":icons/qbt-theme/download.svg"},
        {"TrackerList.AddAction", ":icons/qbt-theme/list-add.svg"},
        {"TrackerList.DownAction", ":icons/qbt-theme/go-down.svg"},
        {"TrackerList.ReAnnounce.RefreshAction", ":icons/qbt-theme/view-refresh.svg"},
        {"TrackerList.ReAnnounceAll.RefreshAction", ":icons/qbt-theme/view-refresh.svg"},
        {"TrackerList.RemoveAction", ":icons/qbt-theme/list-remove.svg"},
        {"TrackerList.URL.CopyAction", ":icons/qbt-theme/edit-copy.svg"},
        {"TrackerList.URL.EditAction", ":icons/qbt-theme/edit-rename.svg"},
        {"TrackerList.UpAction", ":icons/qbt-theme/go-up.svg"},
        {"TransferFilter.AllTrackers", ":icons/qbt-theme/network-server.svg"},
        {"TransferFilter.Tracker", ":icons/qbt-theme/network-server.svg"},
        {"TransferFilter.Tracker.PauseAction", ":icons/qbt-theme/media-playback-pause.svg"},
        {"TransferFilter.Tracker.RemoveAction", ":icons/qbt-theme/edit-delete.svg"},
        {"TransferFilter.Tracker.ResumeAction", ":icons/qbt-theme/media-playback-start.svg"},
        {"TransferFilter.WithoutTrackers", ":icons/qbt-theme/network-server.svg"},
        {"TransferList.BottomAction", ":icons/qbt-theme/go-bottom.svg"},
        {"TransferList.Category", ":icons/qbt-theme/view-categories.svg"},
        {"TransferList.Category.AddAction", ":icons/qbt-theme/list-add.svg"},
        {"TransferList.Category.ResetAction", ":icons/qbt-theme/edit-clear.svg"},
        {"TransferList.Category.SavePath", ":icons/qbt-theme/inode-directory.svg"},
        {"TransferList.CopyAction", ":icons/qbt-theme/edit-copy.svg"},
        {"TransferList.DestinationFolder.OpenAction", ":icons/qbt-theme/inode-directory.svg"},
        {"TransferList.DownAction", ":icons/qbt-theme/go-down.svg"},
        {"TransferList.DownloadLimit", ":icons/qbt-theme/kt-set-max-download-speed.svg"},
        {"TransferList.ForceReannounce", ":icons/qbt-theme/document-edit-verify.svg"},
        {"TransferList.ForceRecheck", ":icons/qbt-theme/document-edit-verify.svg"},
        {"TransferList.ForceResumeAction", ":icons/qbt-theme/media-seek-forward.svg"},
        {"TransferList.Hash.CopyAction", ":icons/qbt-theme/edit-copy.svg"},
        {"TransferList.MagnetLink.CopyAction", ":icons/qbt-theme/kt-magnet.svg"},
        {"TransferList.Name.CopyActin", ":icons/qbt-theme/edit-copy.svg"},
        {"TransferList.PauseAction", ":icons/qbt-theme/media-playback-pause.svg"},
        {"TransferList.PreviewAction", ":icons/qbt-theme/view-preview.svg"},
        {"TransferList.RemoveAction", ":icons/qbt-theme/edit-delete.svg"},
        {"TransferList.RenameAction", ":icons/qbt-theme/edit-rename.svg"},
        {"TransferList.ResumeAction", ":icons/qbt-theme/media-playback-start.svg"},
        {"TransferList.Tackers.RenameAction", ":icons/qbt-theme/edit-rename.svg"},
        {"TransferList.Tags", ":icons/qbt-theme/view-categories.svg"},
        {"TransferList.Tags.AddAction", ":icons/qbt-theme/list-add.svg"},
        {"TransferList.Tags.RemoveAllAction", ":icons/qbt-theme/edit-clear.svg"},
        {"TransferList.TopAction", ":icons/qbt-theme/go-top.svg"},
        {"TransferList.TorrentSaveLocation", ":icons/qbt-theme/inode-directory.svg"},
        {"TransferList.UpAction", ":icons/qbt-theme/go-up.svg"},
        {"TransferList.UploadLimit", ":icons/qbt-theme/kt-set-max-upload-speed.svg"},
        {"TransferList.MaxRatio", ":icons/skin/ratio.svg"},
        {"WebSeed.AddAction", ":icons/qbt-theme/list-add.svg"},
        {"WebSeed.RemoveAction", ":icons/qbt-theme/list-remove.svg"},
        {"WebSeedURL.CopyAction", ":icons/qbt-theme/edit-copy.svg"},
        {"WebSeedURL.EditAction", ":icons/qbt-theme/edit-rename.svg"},
        {"Tray.Logo", ":icons/skin/qbittorrent-tray.svg"},
        {"Tray.DarkLogo", ":icons/skin/qbittorrent-tray-dark.svg"},
        {"Tray.LightLogo", ":icons/skin/qbittorrent-tray-light.svg"},
        {"Tray.Download", ":icons/skin/download.svg"},
        {"Tray.Seeding", ":icons/skin/seeding.svg"}};


#ifdef QBT_SYSTEMICONS
    static const QHash<QString, QString> systemIconConfig = {{"AboutDialog.Logo", "qbittorrent-tray"},
        {"AddNewTorrentDialog.TorrentContentView.RenameAction", "edit-rename"},
        {"CategoryFilter.ContextMenu.AddAction", "list-add"},
        {"CategoryFilter.ContextMenu.AddSubcategoryAction", "list-add"},
        {"CategoryFilter.ContextMenu.EditAction", "document-edit"},
        {"CategoryFilter.ContextMenu.RemoveAction", "list-remove"},
        {"CategoryFilter.ContextMenu.Torrent.DeleteAction", "edit-delete"},
        {"CategoryFilter.ContextMenu.Torrent.PauseAction", "media-playback-pause"},
        {"CategoryFilter.ContextMenu.Torrent.ResumeAction", "media-playback-start"},
        {"CategoryFilter.ContextMenu.Unused.RemoveAction", "list-remove"},
        {"CategoryFilter.SubCategory", "inode-directory"},
        {"CookiesDialog", "preferences-web-browser-cookies"},
        {"CookiesDialog.AddAction", "list-add"},
        {"CookiesDialog.RemoveAction", "list-remove"},
        {"DownloadedEpisodes.RemoveAction", "edit-clear"},
        {"ExecutionLog.BlockedIPsTab", "view-filter"},
        {"ExecutionLog.GeneralTab", "view-calendar-journal"},
        {"FileIconProvider.Cache", "text-plain"},
        {"LineEdit.FindAction", "edit-find"},
        {"LogList.ContextMenu.ClearAction", "edit-clear"},
        {"LogList.ContextMenu.CopyAction", "edit-copy"},
        {"MainMenu.ConfigureAction", "configure"},
        {"MainMenu.Documentation.HelpAction", "help-contents"},
        {"MainMenu.DonateAction", "wallet-open"}, {"MainMenu.DownAction", "go-up"},
        {"MainMenu.DownloadLimit", "kt-set-max-download-speed"},
        {"MainMenu.ExecutionLogTab", "view-calendar-journal"},
        {"MainMenu.ExitAction", "application-exit"},
        {"MainMenu.GlobalDownloadLimit", "kt-set-max-download-speed"},
        {"MainMenu.GlobalUploadLimit", "kt-set-max-upload-speed"},
        {"MainMenu.HelpAction", "help-about"},
        {"MainMenu.LockAction", "object-locked"},
        {"MainMenu.ManageCookies", "preferences-web-browser-cookies"},
        {"MainMenu.PauseAction", "media-playback-pause"},
        {"MainMenu.PauseAllAction", "media-playback-pause"},
        {"MainMenu.Queue.BottomAction", "go-bottom"},
        {"MainMenu.Queue.DownAction", "go-down"},
        {"MainMenu.RSSTab", "application-rss+xml"},
        {"MainMenu.SearchTab", "edit-find"},
        {"MainMenu.Shutdown.ExitAction", "application-exit"},
        {"MainMenu.StartAction", "media-playback-start"},
        {"MainMenu.StartAllAction", "media-playback-start"},
        {"MainMenu.Statistics", "view-statistics"},
        {"MainMenu.Torrent.CreateAction", "document-edit"},
        {"MainMenu.Torrent.RemoveAction", "list-remove"},
        {"MainMenu.TorrentFile.OpenAction", "list-add"},
        {"MainMenu.TorrentURL.OpenAction", "insert-link"},
        {"MainMenu.TransfersTab", "folder-remote"}, {"MainMenu.UpAction", "go-top"},
        {"MainMenu.UploadLimit", "kt-set-max-upload-speed"},
        {"MainWindow.Logo", "qbittorrent-tray"},
        {"OptionsDialog.AdvancedTab", "preferences-other"},
        {"OptionsDialog.AlternativeRate", "slow"},
        {"OptionsDialog.BitTorrentTab", "preferences-system-network"},
        {"OptionsDialog.ConnectionTab", "network-wired"},
        {"OptionsDialog.ConnectionTab.IPFilter.RefreshAction", "view-refresh"},
        {"OptionsDialog.DownloadTab", "folder-download"},
        {"OptionsDialog.GlobalRate", "slow_off"},
        {"OptionsDialog.RSSTab", "rss-config"},
        {"OptionsDialog.SSL.HighSecurity", "security-high"},
        {"OptionsDialog.SSL.LowSecurity", "security-low"},
        {"OptionsDialog.SpeedTab", "speedometer"},
        {"OptionsDialog.UITab", "preferences-desktop"},
        {"OptionsDialog.WebUITab", "network-server"},
        {"PeerList.ContextMenu.AddAction", "user-group-new"},
        {"PeerList.ContextMenu.BanAction", "user-group-delete"},
        {"PeerList.ContextMenu.CopyAction", "edit-copy"},
        {"RSS", "inode-directory"}, {"RSS.Available", "application-rss+xml"},
        {"RSS.EpisodeFilter.ErrorAction", "task-attention"},
        {"RSS.Feed.DownloadAction", "download"},
        {"RSS.Feed.MarkAllRead", "mail-mark-read"},
        {"RSS.Feed.MarkReadAction", "mail-mark-read"},
        {"RSS.Feed.NewFolderAction", "folder-new"},
        {"RSS.Feed.NewsURL.OpenAction", "application-x-mswinurl"},
        {"RSS.Feed.RefreshAction", "view-refresh"},
        {"RSS.Feed.RefreshAllAction", "view-refresh"},
        {"RSS.Feed.RemoveAction", "edit-delete"},
        {"RSS.Feed.RenameAction", "edit-rename"},
        {"RSS.Feed.Subscription.AddAction", "list-add"},
        {"RSS.FeedUrl.CopyAction", "edit-copy"}, {"RSS.LoadingFeed", "loading"},
        {"RSS.MustContain.ErrorAction", "task-attention"},
        {"RSS.MustNotContain.ErrorAction", "task-attention"},
        {"RSS.NewFeed.AddAction", "list-add"}, {"RSS.ReadArticle", "sphere"},
        {"RSS.SelectedFeed.RemoveAction", "list-remove"},
        {"RSS.UnAvailable", "unavailable"}, {"RSS.UnReadArticle", "sphere2"},
        {"RSS.Unread", "mail-folder-inbox"}, {"RSSFeedTree", "inode-directory"},
        {"RSSRule.AddAction", "list-add"}, {"RSSRule.RemoveAction", "list-remove"},
        {"RSSRule.RenameAction", "edit-rename"},
        {"RSSRule.Selected.RemoveAction", "list-remove"},
        {"Search.CopyAction", "edit-copy"},
        {"Search.DescriptionPageURL.CopyAction", "edit-copy"},
        {"Search.DownloadAction", "download"},
        {"Search.DownloadLink.CopyAction", "edit-copy"},
        {"Search.FindAction", "edit-find"}, {"Search.Name.CopyAction", "edit-copy"},
        {"Search.OpenDescriptionPage", "application-x-mswinurl"},
        {"Search.Plugins", "preferences-system-network"},
        {"StatusBar.AlternateSpeed", "slow"},
        {"StatusBar.ConnectedConnection", "connected"},
        {"StatusBar.DisconnectedConnection", "disconnected"},
        {"StatusBar.DownloadSpeed", "download"},
        {"StatusBar.ErrorConnection", "firewalled"},
        {"StatusBar.RegularSpeed", "slow_off"},
        {"StatusBar.UploadSpeed", "seeding"},
        {"StatusFilter.Active", "filteractive"}, {"StatusFilter.All", "filterall"},
        {"StatusFilter.Completed", "completed"},
        {"StatusFilter.Downloading", "downloading"},
        {"StatusFilter.Error", "error"},
        {"StatusFilter.InActive", "filterinactive"},
        {"StatusFilter.Paused", "paused"}, {"StatusFilter.Resumed", "resumed"},
        {"StatusFilter.Uploading", "uploading"},
        {"TagFilter.Category.RemoveAction", "list-remove"},
        {"TagFilter.Category.RemoveUnsedAction", "list-remove"},
        {"TagFilter.CategoryTorrent.PauseAction", "media-playback-pause"},
        {"TagFilter.CategoryTorrent.RemoveAction", "edit-delete"},
        {"TagFilter.CategoryTorrent.ResumeAction", "media-playback-start"},
        {"TagFilter.NewCategory.AddAction", "list-add"},
        {"TagFilter.SubCategory", "inode-directory"},
        {"Torrent.Checking", "checking"}, {"Torrent.Completed", "completed"},
        {"Torrent.Downloading", "downloading"}, {"Torrent.Error", "error"},
        {"Torrent.Paused", "paused"}, {"Torrent.Queued", "queued"},
        {"Torrent.StalledDownloading", "stalledDL"},
        {"Torrent.StalledUploading", "stalledUP"},
        {"Torrent.Uploading", "uploading"},
        {"TorrentContent.ContainingFolder.OpenAction", "inode-directory"},
        {"TorrentContent.OpenAction", "folder-documents"},
        {"TorrentContent.RenameAction", "edit-rename"},
        {"TorrentInfo.GeneralTab", "document-properties"},
        {"TorrentInfo.HttpSourcesTab", "network-server"},
        {"TorrentInfo.PeerListTab", "edit-find-user"},
        {"TorrentInfo.SpeedGraphTab", "office-chart-line"},
        {"TorrentInfo.TorrentContentTab", "inode-directory"},
        {"TorrentInfo.TrackersTab", "network-server"},
        {"TrackerAddion.DownloadAction", "download"},
        {"TrackerList.AddAction", "list-add"},
        {"TrackerList.DownAction", "go-down"},
        {"TrackerList.ReAnnounce.RefreshAction", "view-refresh"},
        {"TrackerList.ReAnnounceAll.RefreshAction", "view-refresh"},
        {"TrackerList.RemoveAction", "list-remove"},
        {"TrackerList.URL.CopyAction", "edit-copy"},
        {"TrackerList.URL.EditAction", "edit-rename"},
        {"TrackerList.UpAction", "go-up"},
        {"TransferFilter.AllTrackers", "network-server"},
        {"TransferFilter.Tracker", "network-server"},
        {"TransferFilter.Tracker.PauseAction", "media-playback-pause"},
        {"TransferFilter.Tracker.RemoveAction", "edit-delete"},
        {"TransferFilter.Tracker.ResumeAction", "media-playback-start"},
        {"TransferFilter.WithoutTrackers", "network-server"},
        {"TransferList.BottomAction", "go-bottom"},
        {"TransferList.Category", "view-categories"},
        {"TransferList.Category.AddAction", "list-add"},
        {"TransferList.Category.ResetAction", "edit-clear"},
        {"TransferList.Category.SavePath", "inode-directory"},
        {"TransferList.CopyAction", "edit-copy"},
        {"TransferList.DestinationFolder.OpenAction", "inode-directory"},
        {"TransferList.DownAction", "go-down"},
        {"TransferList.DownloadLimit", "kt-set-max-download-speed"},
        {"TransferList.ForceReannounce", "document-edit-verify"},
        {"TransferList.ForceRecheck", "document-edit-verify"},
        {"TransferList.ForceResumeAction", "media-seek-forward"},
        {"TransferList.Hash.CopyAction", "edit-copy"},
        {"TransferList.MagnetLink.CopyAction", "kt-magnet"},
        {"TransferList.MaxRatio", "ratio"},
        {"TransferList.Name.CopyActin", "edit-copy"},
        {"TransferList.PauseAction", "media-playback-pause"},
        {"TransferList.PreviewAction", "view-preview"},
        {"TransferList.RemoveAction", "edit-delete"},
        {"TransferList.RenameAction", "edit-rename"},
        {"TransferList.ResumeAction", "media-playback-start"},
        {"TransferList.Tackers.RenameAction", "edit-rename"},
        {"TransferList.Tags", "view-categories"},
        {"TransferList.Tags.AddAction", "list-add"},
        {"TransferList.Tags.RemoveAllAction", "edit-clear"},
        {"TransferList.TopAction", "go-top"},
        {"TransferList.TorrentSaveLocation", "inode-directory"},
        {"TransferList.UpAction", "go-up"},
        {"TransferList.UploadLimit", "kt-set-max-upload-speed"},
        {"Tray.DarkLogo", "qbittorrent-tray-dark"}, {"Tray.Download", "download"},
        {"Tray.LightLogo", "qbittorrent-tray-light"},
        {"Tray.Logo", "qbittorrent-tray"}, {"Tray.Seeding", "seeding"},
        {"WebSeed.AddAction", "list-add"}, {"WebSeed.RemoveAction", "list-remove"},
        {"WebSeedURL.CopyAction", "edit-copy"},
        {"WebSeedURL.EditAction", "edit-rename"} };
#endif
}

UIThemeManager *UIThemeManager::m_instance = nullptr;

namespace
{
    class ThemeError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
    };

    /*
        Loads Json Icon Config file from `configFile`, and transforms it into a map

        function throws ThemeError if encountered with any error
     */
    QHash<QString, QString> loadIconConfig(const QString &configFile)
    {
        // load icon config from file
        QFile iconJsonMap(configFile);
        if (!iconJsonMap.open(QIODevice::ReadOnly))
            throw ThemeError(QObject::tr("Failed to open \"%1\", error: %2.")
                             .arg(iconJsonMap.fileName(), iconJsonMap.errorString()));

        QJsonObject jsonObject;
        QJsonParseError err;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(iconJsonMap.readAll(), &err);
        if (err.error != QJsonParseError::NoError)
            throw ThemeError(QObject::tr("Error occurred while parsing \"%1\", error: %2.")
                             .arg(iconJsonMap.fileName(), err.errorString()));
        if (!jsonDoc.isObject())
            throw ThemeError(QObject::tr("Invalid icon configuration file format in \"%1\". JSON object is expected.")
                             .arg(iconJsonMap.fileName()));
        jsonObject = jsonDoc.object();

        QHash<QString, QString> config; // resultant icon-config
        config.reserve(jsonObject.size());

        // copy jsonObject to config, also check for errors
        for (auto i = jsonObject.begin(), e = jsonObject.end(); i != e; ++i) {
            if (!i.value().isString())
                throw ThemeError(QObject::tr("Error in iconconfig \"%1\", error: Provided value for %2, is not a string.")
                                 .arg(configFile, i.key()));

            QString value = i.value().toString();
            if (value.isEmpty())
                throw ThemeError(QObject::tr("Error in iconconfig \"%1\", error: Provided value for %2, is empty string")
                                 .arg(configFile, i.key()));

            config.insert(i.key(), value);
        }
        return config;
    }

    QString resolveIconId(const QString &iconId, const QHash<QString, QString> &iconMap)
    {
        QString iconPath = iconMap.value(iconId);
        QString iconIdPattern = iconId;
        while (iconPath.isEmpty() && !iconIdPattern.isEmpty()) {
            const int sepIndex = iconIdPattern.indexOf('.');
            iconIdPattern = "*" + ((sepIndex == -1) ? "" : iconIdPattern.right(iconIdPattern.size() - sepIndex));
            const auto patternIter = iconMap.find(iconIdPattern);
            if (patternIter != iconMap.end())
                iconPath = *patternIter;
            else
                iconIdPattern.remove(0, 2); // removes "*."
        }

        return iconPath;
    }
}

void UIThemeManager::freeInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = nullptr;
    }
}

void UIThemeManager::initInstance()
{
    if (!m_instance)
        m_instance = new UIThemeManager;
}

UIThemeManager::UIThemeManager() : m_useCustomStylesheet(false)
{
    const Preferences *const pref = Preferences::instance();
    bool useCustomUITheme = pref->useCustomUITheme();

#ifdef QBT_SYSTEMICONS
    m_useSystemTheme = pref->useSystemIconTheme();
    m_systemIconMap = systemIconConfig;
#endif

    QHash<QString, QString> customIconMap;

    if (useCustomUITheme) {
        try {
            if (!QResource::registerResource(pref->customUIThemePath(), "/uitheme"))
                throw ThemeError(tr("Failed to load UI theme from file: \"%1\".")
                                 .arg(pref->customUIThemePath()));
            m_useCustomStylesheet = QFile::exists(":uitheme/stylesheet.qss");

            customIconMap = loadIconConfig(":uitheme/icons/iconconfig.json");
            const QString customIconDir{":uitheme/icons/"};
            for (auto i = customIconMap.begin(); i != customIconMap.end(); i++) {
                i.value() = customIconDir + i.value();
                if (!QFile::exists(i.value()))
                    throw ThemeError(tr("Error in iconconfig \"%1\", error: icon \"%2\" doesn't exists")
                                     .arg(":uitheme/icons/iconconfig.json", i.value()));
            }

#ifdef QBT_SYSTEMICONS
            if (m_useSystemTheme)
                m_systemIconMap = loadIconConfig(":uitheme/icons/systemiconconfig.json");
#endif
        } catch (const ThemeError &err) {
            LogMsg(err.message(), Log::WARNING);
            useCustomUITheme = false;

            customIconMap.clear(); // reset icon map, so that erroed state don't interfere with merging
        }
    }

    // merge defaultIconMap and customIconMap into m_iconMap
    m_iconMap = iconConfig;
    for (auto i = customIconMap.constBegin(), e = customIconMap.constEnd(); i != e; i++)
        m_iconMap.insert(i.key(), i.value()); // overwrite existing values or insert
}

UIThemeManager *UIThemeManager::instance()
{
    return m_instance;
}

void UIThemeManager::applyStyleSheet() const
{
    if (!m_useCustomStylesheet) {
        qApp->setStyleSheet({});
        return;
    }

    QFile qssFile(":uitheme/stylesheet.qss");
    if (!qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qApp->setStyleSheet({});
        LogMsg(tr("Couldn't apply theme stylesheet. stylesheet.qss couldn't be opened. Reason: %1").arg(qssFile.errorString())
               , Log::WARNING);
        return;
    }

    qApp->setStyleSheet(qssFile.readAll());
}

QIcon UIThemeManager::getIcon(const QString &iconId) const
{
    return getIcon(iconId, iconId);
}

QIcon UIThemeManager::getIcon(const QString &iconId, const QString &fallbackSysThemeIcon) const
{
#ifdef QBT_SYSTEMICONS
    if (m_useSystemTheme) {
        const QString themeIcon = resolveIconId(iconId, m_systemIconMap);

        // QIcon::fromTheme may return valid icons for not registerd system id as well,
        // for ex: edit-find-user
        QIcon icon = QIcon::fromTheme(themeIcon);
        if (icon.isNull() && (fallbackSysThemeIcon != iconId))
            icon = QIcon::fromTheme(fallbackSysThemeIcon);
        if (!icon.isNull())
            return icon;
        LogMsg(tr("Can't find system icon %1 nor %2").arg(themeIcon, fallbackSysThemeIcon), Log::WARNING);
    }
#else
    Q_UNUSED(fallbackSysThemeIcon)
#endif

    // cache to avoid rescaling svg icons
    static QHash<QString, QIcon> iconCache;
    const QString iconPath = getIconPath(iconId);
    if (iconPath.isEmpty()) // error already reported by getIconPath
        return {};

    const auto iter = iconCache.find(iconPath);
    if (iter != iconCache.end())
        return *iter;

    const QIcon icon {iconPath};
    iconCache[iconPath] = icon;
    return icon;
}

QIcon UIThemeManager::getFlagIcon(const QString &countryIsoCode) const
{
    if (countryIsoCode.isEmpty()) return {};

    // cache to avoid rescaling svg icons
    static QHash<QString, QIcon> flagCache;
    const QString iconPath = ":icons/flags/" + countryIsoCode.toLower() + ".svg";
    if (!QFile::exists(iconPath)) {
        LogMsg(tr("No flag icon for country code - %1").arg(countryIsoCode), Log::WARNING);
        return {};
    }

    const auto iter = flagCache.find(iconPath);
    if (iter != flagCache.end())
        return *iter;

    const QIcon icon {iconPath};
    flagCache[iconPath] = icon;
    return icon;
}

QPixmap UIThemeManager::getScaledPixmap(const QString &iconId, const QWidget *widget, int baseHeight) const
{
    return Utils::Gui::scaledPixmapSvg(getIconPath(iconId), widget, baseHeight);
}

QString UIThemeManager::getIconPath(const QString &iconId) const
{
    QString iconPath = resolveIconId(iconId, m_iconMap);

    if (iconPath.isEmpty()) {
        LogMsg(tr("Can't resolve icon id - %1").arg(iconId), Log::WARNING);
        return {};
    }

    return iconPath;
}
