/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022-2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2012  Christophe Dumez <chris@qbittorrent.org>
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

#include "addnewtorrentdialog.h"

#include <algorithm>
#include <functional>

#include <QAction>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QSize>
#include <QString>
#include <QUrl>
#include <QVector>

#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/magneturi.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentcontenthandler.h"
#include "base/bittorrent/torrentcontentlayout.h"
#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/settingsstorage.h"
#include "base/torrentfileguard.h"
#include "base/utils/compare.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "lineedit.h"
#include "raisedmessagebox.h"
#include "torrenttagsdialog.h"
#include "ui_addnewtorrentdialog.h"
#include "uithememanager.h"

namespace
{
#define SETTINGS_KEY(name) u"AddNewTorrentDialog/" name
    const QString KEY_ENABLED = SETTINGS_KEY(u"Enabled"_s);
    const QString KEY_TOPLEVEL = SETTINGS_KEY(u"TopLevel"_s);
    const QString KEY_ATTACHED = SETTINGS_KEY(u"Attached"_s);
    const QString KEY_SAVEPATHHISTORY = SETTINGS_KEY(u"SavePathHistory"_s);
    const QString KEY_DOWNLOADPATHHISTORY = SETTINGS_KEY(u"DownloadPathHistory"_s);
    const QString KEY_SAVEPATHHISTORYLENGTH = SETTINGS_KEY(u"SavePathHistoryLength"_s);

    // just a shortcut
    inline SettingsStorage *settings()
    {
        return SettingsStorage::instance();
    }

    // savePath is a folder, not an absolute file path
    int indexOfPath(const FileSystemPathComboEdit *fsPathEdit, const Path &savePath)
    {
        for (int i = 0; i < fsPathEdit->count(); ++i)
        {
            if (fsPathEdit->item(i) == savePath)
                return i;
        }
        return -1;
    }

    qint64 queryFreeDiskSpace(const Path &path)
    {
        const Path root = path.rootItem();
        Path current = path;
        qint64 freeSpace = Utils::Fs::freeDiskSpaceOnPath(current);

        // for non-existent directories (which will be created on demand) `Utils::Fs::freeDiskSpaceOnPath`
        // will return invalid value so instead query its parent/ancestor paths
        while ((freeSpace < 0) && (current != root))
        {
            current = current.parentPath();
            freeSpace = Utils::Fs::freeDiskSpaceOnPath(current);
        }
        return freeSpace;
    }

    void setPath(FileSystemPathComboEdit *fsPathEdit, const Path &newPath)
    {
        int existingIndex = indexOfPath(fsPathEdit, newPath);
        if (existingIndex < 0)
        {
            // New path, prepend to combo box
            fsPathEdit->insertItem(0, newPath);
            existingIndex = 0;
        }

        fsPathEdit->setCurrentIndex(existingIndex);
    }

    void updatePathHistory(const QString &settingsKey, const Path &path, const int maxLength)
    {
        // Add last used save path to the front of history

        auto pathList = settings()->loadValue<QStringList>(settingsKey);

        const int selectedSavePathIndex = pathList.indexOf(path.toString());
        if (selectedSavePathIndex > -1)
            pathList.move(selectedSavePathIndex, 0);
        else
            pathList.prepend(path.toString());

        settings()->storeValue(settingsKey, QStringList(pathList.mid(0, maxLength)));
    }

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

        delta = screenGeometry.topLeft() - dialogGeometry.topLeft();
        if (delta.x() < 0)
            delta.setX(0);
        if (delta.y() < 0)
            delta.setY(0);
        dialogGeometry.translate(delta);

        dialog->setGeometry(dialogGeometry);
    }
}

class AddNewTorrentDialog::TorrentContentAdaptor final
        : public BitTorrent::TorrentContentHandler
{
public:
    TorrentContentAdaptor(BitTorrent::TorrentInfo &torrentInfo, PathList &filePaths
            , QVector<BitTorrent::DownloadPriority> &filePriorities, std::function<void ()> onFilePrioritiesChanged)
        : m_torrentInfo {torrentInfo}
        , m_filePaths {filePaths}
        , m_filePriorities {filePriorities}
        , m_onFilePrioritiesChanged {std::move(onFilePrioritiesChanged)}
    {
        Q_ASSERT(filePaths.isEmpty() || (filePaths.size() == torrentInfo.filesCount()));

        m_originalRootFolder = Path::findRootFolder(m_torrentInfo.filePaths());
        m_currentContentLayout = (m_originalRootFolder.isEmpty()
                                  ? BitTorrent::TorrentContentLayout::NoSubfolder
                                  : BitTorrent::TorrentContentLayout::Subfolder);

        if (!m_filePriorities.isEmpty())
        {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
            const int currentSize = m_filePriorities.size();
            m_filePriorities.resize(filesCount());
            for (int i = currentSize; i < filesCount(); ++i)
                m_filePriorities[i] = BitTorrent::DownloadPriority::Normal;
#else
            m_filePriorities.resize(filesCount(), BitTorrent::DownloadPriority::Normal);
#endif
        }
    }

    bool hasMetadata() const override
    {
        return m_torrentInfo.isValid();
    }

    int filesCount() const override
    {
        return m_torrentInfo.filesCount();
    }

    qlonglong fileSize(const int index) const override
    {
        Q_ASSERT((index >= 0) && (index < filesCount()));
        return m_torrentInfo.fileSize(index);
    }

    Path filePath(const int index) const override
    {
        Q_ASSERT((index >= 0) && (index < filesCount()));
        return (m_filePaths.isEmpty() ? m_torrentInfo.filePath(index) : m_filePaths.at(index));
    }

    void renameFile(const int index, const Path &newFilePath) override
    {
        Q_ASSERT((index >= 0) && (index < filesCount()));
        const Path currentFilePath = filePath(index);
        if (currentFilePath == newFilePath)
            return;

        if (m_filePaths.isEmpty())
            m_filePaths = m_torrentInfo.filePaths();

        m_filePaths[index] = newFilePath;
    }

    void applyContentLayout(const BitTorrent::TorrentContentLayout contentLayout)
    {
        Q_ASSERT(hasMetadata());
        Q_ASSERT(!m_filePaths.isEmpty());

        const auto originalContentLayout = (m_originalRootFolder.isEmpty()
                                            ? BitTorrent::TorrentContentLayout::NoSubfolder
                                            : BitTorrent::TorrentContentLayout::Subfolder);
        const auto newContentLayout = ((contentLayout == BitTorrent::TorrentContentLayout::Original)
                                       ? originalContentLayout : contentLayout);
        if (newContentLayout != m_currentContentLayout)
        {
            if (newContentLayout == BitTorrent::TorrentContentLayout::NoSubfolder)
            {
                Path::stripRootFolder(m_filePaths);
            }
            else
            {
                const auto rootFolder = ((originalContentLayout == BitTorrent::TorrentContentLayout::Subfolder)
                                         ? m_originalRootFolder
                                         : m_filePaths.at(0).removedExtension());
                Path::addRootFolder(m_filePaths, rootFolder);
            }

            m_currentContentLayout = newContentLayout;
        }
    }

    QVector<BitTorrent::DownloadPriority> filePriorities() const override
    {
        return m_filePriorities.isEmpty()
                ? QVector<BitTorrent::DownloadPriority>(filesCount(), BitTorrent::DownloadPriority::Normal)
                : m_filePriorities;
    }

    QVector<qreal> filesProgress() const override
    {
        return QVector<qreal>(filesCount(), 0);
    }

    QVector<qreal> availableFileFractions() const override
    {
        return QVector<qreal>(filesCount(), 0);
    }

    void fetchAvailableFileFractions(std::function<void (QVector<qreal>)> resultHandler) const override
    {
        resultHandler(availableFileFractions());
    }

    void prioritizeFiles(const QVector<BitTorrent::DownloadPriority> &priorities) override
    {
        Q_ASSERT(priorities.size() == filesCount());
        m_filePriorities = priorities;
        if (m_onFilePrioritiesChanged)
            m_onFilePrioritiesChanged();
    }

    Path actualStorageLocation() const override
    {
        return {};
    }

    Path actualFilePath([[maybe_unused]] int fileIndex) const override
    {
        return {};
    }

    void flushCache() const override
    {
    }

private:
    BitTorrent::TorrentInfo &m_torrentInfo;
    PathList &m_filePaths;
    QVector<BitTorrent::DownloadPriority> &m_filePriorities;
    std::function<void ()> m_onFilePrioritiesChanged;
    Path m_originalRootFolder;
    BitTorrent::TorrentContentLayout m_currentContentLayout;
};

const int AddNewTorrentDialog::minPathHistoryLength;
const int AddNewTorrentDialog::maxPathHistoryLength;

AddNewTorrentDialog::AddNewTorrentDialog(const BitTorrent::AddTorrentParams &inParams, QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::AddNewTorrentDialog)
    , m_filterLine(new LineEdit(this))
    , m_torrentParams(inParams)
    , m_storeDialogSize(SETTINGS_KEY(u"DialogSize"_s))
    , m_storeDefaultCategory(SETTINGS_KEY(u"DefaultCategory"_s))
    , m_storeRememberLastSavePath(SETTINGS_KEY(u"RememberLastSavePath"_s))
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    , m_storeTreeHeaderState(u"GUI/Qt6/" SETTINGS_KEY(u"TreeHeaderState"_s))
    , m_storeSplitterState(u"GUI/Qt6/" SETTINGS_KEY(u"SplitterState"_s))
#else
    , m_storeTreeHeaderState(SETTINGS_KEY(u"TreeHeaderState"_s))
    , m_storeSplitterState(SETTINGS_KEY(u"SplitterState"_s))
#endif
{
    // TODO: set dialog file properties using m_torrentParams.filePriorities
    m_ui->setupUi(this);

    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_ui->lblMetaLoading->setVisible(false);
    m_ui->progMetaLoading->setVisible(false);
    m_ui->buttonSave->setVisible(false);
    connect(m_ui->buttonSave, &QPushButton::clicked, this, &AddNewTorrentDialog::saveTorrentFile);

    m_ui->savePath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->savePath->setDialogCaption(tr("Choose save path"));
    m_ui->savePath->setMaxVisibleItems(20);

    const auto *session = BitTorrent::Session::instance();

    m_ui->downloadPath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->downloadPath->setDialogCaption(tr("Choose save path"));
    m_ui->downloadPath->setMaxVisibleItems(20);

    m_ui->addToQueueTopCheckBox->setChecked(m_torrentParams.addToQueueTop.value_or(session->isAddTorrentToQueueTop()));

    m_ui->stopConditionComboBox->setToolTip(
                u"<html><body><p><b>" + tr("None") + u"</b> - " + tr("No stop condition is set.") + u"</p><p><b>" +
                tr("Metadata received") + u"</b> - " + tr("Torrent will stop after metadata is received.") +
                u" <em>" + tr("Torrents that have metadata initially will be added as stopped.") + u"</em></p><p><b>" +
                tr("Files checked") + u"</b> - " + tr("Torrent will stop after files are initially checked.") +
                u" <em>" + tr("This will also download metadata if it wasn't there initially.") + u"</em></p></body></html>");
    m_ui->stopConditionComboBox->addItem(tr("None"), QVariant::fromValue(BitTorrent::Torrent::StopCondition::None));
    if (!hasMetadata())
        m_ui->stopConditionComboBox->addItem(tr("Metadata received"), QVariant::fromValue(BitTorrent::Torrent::StopCondition::MetadataReceived));
    m_ui->stopConditionComboBox->addItem(tr("Files checked"), QVariant::fromValue(BitTorrent::Torrent::StopCondition::FilesChecked));
    const auto stopCondition = m_torrentParams.stopCondition.value_or(session->torrentStopCondition());
    if (hasMetadata() && (stopCondition == BitTorrent::Torrent::StopCondition::MetadataReceived))
    {
        m_ui->startTorrentCheckBox->setChecked(false);
        m_ui->stopConditionComboBox->setCurrentIndex(m_ui->stopConditionComboBox->findData(QVariant::fromValue(BitTorrent::Torrent::StopCondition::None)));
    }
    else
    {
        m_ui->startTorrentCheckBox->setChecked(!m_torrentParams.addPaused.value_or(session->isAddTorrentPaused()));
        m_ui->stopConditionComboBox->setCurrentIndex(m_ui->stopConditionComboBox->findData(QVariant::fromValue(stopCondition)));
    }
    m_ui->stopConditionLabel->setEnabled(m_ui->startTorrentCheckBox->isChecked());
    m_ui->stopConditionComboBox->setEnabled(m_ui->startTorrentCheckBox->isChecked());
    connect(m_ui->startTorrentCheckBox, &QCheckBox::toggled, this, [this](const bool checked)
    {
        m_ui->stopConditionLabel->setEnabled(checked);
        m_ui->stopConditionComboBox->setEnabled(checked);
    });

    m_ui->comboTTM->blockSignals(true); // the TreeView size isn't correct if the slot does its job at this point
    m_ui->comboTTM->setCurrentIndex(session->isAutoTMMDisabledByDefault() ? 0 : 1);
    m_ui->comboTTM->blockSignals(false);
    connect(m_ui->comboTTM, &QComboBox::currentIndexChanged, this, &AddNewTorrentDialog::TMMChanged);

    connect(m_ui->savePath, &FileSystemPathEdit::selectedPathChanged, this, &AddNewTorrentDialog::onSavePathChanged);
    connect(m_ui->downloadPath, &FileSystemPathEdit::selectedPathChanged, this, &AddNewTorrentDialog::onDownloadPathChanged);
    connect(m_ui->groupBoxDownloadPath, &QGroupBox::toggled, this, &AddNewTorrentDialog::onUseDownloadPathChanged);

    m_ui->checkBoxRememberLastSavePath->setChecked(m_storeRememberLastSavePath);

    m_ui->contentLayoutComboBox->setCurrentIndex(
            static_cast<int>(m_torrentParams.contentLayout.value_or(session->torrentContentLayout())));
    connect(m_ui->contentLayoutComboBox, &QComboBox::currentIndexChanged, this, &AddNewTorrentDialog::contentLayoutChanged);

    m_ui->sequentialCheckBox->setChecked(m_torrentParams.sequential);
    m_ui->firstLastCheckBox->setChecked(m_torrentParams.firstLastPiecePriority);

    m_ui->skipCheckingCheckBox->setChecked(m_torrentParams.skipChecking);
    m_ui->doNotDeleteTorrentCheckBox->setVisible(TorrentFileGuard::autoDeleteMode() != TorrentFileGuard::Never);

    // Load categories
    QStringList categories = session->categories();
    std::sort(categories.begin(), categories.end(), Utils::Compare::NaturalLessThan<Qt::CaseInsensitive>());
    const QString defaultCategory = m_storeDefaultCategory;

    if (!m_torrentParams.category.isEmpty())
        m_ui->categoryComboBox->addItem(m_torrentParams.category);
    if (!defaultCategory.isEmpty())
        m_ui->categoryComboBox->addItem(defaultCategory);
    m_ui->categoryComboBox->addItem(u""_s);

    for (const QString &category : asConst(categories))
    {
        if ((category != defaultCategory) && (category != m_torrentParams.category))
            m_ui->categoryComboBox->addItem(category);
    }

    connect(m_ui->categoryComboBox, &QComboBox::currentIndexChanged, this, &AddNewTorrentDialog::categoryChanged);

    m_ui->tagsLineEdit->setText(m_torrentParams.tags.join(u", "_s));
    connect(m_ui->tagsEditButton, &QAbstractButton::clicked, this, [this]
    {
        auto *dlg = new TorrentTagsDialog(m_torrentParams.tags, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &TorrentTagsDialog::accepted, this, [this, dlg]
        {
            m_torrentParams.tags = dlg->tags();
            m_ui->tagsLineEdit->setText(m_torrentParams.tags.join(u", "_s));
        });
        dlg->open();
    });

    // Torrent content filtering
    m_filterLine->setPlaceholderText(tr("Filter files..."));
    m_filterLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_filterLine, &LineEdit::textChanged, m_ui->contentTreeView, &TorrentContentWidget::setFilterPattern);
    m_ui->contentFilterLayout->insertWidget(3, m_filterLine);

    loadState();

    connect(m_ui->doNotDeleteTorrentCheckBox, &QCheckBox::clicked, this, &AddNewTorrentDialog::doNotDeleteTorrentClicked);

    connect(m_ui->buttonSelectAll, &QPushButton::clicked, m_ui->contentTreeView, &TorrentContentWidget::checkAll);
    connect(m_ui->buttonSelectNone, &QPushButton::clicked, m_ui->contentTreeView, &TorrentContentWidget::checkNone);

    if (const QByteArray state = m_storeTreeHeaderState; !state.isEmpty())
        m_ui->contentTreeView->header()->restoreState(state);
    // Hide useless columns after loading the header state
    m_ui->contentTreeView->hideColumn(TorrentContentWidget::Progress);
    m_ui->contentTreeView->hideColumn(TorrentContentWidget::Remaining);
    m_ui->contentTreeView->hideColumn(TorrentContentWidget::Availability);
    m_ui->contentTreeView->setColumnsVisibilityMode(TorrentContentWidget::ColumnsVisibilityMode::Locked);
    m_ui->contentTreeView->setDoubleClickAction(TorrentContentWidget::DoubleClickAction::Rename);

    m_ui->labelCommentData->setText(tr("Not Available", "This comment is unavailable"));
    m_ui->labelDateData->setText(tr("Not Available", "This date is unavailable"));

    m_filterLine->blockSignals(true);

    // Default focus
    if (m_ui->comboTTM->currentIndex() == 0) // 0 is Manual mode
        m_ui->savePath->setFocus();
    else
        m_ui->categoryComboBox->setFocus();
}

AddNewTorrentDialog::~AddNewTorrentDialog()
{
    saveState();
    delete m_ui;
}

bool AddNewTorrentDialog::isEnabled()
{
    return settings()->loadValue(KEY_ENABLED, true);
}

void AddNewTorrentDialog::setEnabled(const bool value)
{
    settings()->storeValue(KEY_ENABLED, value);
}

bool AddNewTorrentDialog::isTopLevel()
{
    return settings()->loadValue(KEY_TOPLEVEL, true);
}

void AddNewTorrentDialog::setTopLevel(const bool value)
{
    settings()->storeValue(KEY_TOPLEVEL, value);
}

int AddNewTorrentDialog::savePathHistoryLength()
{
    const int defaultHistoryLength = 8;
    const int value = settings()->loadValue(KEY_SAVEPATHHISTORYLENGTH, defaultHistoryLength);
    return std::clamp(value, minPathHistoryLength, maxPathHistoryLength);
}

void AddNewTorrentDialog::setSavePathHistoryLength(const int value)
{
    const int clampedValue = qBound(minPathHistoryLength, value, maxPathHistoryLength);
    const int oldValue = savePathHistoryLength();
    if (clampedValue == oldValue)
        return;

    settings()->storeValue(KEY_SAVEPATHHISTORYLENGTH, clampedValue);
    settings()->storeValue(KEY_SAVEPATHHISTORY
        , QStringList(settings()->loadValue<QStringList>(KEY_SAVEPATHHISTORY).mid(0, clampedValue)));
}

#ifndef Q_OS_MACOS
void AddNewTorrentDialog::setAttached(const bool value)
{
    settings()->storeValue(KEY_ATTACHED, value);
}

bool AddNewTorrentDialog::isAttached()
{
    return settings()->loadValue(KEY_ATTACHED, false);
}
#endif

void AddNewTorrentDialog::loadState()
{
    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);

    m_ui->splitter->restoreState(m_storeSplitterState);;
}

void AddNewTorrentDialog::saveState()
{
    m_storeDialogSize = size();
    m_storeSplitterState = m_ui->splitter->saveState();
    if (hasMetadata())
        m_storeTreeHeaderState = m_ui->contentTreeView->header()->saveState();
}

void AddNewTorrentDialog::show(const QString &source, const BitTorrent::AddTorrentParams &inParams, QWidget *parent)
{
    const auto *pref = Preferences::instance();
#ifdef Q_OS_MACOS
    const bool attached = false;
#else
    const bool attached = isAttached();
#endif

    // By not setting a parent to the "AddNewTorrentDialog", all those dialogs
    // will be displayed on top and will not overlap with the main window.
    auto *dlg = new AddNewTorrentDialog(inParams, (attached ? parent : nullptr));
    // Qt::Window is required to avoid showing only two dialog on top (see #12852).
    // Also improves the general convenience of adding multiple torrents.
    if (!attached)
    {
        dlg->setWindowFlags(Qt::Window);
        adjustDialogGeometry(dlg, parent);
    }
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    if (Net::DownloadManager::hasSupportedScheme(source))
    {
        // Launch downloader
        Net::DownloadManager::instance()->download(
                Net::DownloadRequest(source).limit(pref->getTorrentFileSizeLimit())
                , pref->useProxyForGeneralPurposes()
                , dlg, &AddNewTorrentDialog::handleDownloadFinished);
        return;
    }

    const BitTorrent::MagnetUri magnetUri {source};
    const bool isLoaded = magnetUri.isValid()
        ? dlg->loadMagnet(magnetUri)
        : dlg->loadTorrentFile(source);

    if (isLoaded)
        dlg->QDialog::show();
    else
        delete dlg;
}

void AddNewTorrentDialog::show(const QString &source, QWidget *parent)
{
    show(source, BitTorrent::AddTorrentParams(), parent);
}

bool AddNewTorrentDialog::loadTorrentFile(const QString &source)
{
    const Path decodedPath {source.startsWith(u"file://", Qt::CaseInsensitive)
                ? QUrl::fromEncoded(source.toLocal8Bit()).toLocalFile()
                : source};

    const nonstd::expected<BitTorrent::TorrentInfo, QString> result = BitTorrent::TorrentInfo::loadFromFile(decodedPath);
    if (!result)
    {
        RaisedMessageBox::critical(this, tr("Invalid torrent")
            , tr("Failed to load the torrent: %1.\nError: %2", "Don't remove the '\n' characters. They insert a newline.")
                .arg(decodedPath.toString(), result.error()));
        return false;
    }

    m_torrentInfo = result.value();
    m_torrentGuard = std::make_unique<TorrentFileGuard>(decodedPath);

    return loadTorrentImpl();
}

bool AddNewTorrentDialog::loadTorrentImpl()
{
    const BitTorrent::InfoHash infoHash = m_torrentInfo.infoHash();

    // Prevent showing the dialog if download is already present
    const auto *btSession = BitTorrent::Session::instance();
    if (btSession->isKnownTorrent(infoHash))
    {
        BitTorrent::Torrent *const torrent = btSession->findTorrent(infoHash);
        if (torrent)
        {
            // Trying to set metadata to existing torrent in case if it has none
            torrent->setMetadata(m_torrentInfo);

            if (torrent->isPrivate() || m_torrentInfo.isPrivate())
            {
                RaisedMessageBox::warning(this, tr("Torrent is already present"), tr("Torrent '%1' is already in the transfer list. Trackers cannot be merged because it is a private torrent.").arg(torrent->name()), QMessageBox::Ok);
            }
            else
            {
                bool mergeTrackers = btSession->isMergeTrackersEnabled();
                if (Preferences::instance()->confirmMergeTrackers())
                {
                    const QMessageBox::StandardButton btn = RaisedMessageBox::question(this, tr("Torrent is already present")
                            , tr("Torrent '%1' is already in the transfer list. Do you want to merge trackers from new source?").arg(torrent->name())
                            , (QMessageBox::Yes | QMessageBox::No), QMessageBox::Yes);
                    mergeTrackers = (btn == QMessageBox::Yes);
                }

                if (mergeTrackers)
                {
                    torrent->addTrackers(m_torrentInfo.trackers());
                    torrent->addUrlSeeds(m_torrentInfo.urlSeeds());
                }
            }
        }
        else
        {
            RaisedMessageBox::information(this, tr("Torrent is already present"), tr("Torrent is already queued for processing."), QMessageBox::Ok);
        }

        return false;
    }

    m_ui->labelInfohash1Data->setText(m_torrentInfo.infoHash().v1().isValid() ? m_torrentInfo.infoHash().v1().toString() : tr("N/A"));
    m_ui->labelInfohash2Data->setText(m_torrentInfo.infoHash().v2().isValid() ? m_torrentInfo.infoHash().v2().toString() : tr("N/A"));
    setupTreeview();
    TMMChanged(m_ui->comboTTM->currentIndex());

    return true;
}

bool AddNewTorrentDialog::loadMagnet(const BitTorrent::MagnetUri &magnetUri)
{
    if (!magnetUri.isValid())
    {
        RaisedMessageBox::critical(this, tr("Invalid magnet link"), tr("This magnet link was not recognized"));
        return false;
    }

    m_torrentGuard = std::make_unique<TorrentFileGuard>();

    const BitTorrent::InfoHash infoHash = magnetUri.infoHash();

    // Prevent showing the dialog if download is already present
    auto *btSession = BitTorrent::Session::instance();
    if (btSession->isKnownTorrent(infoHash))
    {
        BitTorrent::Torrent *const torrent = btSession->findTorrent(infoHash);
        if (torrent)
        {
            if (torrent->isPrivate())
            {
                RaisedMessageBox::warning(this, tr("Torrent is already present"), tr("Torrent '%1' is already in the transfer list. Trackers haven't been merged because it is a private torrent.").arg(torrent->name()), QMessageBox::Ok);
            }
            else
            {
                bool mergeTrackers = btSession->isMergeTrackersEnabled();
                if (Preferences::instance()->confirmMergeTrackers())
                {
                    const QMessageBox::StandardButton btn = RaisedMessageBox::question(this, tr("Torrent is already present")
                            , tr("Torrent '%1' is already in the transfer list. Do you want to merge trackers from new source?").arg(torrent->name())
                            , (QMessageBox::Yes | QMessageBox::No), QMessageBox::Yes);
                    mergeTrackers = (btn == QMessageBox::Yes);
                }

                if (mergeTrackers)
                {
                    torrent->addTrackers(magnetUri.trackers());
                    torrent->addUrlSeeds(magnetUri.urlSeeds());
                }
            }
        }
        else
        {
            RaisedMessageBox::information(this, tr("Torrent is already present"), tr("Magnet link is already queued for processing."), QMessageBox::Ok);
        }

        return false;
    }

    connect(btSession, &BitTorrent::Session::metadataDownloaded, this, &AddNewTorrentDialog::updateMetadata);

    // Set dialog title
    const QString torrentName = magnetUri.name();
    setWindowTitle(torrentName.isEmpty() ? tr("Magnet link") : torrentName);

    updateDiskSpaceLabel();
    TMMChanged(m_ui->comboTTM->currentIndex());

    btSession->downloadMetadata(magnetUri);
    setMetadataProgressIndicator(true, tr("Retrieving metadata..."));
    m_ui->labelInfohash1Data->setText(magnetUri.infoHash().v1().isValid() ? magnetUri.infoHash().v1().toString() : tr("N/A"));
    m_ui->labelInfohash2Data->setText(magnetUri.infoHash().v2().isValid() ? magnetUri.infoHash().v2().toString() : tr("N/A"));

    m_magnetURI = magnetUri;
    return true;
}

void AddNewTorrentDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (!isTopLevel()) return;

    activateWindow();
    raise();
}

void AddNewTorrentDialog::updateDiskSpaceLabel()
{
    // Determine torrent size
    qlonglong torrentSize = 0;

    if (hasMetadata())
    {
        const QVector<BitTorrent::DownloadPriority> &priorities = m_contentAdaptor->filePriorities();
        Q_ASSERT(priorities.size() == m_torrentInfo.filesCount());
        for (int i = 0; i < priorities.size(); ++i)
        {
            if (priorities[i] > BitTorrent::DownloadPriority::Ignored)
                torrentSize += m_torrentInfo.fileSize(i);
        }
    }

    const QString freeSpace = Utils::Misc::friendlyUnit(queryFreeDiskSpace(m_ui->savePath->selectedPath()));
    const QString sizeString = tr("%1 (Free space on disk: %2)").arg(
        ((torrentSize > 0) ? Utils::Misc::friendlyUnit(torrentSize) : tr("Not available", "This size is unavailable."))
        , freeSpace);
    m_ui->labelSizeData->setText(sizeString);
}

void AddNewTorrentDialog::onSavePathChanged(const Path &newPath)
{
    Q_UNUSED(newPath);
    // Remember index
    m_savePathIndex = m_ui->savePath->currentIndex();
    updateDiskSpaceLabel();
}

void AddNewTorrentDialog::onDownloadPathChanged(const Path &newPath)
{
    Q_UNUSED(newPath);
    // Remember index
    const int currentPathIndex = m_ui->downloadPath->currentIndex();
    if (currentPathIndex >= 0)
        m_downloadPathIndex = m_ui->downloadPath->currentIndex();
}

void AddNewTorrentDialog::onUseDownloadPathChanged(const bool checked)
{
    m_useDownloadPath = checked;
    m_ui->downloadPath->setCurrentIndex(checked ? m_downloadPathIndex : -1);
}

void AddNewTorrentDialog::categoryChanged(int index)
{
    Q_UNUSED(index);

    if (m_ui->comboTTM->currentIndex() == 1)
    {
        const auto *btSession = BitTorrent::Session::instance();
        const QString categoryName = m_ui->categoryComboBox->currentText();

        const Path savePath = btSession->categorySavePath(categoryName);
        m_ui->savePath->setSelectedPath(savePath);

        const Path downloadPath = btSession->categoryDownloadPath(categoryName);
        m_ui->downloadPath->setSelectedPath(downloadPath);

        m_ui->groupBoxDownloadPath->setChecked(!m_ui->downloadPath->selectedPath().isEmpty());

        updateDiskSpaceLabel();
    }
}

void AddNewTorrentDialog::contentLayoutChanged()
{
    if (!hasMetadata())
        return;

    const auto contentLayout = static_cast<BitTorrent::TorrentContentLayout>(m_ui->contentLayoutComboBox->currentIndex());
    m_contentAdaptor->applyContentLayout(contentLayout);
    m_ui->contentTreeView->setContentHandler(m_contentAdaptor.get()); // to cause reloading
}

void AddNewTorrentDialog::saveTorrentFile()
{
    Q_ASSERT(hasMetadata());

    const QString filter {tr("Torrent file (*%1)").arg(TORRENT_FILE_EXTENSION)};

    Path path {QFileDialog::getSaveFileName(this, tr("Save as torrent file")
                , QDir::home().absoluteFilePath(m_torrentInfo.name() + TORRENT_FILE_EXTENSION)
                , filter)};
    if (path.isEmpty()) return;

    if (!path.hasExtension(TORRENT_FILE_EXTENSION))
        path += TORRENT_FILE_EXTENSION;

    const nonstd::expected<void, QString> result = m_torrentInfo.saveToFile(path);
    if (!result)
    {
        QMessageBox::critical(this, tr("I/O Error")
            , tr("Couldn't export torrent metadata file '%1'. Reason: %2.").arg(path.toString(), result.error()));
    }
}

bool AddNewTorrentDialog::hasMetadata() const
{
    return m_torrentInfo.isValid();
}

void AddNewTorrentDialog::populateSavePaths()
{
    const auto *btSession = BitTorrent::Session::instance();

    m_ui->savePath->blockSignals(true);
    m_ui->savePath->clear();
    const auto savePathHistory = settings()->loadValue<QStringList>(KEY_SAVEPATHHISTORY);
    if (savePathHistory.size() > 0)
    {
        for (const QString &path : savePathHistory)
            m_ui->savePath->addItem(Path(path));
    }
    else
    {
        m_ui->savePath->addItem(btSession->savePath());
    }

    if (m_savePathIndex >= 0)
    {
        m_ui->savePath->setCurrentIndex(std::min(m_savePathIndex, (m_ui->savePath->count() - 1)));
    }
    else
    {
        if (!m_torrentParams.savePath.isEmpty())
            setPath(m_ui->savePath, m_torrentParams.savePath);
        else if (!m_storeRememberLastSavePath)
            setPath(m_ui->savePath, btSession->savePath());
        else
            m_ui->savePath->setCurrentIndex(0);

        m_savePathIndex = m_ui->savePath->currentIndex();
    }

    m_ui->savePath->blockSignals(false);

    m_ui->downloadPath->blockSignals(true);
    m_ui->downloadPath->clear();
    const auto downloadPathHistory = settings()->loadValue<QStringList>(KEY_DOWNLOADPATHHISTORY);
    if (downloadPathHistory.size() > 0)
    {
        for (const QString &path : downloadPathHistory)
            m_ui->downloadPath->addItem(Path(path));
    }
    else
    {
        m_ui->downloadPath->addItem(btSession->downloadPath());
    }

    if (m_downloadPathIndex >= 0)
    {
        m_ui->downloadPath->setCurrentIndex(m_useDownloadPath ? std::min(m_downloadPathIndex, (m_ui->downloadPath->count() - 1)) : -1);
        m_ui->groupBoxDownloadPath->setChecked(m_useDownloadPath);
    }
    else
    {
        const bool useDownloadPath = m_torrentParams.useDownloadPath.value_or(btSession->isDownloadPathEnabled());
        m_ui->groupBoxDownloadPath->setChecked(useDownloadPath);

        if (!m_torrentParams.downloadPath.isEmpty())
            setPath(m_ui->downloadPath, m_torrentParams.downloadPath);
        else if (!m_storeRememberLastSavePath)
            setPath(m_ui->downloadPath, btSession->downloadPath());
        else
            m_ui->downloadPath->setCurrentIndex(0);

        m_downloadPathIndex = m_ui->downloadPath->currentIndex();
        if (!useDownloadPath)
            m_ui->downloadPath->setCurrentIndex(-1);
    }

    m_ui->downloadPath->blockSignals(false);
    m_ui->groupBoxDownloadPath->blockSignals(false);
}

void AddNewTorrentDialog::accept()
{
    // TODO: Check if destination actually exists
    m_torrentParams.skipChecking = m_ui->skipCheckingCheckBox->isChecked();

    // Category
    m_torrentParams.category = m_ui->categoryComboBox->currentText();
    if (m_ui->defaultCategoryCheckbox->isChecked())
        m_storeDefaultCategory = m_torrentParams.category;

    m_storeRememberLastSavePath = m_ui->checkBoxRememberLastSavePath->isChecked();

    m_torrentParams.addToQueueTop = m_ui->addToQueueTopCheckBox->isChecked();
    m_torrentParams.addPaused = !m_ui->startTorrentCheckBox->isChecked();
    m_torrentParams.stopCondition = m_ui->stopConditionComboBox->currentData().value<BitTorrent::Torrent::StopCondition>();
    m_torrentParams.contentLayout = static_cast<BitTorrent::TorrentContentLayout>(m_ui->contentLayoutComboBox->currentIndex());

    m_torrentParams.sequential = m_ui->sequentialCheckBox->isChecked();
    m_torrentParams.firstLastPiecePriority = m_ui->firstLastCheckBox->isChecked();

    const bool useAutoTMM = (m_ui->comboTTM->currentIndex() == 1); // 1 is Automatic mode. Handle all non 1 values as manual mode.
    m_torrentParams.useAutoTMM = useAutoTMM;
    if (!useAutoTMM)
    {
        const Path savePath = m_ui->savePath->selectedPath();
        m_torrentParams.savePath = savePath;
        updatePathHistory(KEY_SAVEPATHHISTORY, savePath, savePathHistoryLength());

        m_torrentParams.useDownloadPath = m_ui->groupBoxDownloadPath->isChecked();
        if (m_torrentParams.useDownloadPath)
        {
            const Path downloadPath = m_ui->downloadPath->selectedPath();
            m_torrentParams.downloadPath = downloadPath;
            updatePathHistory(KEY_DOWNLOADPATHHISTORY, downloadPath, savePathHistoryLength());
        }
        else
        {
            m_torrentParams.downloadPath = Path();
        }
    }
    else
    {
        m_torrentParams.savePath = Path();
        m_torrentParams.downloadPath = Path();
        m_torrentParams.useDownloadPath = std::nullopt;
    }

    setEnabled(!m_ui->checkBoxNeverShow->isChecked());

    // Add torrent
    if (!hasMetadata())
        BitTorrent::Session::instance()->addTorrent(m_magnetURI, m_torrentParams);
    else
        BitTorrent::Session::instance()->addTorrent(m_torrentInfo, m_torrentParams);

    m_torrentGuard->markAsAddedToSession();
    QDialog::accept();
}

void AddNewTorrentDialog::reject()
{
    if (!hasMetadata())
    {
        setMetadataProgressIndicator(false);
        BitTorrent::Session::instance()->cancelDownloadMetadata(m_magnetURI.infoHash().toTorrentID());
    }

    QDialog::reject();
}

void AddNewTorrentDialog::updateMetadata(const BitTorrent::TorrentInfo &metadata)
{
    Q_ASSERT(metadata.isValid());

    if (metadata.infoHash() != m_magnetURI.infoHash()) return;

    disconnect(BitTorrent::Session::instance(), &BitTorrent::Session::metadataDownloaded, this, &AddNewTorrentDialog::updateMetadata);

    // Good to go
    m_torrentInfo = metadata;
    setMetadataProgressIndicator(true, tr("Parsing metadata..."));

    // Update UI
    setupTreeview();
    setMetadataProgressIndicator(false, tr("Metadata retrieval complete"));

    if (const auto stopCondition = m_ui->stopConditionComboBox->currentData().value<BitTorrent::Torrent::StopCondition>()
            ; stopCondition == BitTorrent::Torrent::StopCondition::MetadataReceived)
    {
        m_ui->startTorrentCheckBox->setChecked(false);

        const auto index = m_ui->stopConditionComboBox->currentIndex();
        m_ui->stopConditionComboBox->setCurrentIndex(m_ui->stopConditionComboBox->findData(
                QVariant::fromValue(BitTorrent::Torrent::StopCondition::None)));
        m_ui->stopConditionComboBox->removeItem(index);
    }

    m_ui->buttonSave->setVisible(true);
    if (m_torrentInfo.infoHash().v2().isValid())
    {
        m_ui->buttonSave->setEnabled(false);
        m_ui->buttonSave->setToolTip(tr("Cannot create v2 torrent until its data is fully downloaded."));
    }
}

void AddNewTorrentDialog::setMetadataProgressIndicator(bool visibleIndicator, const QString &labelText)
{
    // Always show info label when waiting for metadata
    m_ui->lblMetaLoading->setVisible(true);
    m_ui->lblMetaLoading->setText(labelText);
    m_ui->progMetaLoading->setVisible(visibleIndicator);
}

void AddNewTorrentDialog::setupTreeview()
{
    Q_ASSERT(hasMetadata());
    if (Q_UNLIKELY(!hasMetadata()))
        return;

    // Set dialog title
    setWindowTitle(m_torrentInfo.name());

    // Set torrent information
    m_ui->labelCommentData->setText(Utils::Misc::parseHtmlLinks(m_torrentInfo.comment().toHtmlEscaped()));
    m_ui->labelDateData->setText(!m_torrentInfo.creationDate().isNull() ? QLocale().toString(m_torrentInfo.creationDate(), QLocale::ShortFormat) : tr("Not available"));

    if (m_torrentParams.filePaths.isEmpty())
        m_torrentParams.filePaths = m_torrentInfo.filePaths();

    m_contentAdaptor = std::make_unique<TorrentContentAdaptor>(m_torrentInfo, m_torrentParams.filePaths
            , m_torrentParams.filePriorities, [this] { updateDiskSpaceLabel(); });

    const auto contentLayout = static_cast<BitTorrent::TorrentContentLayout>(m_ui->contentLayoutComboBox->currentIndex());
    m_contentAdaptor->applyContentLayout(contentLayout);

    if (BitTorrent::Session::instance()->isExcludedFileNamesEnabled())
    {
        // Check file name blacklist for torrents that are manually added
        QVector<BitTorrent::DownloadPriority> priorities = m_contentAdaptor->filePriorities();
        for (int i = 0; i < priorities.size(); ++i)
        {
            if (priorities[i] == BitTorrent::DownloadPriority::Ignored)
                continue;

            if (BitTorrent::Session::instance()->isFilenameExcluded(m_torrentInfo.filePath(i).filename()))
                priorities[i] = BitTorrent::DownloadPriority::Ignored;
        }

        m_contentAdaptor->prioritizeFiles(priorities);
    }

    m_ui->contentTreeView->setContentHandler(m_contentAdaptor.get());

    m_filterLine->blockSignals(false);

    updateDiskSpaceLabel();
}

void AddNewTorrentDialog::handleDownloadFinished(const Net::DownloadResult &downloadResult)
{
    switch (downloadResult.status)
    {
    case Net::DownloadStatus::Success:
        {
            const nonstd::expected<BitTorrent::TorrentInfo, QString> result = BitTorrent::TorrentInfo::load(downloadResult.data);
            if (!result)
            {
                RaisedMessageBox::critical(this, tr("Invalid torrent"), tr("Failed to load from URL: %1.\nError: %2")
                                           .arg(downloadResult.url, result.error()));
                return;
            }

            m_torrentInfo = result.value();
            m_torrentGuard = std::make_unique<TorrentFileGuard>();

            if (loadTorrentImpl())
                open();
            else
                deleteLater();
        }
        break;
    case Net::DownloadStatus::RedirectedToMagnet:
        if (loadMagnet(BitTorrent::MagnetUri(downloadResult.magnet)))
            open();
        else
            deleteLater();
        break;
    default:
        RaisedMessageBox::critical(this, tr("Download Error"),
            tr("Cannot download '%1': %2").arg(downloadResult.url, downloadResult.errorString));
        deleteLater();
    }
}

void AddNewTorrentDialog::TMMChanged(int index)
{
    if (index != 1)
    { // 0 is Manual mode and 1 is Automatic mode. Handle all non 1 values as manual mode.
        populateSavePaths();
        m_ui->groupBoxSavePath->setEnabled(true);
    }
    else
    {
        const auto *session = BitTorrent::Session::instance();

        m_ui->groupBoxSavePath->setEnabled(false);

        m_ui->savePath->blockSignals(true);
        m_ui->savePath->clear();
        const Path savePath = session->categorySavePath(m_ui->categoryComboBox->currentText());
        m_ui->savePath->addItem(savePath);

        m_ui->downloadPath->blockSignals(true);
        m_ui->downloadPath->clear();
        const Path downloadPath = session->categoryDownloadPath(m_ui->categoryComboBox->currentText());
        m_ui->downloadPath->addItem(downloadPath);

        m_ui->groupBoxDownloadPath->blockSignals(true);
        m_ui->groupBoxDownloadPath->setChecked(!downloadPath.isEmpty());
    }

    updateDiskSpaceLabel();
}

void AddNewTorrentDialog::doNotDeleteTorrentClicked(bool checked)
{
    m_torrentGuard->setAutoRemove(!checked);
}
