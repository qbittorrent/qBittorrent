/*
 * Bittorrent Client using Qt and libtorrent.
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

#include "transferlistwidget.h"

#include <QClipboard>
#include <QDebug>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QRegExp>
#include <QRegularExpression>
#include <QShortcut>
#include <QStylePainter>
#include <QTableView>
#include <QWheelEvent>
#include <QWidgetAction>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/torrentfilter.h"
#include "base/utils/fs.h"
#include "base/utils/string.h"
#include "autoexpandabledialog.h"
#include "deletionconfirmationdialog.h"
#include "guiiconprovider.h"
#include "mainwindow.h"
#include "optionsdialog.h"
#include "previewselectdialog.h"
#include "speedlimitdialog.h"
#include "torrentcategorydialog.h"
#include "transferlistdelegate.h"
#include "transferlistmodel.h"
#include "transferlistsortmodel.h"
#include "updownratiodialog.h"

#ifdef Q_OS_MAC
#include "macutilities.h"
#endif

namespace
{
    using ToggleFn = std::function<void (Qt::CheckState)>;

    QStringList extractHashes(const QList<BitTorrent::TorrentHandle *> &torrents)
    {
        QStringList hashes;
        for (BitTorrent::TorrentHandle *const torrent : torrents)
            hashes << torrent->hash();

        return hashes;
    }

    // Helper for setting style parameters when painting check box primitives.
    class CheckBoxIconHelper : public QCheckBox
    {
    public:
        explicit CheckBoxIconHelper(QWidget *parent);
        QSize sizeHint() const override;
        void initStyleOption(QStyleOptionButton *opt) const;

    protected:
        void paintEvent(QPaintEvent *) override {}
    };

    CheckBoxIconHelper::CheckBoxIconHelper(QWidget *parent)
        : QCheckBox(parent)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    QSize CheckBoxIconHelper::sizeHint() const
    {
        const int dim = QCheckBox::sizeHint().height();
        return QSize(dim, dim);
    }

    void CheckBoxIconHelper::initStyleOption(QStyleOptionButton *opt) const
    {
        QCheckBox::initStyleOption(opt);
    }

    // Tristate checkbox styled for use in menus.
    class MenuCheckBox : public QWidget
    {
    public:
        MenuCheckBox(const QString &text, const ToggleFn &onToggle, Qt::CheckState initialState);
        QSize sizeHint() const override;

    protected:
        void paintEvent(QPaintEvent *e) override;
        void mousePressEvent(QMouseEvent *) override;

    private:
        CheckBoxIconHelper *const m_checkBox;
        const QString m_text;
        QSize m_sizeHint;
        QSize m_checkBoxOffset;
    };

    MenuCheckBox::MenuCheckBox(const QString &text, const ToggleFn &onToggle, Qt::CheckState initialState)
        : m_checkBox(new CheckBoxIconHelper(this))
        , m_text(text)
        , m_sizeHint(QCheckBox(m_text).sizeHint())
    {
        m_checkBox->setCheckState(initialState);
        connect(m_checkBox, &QCheckBox::stateChanged, this, [this, onToggle](int newState)
        {
            m_checkBox->setTristate(false);
            onToggle(static_cast<Qt::CheckState>(newState));
        });
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setMouseTracking(true);

        // We attempt to mimic the amount of vertical whitespace padding around a QCheckBox.
        QSize layoutPadding(3, 0);
        const int sizeHintMargin = (m_sizeHint.height() - m_checkBox->sizeHint().height()) / 2;
        if (sizeHintMargin > 0) {
            m_checkBoxOffset.setHeight(sizeHintMargin);
        }
        else {
            layoutPadding.setHeight(1);
            m_checkBoxOffset.setHeight(1);
        }
        m_checkBoxOffset.setWidth(layoutPadding.width());

        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->addWidget(m_checkBox);
        layout->addStretch();
        layout->setContentsMargins(layoutPadding.width(), layoutPadding.height(), layoutPadding.width(), layoutPadding.height());
        setLayout(layout);
    }

    QSize MenuCheckBox::sizeHint() const
    {
        return m_sizeHint;
    }

    void MenuCheckBox::paintEvent(QPaintEvent *e)
    {
        if (!rect().intersects(e->rect()))
            return;
        QStylePainter painter(this);
        QStyleOptionMenuItem menuOpt;
        menuOpt.initFrom(this);
        menuOpt.menuItemType = QStyleOptionMenuItem::Normal;
        menuOpt.text = m_text;
        QStyleOptionButton checkBoxOpt;
        m_checkBox->initStyleOption(&checkBoxOpt);
        checkBoxOpt.rect.translate(m_checkBoxOffset.width(), m_checkBoxOffset.height());
        if (rect().contains(mapFromGlobal(QCursor::pos()))) {
            menuOpt.state |= QStyle::State_Selected;
            checkBoxOpt.state |= QStyle::State_MouseOver;
        }
        painter.drawControl(QStyle::CE_MenuItem, menuOpt);
        painter.drawPrimitive(QStyle::PE_IndicatorCheckBox, checkBoxOpt);
    }

    void MenuCheckBox::mousePressEvent(QMouseEvent *)
    {
        m_checkBox->click();
    }

    class CheckBoxMenuItem : public QWidgetAction
    {
    public:
        CheckBoxMenuItem(const QString &text, const ToggleFn &onToggle, Qt::CheckState initialState, QObject *parent)
            : QWidgetAction(parent)
        {
            setDefaultWidget(new MenuCheckBox(text, onToggle, initialState));
        }
    };
}

TransferListWidget::TransferListWidget(QWidget *parent, MainWindow *mainWindow)
    : QTreeView(parent)
    , m_mainWindow(mainWindow)
{

    setUniformRowHeights(true);
    // Load settings
    bool columnLoaded = loadSettings();

    // Create and apply delegate
    m_listDelegate = new TransferListDelegate(this);
    setItemDelegate(m_listDelegate);

    // Create transfer list model
    m_listModel = new TransferListModel(this);

    m_sortFilterModel = new TransferListSortModel();
    m_sortFilterModel->setDynamicSortFilter(true);
    m_sortFilterModel->setSourceModel(m_listModel);
    m_sortFilterModel->setFilterKeyColumn(TransferListModel::TR_NAME);
    m_sortFilterModel->setFilterRole(Qt::DisplayRole);
    m_sortFilterModel->setSortCaseSensitivity(Qt::CaseInsensitive);

    setModel(m_sortFilterModel);

    // Visual settings
    setRootIsDecorated(false);
    setAllColumnsShowFocus(true);
    setSortingEnabled(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setItemsExpandable(false);
    setAutoScroll(true);
    setDragDropMode(QAbstractItemView::DragOnly);
#if defined(Q_OS_MAC)
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
    header()->setStretchLastSection(false);

    // Default hidden columns
    if (!columnLoaded) {
        setColumnHidden(TransferListModel::TR_ADD_DATE, true);
        setColumnHidden(TransferListModel::TR_SEED_DATE, true);
        setColumnHidden(TransferListModel::TR_UPLIMIT, true);
        setColumnHidden(TransferListModel::TR_DLLIMIT, true);
        setColumnHidden(TransferListModel::TR_TRACKER, true);
        setColumnHidden(TransferListModel::TR_AMOUNT_DOWNLOADED, true);
        setColumnHidden(TransferListModel::TR_AMOUNT_UPLOADED, true);
        setColumnHidden(TransferListModel::TR_AMOUNT_DOWNLOADED_SESSION, true);
        setColumnHidden(TransferListModel::TR_AMOUNT_UPLOADED_SESSION, true);
        setColumnHidden(TransferListModel::TR_AMOUNT_LEFT, true);
        setColumnHidden(TransferListModel::TR_TIME_ELAPSED, true);
        setColumnHidden(TransferListModel::TR_SAVE_PATH, true);
        setColumnHidden(TransferListModel::TR_COMPLETED, true);
        setColumnHidden(TransferListModel::TR_RATIO_LIMIT, true);
        setColumnHidden(TransferListModel::TR_SEEN_COMPLETE_DATE, true);
        setColumnHidden(TransferListModel::TR_LAST_ACTIVITY, true);
        setColumnHidden(TransferListModel::TR_TOTAL_SIZE, true);
    }

    //Ensure that at least one column is visible at all times
    bool atLeastOne = false;
    for (int i = 0; i < TransferListModel::NB_COLUMNS; ++i) {
        if (!isColumnHidden(i)) {
            atLeastOne = true;
            break;
        }
    }
    if (!atLeastOne)
        setColumnHidden(TransferListModel::TR_NAME, false);

    //When adding/removing columns between versions some may
    //end up being size 0 when the new version is launched with
    //a conf file from the previous version.
    for (int i = 0; i < TransferListModel::NB_COLUMNS; ++i)
        if ((columnWidth(i) <= 0) && (!isColumnHidden(i)))
            resizeColumnToContents(i);

    setContextMenuPolicy(Qt::CustomContextMenu);

    // Listen for list events
    connect(this, &QAbstractItemView::doubleClicked, this, &TransferListWidget::torrentDoubleClicked);
    connect(this, &QWidget::customContextMenuRequested, this, &TransferListWidget::displayListMenu);
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), &QWidget::customContextMenuRequested, this, &TransferListWidget::displayDLHoSMenu);
    connect(header(), &QHeaderView::sectionMoved, this, &TransferListWidget::saveSettings);
    connect(header(), &QHeaderView::sectionResized, this, &TransferListWidget::saveSettings);
    connect(header(), &QHeaderView::sortIndicatorChanged, this, &TransferListWidget::saveSettings);

    m_editHotkey = new QShortcut(Qt::Key_F2, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(m_editHotkey, &QShortcut::activated, this, &TransferListWidget::renameSelectedTorrent);
    m_deleteHotkey = new QShortcut(QKeySequence::Delete, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(m_deleteHotkey, &QShortcut::activated, this, &TransferListWidget::softDeleteSelectedTorrents);
    m_permDeleteHotkey = new QShortcut(Qt::SHIFT + Qt::Key_Delete, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(m_permDeleteHotkey, &QShortcut::activated, this, &TransferListWidget::permDeleteSelectedTorrents);
    m_doubleClickHotkey = new QShortcut(Qt::Key_Return, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(m_doubleClickHotkey, &QShortcut::activated, this, &TransferListWidget::torrentDoubleClicked);
    m_recheckHotkey = new QShortcut(Qt::CTRL + Qt::Key_R, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(m_recheckHotkey, &QShortcut::activated, this, &TransferListWidget::recheckSelectedTorrents);

    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(header());
    header()->setParent(this);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));
}

TransferListWidget::~TransferListWidget()
{
    qDebug() << Q_FUNC_INFO << "ENTER";
    // Save settings
    saveSettings();
    // Clean up
    delete m_sortFilterModel;
    delete m_listModel;
    delete m_listDelegate;
    qDebug() << Q_FUNC_INFO << "EXIT";
}

TransferListModel *TransferListWidget::getSourceModel() const
{
    return m_listModel;
}

void TransferListWidget::previewFile(QString filePath)
{
    Utils::Misc::openPath(filePath);
}

inline QModelIndex TransferListWidget::mapToSource(const QModelIndex &index) const
{
    Q_ASSERT(index.isValid());
    if (index.model() == m_sortFilterModel)
        return m_sortFilterModel->mapToSource(index);
    return index;
}

inline QModelIndex TransferListWidget::mapFromSource(const QModelIndex &index) const
{
    Q_ASSERT(index.isValid());
    Q_ASSERT(index.model() == m_sortFilterModel);
    return m_sortFilterModel->mapFromSource(index);
}

void TransferListWidget::torrentDoubleClicked()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if ((selectedIndexes.size() != 1) || !selectedIndexes.first().isValid()) return;

    const QModelIndex index = m_listModel->index(mapToSource(selectedIndexes.first()).row());
    BitTorrent::TorrentHandle *const torrent = m_listModel->torrentHandle(index);
    if (!torrent) return;

    int action;
    if (torrent->isSeed())
        action = Preferences::instance()->getActionOnDblClOnTorrentFn();
    else
        action = Preferences::instance()->getActionOnDblClOnTorrentDl();

    switch (action) {
    case TOGGLE_PAUSE:
        if (torrent->isPaused())
            torrent->resume();
        else
            torrent->pause();
        break;
    case OPEN_DEST:
#ifdef Q_OS_MAC
        MacUtils::openFiles(QSet<QString>{torrent->contentPath(true)});
#else
        if (torrent->filesCount() == 1)
            Utils::Misc::openFolderSelect(torrent->contentPath(true));
        else
            Utils::Misc::openPath(torrent->contentPath(true));
#endif
        break;
    }
}

QList<BitTorrent::TorrentHandle *> TransferListWidget::getSelectedTorrents() const
{
    QList<BitTorrent::TorrentHandle *> torrents;
    for (const QModelIndex &index : asConst(selectionModel()->selectedRows()))
        torrents << m_listModel->torrentHandle(mapToSource(index));

    return torrents;
}

void TransferListWidget::setSelectedTorrentsLocation()
{
    const QList<BitTorrent::TorrentHandle *> torrents = getSelectedTorrents();
    if (torrents.isEmpty()) return;

    const QString oldLocation = torrents[0]->savePath();
    qDebug("Old location is %s", qUtf8Printable(oldLocation));

    const QString newLocation = QFileDialog::getExistingDirectory(this, tr("Choose save path"), oldLocation,
                                            QFileDialog::DontConfirmOverwrite | QFileDialog::ShowDirsOnly | QFileDialog::HideNameFilterDetails);
    if (newLocation.isEmpty() || !QDir(newLocation).exists()) return;
    qDebug("New location is %s", qUtf8Printable(newLocation));

    // Actually move storage
    for (BitTorrent::TorrentHandle *const torrent : torrents) {
        Logger::instance()->addMessage(tr("Set location: moving \"%1\", from \"%2\" to \"%3\""
            , "Set location: moving \"ubuntu_16_04.iso\", from \"/home/dir1\" to \"/home/dir2\"")
            .arg(torrent->name(), Utils::Fs::toNativePath(torrent->savePath())
                , Utils::Fs::toNativePath(newLocation)));
        torrent->move(Utils::Fs::expandPathAbs(newLocation));
    }
}

void TransferListWidget::pauseAllTorrents()
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(BitTorrent::Session::instance()->torrents()))
        torrent->pause();
}

void TransferListWidget::resumeAllTorrents()
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(BitTorrent::Session::instance()->torrents()))
        torrent->resume();
}

void TransferListWidget::startSelectedTorrents()
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrent->resume();
}

void TransferListWidget::forceStartSelectedTorrents()
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrent->resume(true);
}

void TransferListWidget::startVisibleTorrents()
{
    for (int i = 0; i < m_sortFilterModel->rowCount(); ++i) {
        BitTorrent::TorrentHandle *const torrent = m_listModel->torrentHandle(mapToSource(m_sortFilterModel->index(i, 0)));
        if (torrent)
            torrent->resume();
    }
}

void TransferListWidget::pauseSelectedTorrents()
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrent->pause();
}

void TransferListWidget::pauseVisibleTorrents()
{
    for (int i = 0; i < m_sortFilterModel->rowCount(); ++i) {
        BitTorrent::TorrentHandle *const torrent = m_listModel->torrentHandle(mapToSource(m_sortFilterModel->index(i, 0)));
        if (torrent)
            torrent->pause();
    }
}

void TransferListWidget::softDeleteSelectedTorrents()
{
    deleteSelectedTorrents(false);
}

void TransferListWidget::permDeleteSelectedTorrents()
{
    deleteSelectedTorrents(true);
}

void TransferListWidget::deleteSelectedTorrents(bool deleteLocalFiles)
{
    if (m_mainWindow->currentTabWidget() != this) return;

    const QList<BitTorrent::TorrentHandle *> torrents = getSelectedTorrents();
    if (torrents.empty()) return;

    if (Preferences::instance()->confirmTorrentDeletion()
        && !DeletionConfirmationDialog::askForDeletionConfirmation(this, deleteLocalFiles, torrents.size(), torrents[0]->name()))
        return;
    for (BitTorrent::TorrentHandle *const torrent : torrents)
        BitTorrent::Session::instance()->deleteTorrent(torrent->hash(), deleteLocalFiles);
}

void TransferListWidget::deleteVisibleTorrents()
{
    if (m_sortFilterModel->rowCount() <= 0) return;

    QList<BitTorrent::TorrentHandle *> torrents;
    for (int i = 0; i < m_sortFilterModel->rowCount(); ++i)
        torrents << m_listModel->torrentHandle(mapToSource(m_sortFilterModel->index(i, 0)));

    bool deleteLocalFiles = false;
    if (Preferences::instance()->confirmTorrentDeletion()
        && !DeletionConfirmationDialog::askForDeletionConfirmation(this, deleteLocalFiles, torrents.size(), torrents[0]->name()))
        return;

    for (BitTorrent::TorrentHandle *const torrent : asConst(torrents))
        BitTorrent::Session::instance()->deleteTorrent(torrent->hash(), deleteLocalFiles);
}

void TransferListWidget::increasePrioSelectedTorrents()
{
    qDebug() << Q_FUNC_INFO;
    if (m_mainWindow->currentTabWidget() == this)
        BitTorrent::Session::instance()->increaseTorrentsPriority(extractHashes(getSelectedTorrents()));
}

void TransferListWidget::decreasePrioSelectedTorrents()
{
    qDebug() << Q_FUNC_INFO;
    if (m_mainWindow->currentTabWidget() == this)
        BitTorrent::Session::instance()->decreaseTorrentsPriority(extractHashes(getSelectedTorrents()));
}

void TransferListWidget::topPrioSelectedTorrents()
{
    if (m_mainWindow->currentTabWidget() == this)
        BitTorrent::Session::instance()->topTorrentsPriority(extractHashes(getSelectedTorrents()));
}

void TransferListWidget::bottomPrioSelectedTorrents()
{
    if (m_mainWindow->currentTabWidget() == this)
        BitTorrent::Session::instance()->bottomTorrentsPriority(extractHashes(getSelectedTorrents()));
}

void TransferListWidget::copySelectedMagnetURIs() const
{
    QStringList magnetUris;
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        magnetUris << torrent->toMagnetUri();

    qApp->clipboard()->setText(magnetUris.join('\n'));
}

void TransferListWidget::copySelectedNames() const
{
    QStringList torrentNames;
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrentNames << torrent->name();

    qApp->clipboard()->setText(torrentNames.join('\n'));
}

void TransferListWidget::copySelectedHashes() const
{
    QStringList torrentHashes;
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrentHashes << torrent->hash();

    qApp->clipboard()->setText(torrentHashes.join('\n'));
}

void TransferListWidget::hidePriorityColumn(bool hide)
{
    qDebug("hidePriorityColumn(%d)", hide);
    setColumnHidden(TransferListModel::TR_PRIORITY, hide);
    if (!hide && !columnWidth(TransferListModel::TR_PRIORITY))
        resizeColumnToContents(TransferListModel::TR_PRIORITY);
}

void TransferListWidget::openSelectedTorrentsFolder() const
{
    QSet<QString> pathsList;
#ifdef Q_OS_MAC
    // On macOS you expect both the files and folders to be opened in their parent
    // folders prehilighted for opening, so we use a custom method.
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents())) {
        QString path = torrent->contentPath(true);
        pathsList.insert(path);
    }
    MacUtils::openFiles(pathsList);
#else
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents())) {
        QString path = torrent->contentPath(true);
        if (!pathsList.contains(path)) {
            if (torrent->filesCount() == 1)
                Utils::Misc::openFolderSelect(path);
            else
                Utils::Misc::openPath(path);
        }
        pathsList.insert(path);
    }
#endif // Q_OS_MAC
}

void TransferListWidget::previewSelectedTorrents()
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents())) {
        if (torrent->hasMetadata())
            new PreviewSelectDialog(this, torrent);
    }
}

void TransferListWidget::setDlLimitSelectedTorrents()
{
    QList<BitTorrent::TorrentHandle *> torrentsList;
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents())) {
        if (torrent->isSeed())
            continue;
        torrentsList += torrent;
    }
    if (torrentsList.empty()) return;

    int oldLimit = torrentsList.first()->downloadLimit();
    for (BitTorrent::TorrentHandle *const torrent : asConst(torrentsList)) {
        if (torrent->downloadLimit() != oldLimit) {
            oldLimit = -1;
            break;
        }
    }

    bool ok = false;
    const long newLimit = SpeedLimitDialog::askSpeedLimit(
                this, &ok, tr("Torrent Download Speed Limiting"), oldLimit
                , BitTorrent::Session::instance()->globalDownloadSpeedLimit());
    if (!ok) return;

    for (BitTorrent::TorrentHandle *const torrent : asConst(torrentsList)) {
        qDebug("Applying download speed limit of %ld Kb/s to torrent %s", (newLimit / 1024l), qUtf8Printable(torrent->hash()));
        torrent->setDownloadLimit(newLimit);
    }
}

void TransferListWidget::setUpLimitSelectedTorrents()
{
    QList<BitTorrent::TorrentHandle *> torrentsList = getSelectedTorrents();
    if (torrentsList.empty()) return;

    int oldLimit = torrentsList.first()->uploadLimit();
    for (BitTorrent::TorrentHandle *const torrent : asConst(torrentsList)) {
        if (torrent->uploadLimit() != oldLimit) {
            oldLimit = -1;
            break;
        }
    }

    bool ok = false;
    const long newLimit = SpeedLimitDialog::askSpeedLimit(
                this, &ok, tr("Torrent Upload Speed Limiting"), oldLimit
                , BitTorrent::Session::instance()->globalUploadSpeedLimit());
    if (!ok) return;

    for (BitTorrent::TorrentHandle *const torrent : asConst(torrentsList)) {
        qDebug("Applying upload speed limit of %ld Kb/s to torrent %s", (newLimit / 1024l), qUtf8Printable(torrent->hash()));
        torrent->setUploadLimit(newLimit);
    }
}

void TransferListWidget::setMaxRatioSelectedTorrents()
{
    const QList<BitTorrent::TorrentHandle *> torrents = getSelectedTorrents();
    if (torrents.isEmpty()) return;

    qreal currentMaxRatio = BitTorrent::Session::instance()->globalMaxRatio();
    if (torrents.count() == 1)
        currentMaxRatio = torrents[0]->maxRatio();

    int currentMaxSeedingTime = BitTorrent::Session::instance()->globalMaxSeedingMinutes();
    if (torrents.count() == 1)
        currentMaxSeedingTime = torrents[0]->maxSeedingTime();

    bool useGlobalValue = true;
    if (torrents.count() == 1)
        useGlobalValue = (torrents[0]->ratioLimit() == BitTorrent::TorrentHandle::USE_GLOBAL_RATIO)
                && (torrents[0]->seedingTimeLimit() == BitTorrent::TorrentHandle::USE_GLOBAL_SEEDING_TIME);

    UpDownRatioDialog dlg(useGlobalValue, currentMaxRatio, BitTorrent::TorrentHandle::MAX_RATIO,
                       currentMaxSeedingTime, BitTorrent::TorrentHandle::MAX_SEEDING_TIME, this);
    if (dlg.exec() != QDialog::Accepted) return;

    for (BitTorrent::TorrentHandle *const torrent : torrents) {
        qreal ratio = (dlg.useDefault() ? BitTorrent::TorrentHandle::USE_GLOBAL_RATIO : dlg.ratio());
        torrent->setRatioLimit(ratio);

        int seedingTime = (dlg.useDefault() ? BitTorrent::TorrentHandle::USE_GLOBAL_SEEDING_TIME : dlg.seedingTime());
        torrent->setSeedingTimeLimit(seedingTime);
    }
}

void TransferListWidget::recheckSelectedTorrents()
{
    if (Preferences::instance()->confirmTorrentRecheck()) {
        QMessageBox::StandardButton ret = QMessageBox::question(this, tr("Recheck confirmation"), tr("Are you sure you want to recheck the selected torrent(s)?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (ret != QMessageBox::Yes) return;
    }

    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrent->forceRecheck();
}

void TransferListWidget::reannounceSelectedTorrents()
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrent->forceReannounce();
}

// hide/show columns menu
void TransferListWidget::displayDLHoSMenu(const QPoint&)
{
    QMenu hideshowColumn(this);
    hideshowColumn.setTitle(tr("Column visibility"));
    QList<QAction*> actions;
    for (int i = 0; i < m_listModel->columnCount(); ++i) {
        if (!BitTorrent::Session::instance()->isQueueingSystemEnabled() && (i == TransferListModel::TR_PRIORITY)) {
            actions.append(nullptr);
            continue;
        }
        QAction *myAct = hideshowColumn.addAction(m_listModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString());
        myAct->setCheckable(true);
        myAct->setChecked(!isColumnHidden(i));
        actions.append(myAct);
    }
    int visibleCols = 0;
    for (int i = 0; i < TransferListModel::NB_COLUMNS; ++i) {
        if (!isColumnHidden(i))
            ++visibleCols;

        if (visibleCols > 1)
            break;
    }

    // Call menu
    QAction *act = hideshowColumn.exec(QCursor::pos());
    if (act) {
        int col = actions.indexOf(act);
        Q_ASSERT(col >= 0);
        Q_ASSERT(visibleCols > 0);
        if (!isColumnHidden(col) && visibleCols == 1)
            return;
        setColumnHidden(col, !isColumnHidden(col));
        if (!isColumnHidden(col) && columnWidth(col) <= 5)
            resizeColumnToContents(col);
        saveSettings();
    }
}

void TransferListWidget::toggleSelectedTorrentsSuperSeeding() const
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents())) {
        if (torrent->hasMetadata())
            torrent->setSuperSeeding(!torrent->superSeeding());
    }
}

void TransferListWidget::toggleSelectedTorrentsSequentialDownload() const
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrent->toggleSequentialDownload();
}

void TransferListWidget::toggleSelectedFirstLastPiecePrio() const
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrent->toggleFirstLastPiecePriority();
}

void TransferListWidget::setSelectedAutoTMMEnabled(bool enabled) const
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrent->setAutoTMMEnabled(enabled);
}

void TransferListWidget::askNewCategoryForSelection()
{
    const QString newCategoryName = TorrentCategoryDialog::createCategory(this);
    if (!newCategoryName.isEmpty())
        setSelectionCategory(newCategoryName);
}

void TransferListWidget::askAddTagsForSelection()
{
    const QStringList tags = askTagsForSelection(tr("Add Tags"));
    for (const QString &tag : tags)
        addSelectionTag(tag);
}

void TransferListWidget::confirmRemoveAllTagsForSelection()
{
    QMessageBox::StandardButton response = QMessageBox::question(
        this, tr("Remove All Tags"), tr("Remove all tags from selected torrents?"),
        QMessageBox::Yes | QMessageBox::No);
    if (response == QMessageBox::Yes)
        clearSelectionTags();
}

QStringList TransferListWidget::askTagsForSelection(const QString &dialogTitle)
{
    QStringList tags;
    bool invalid = true;
    while (invalid) {
        bool ok = false;
        invalid = false;
        const QString tagsInput = AutoExpandableDialog::getText(
            this, dialogTitle, tr("Comma-separated tags:"), QLineEdit::Normal, "", &ok).trimmed();
        if (!ok || tagsInput.isEmpty())
            return QStringList();
        tags = tagsInput.split(',', QString::SkipEmptyParts);
        for (QString &tag : tags) {
            tag = tag.trimmed();
            if (!BitTorrent::Session::isValidTag(tag)) {
                QMessageBox::warning(this, tr("Invalid tag")
                                     , tr("Tag name: '%1' is invalid").arg(tag));
                invalid = true;
            }
        }
    }
    return tags;
}

void TransferListWidget::applyToSelectedTorrents(const std::function<void (BitTorrent::TorrentHandle *const)> &fn)
{
    for (const QModelIndex &index : asConst(selectionModel()->selectedRows())) {
        BitTorrent::TorrentHandle *const torrent = m_listModel->torrentHandle(mapToSource(index));
        Q_ASSERT(torrent);
        fn(torrent);
    }
}

void TransferListWidget::renameSelectedTorrent()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if ((selectedIndexes.size() != 1) || !selectedIndexes.first().isValid()) return;

    const QModelIndex mi = m_listModel->index(mapToSource(selectedIndexes.first()).row(), TransferListModel::TR_NAME);
    BitTorrent::TorrentHandle *const torrent = m_listModel->torrentHandle(mi);
    if (!torrent) return;

    // Ask for a new Name
    bool ok;
    QString name = AutoExpandableDialog::getText(this, tr("Rename"), tr("New name:"), QLineEdit::Normal, torrent->name(), &ok);
    if (ok && !name.isEmpty()) {
        name.replace(QRegularExpression("\r?\n|\r"), " ");
        // Rename the torrent
        m_listModel->setData(mi, name, Qt::DisplayRole);
    }
}

void TransferListWidget::setSelectionCategory(QString category)
{
    for (const QModelIndex &index : asConst(selectionModel()->selectedRows()))
        m_listModel->setData(m_listModel->index(mapToSource(index).row(), TransferListModel::TR_CATEGORY), category, Qt::DisplayRole);
}

void TransferListWidget::addSelectionTag(const QString &tag)
{
    applyToSelectedTorrents([&tag](BitTorrent::TorrentHandle *const torrent) { torrent->addTag(tag); });
}

void TransferListWidget::removeSelectionTag(const QString &tag)
{
    applyToSelectedTorrents([&tag](BitTorrent::TorrentHandle *const torrent) { torrent->removeTag(tag); });
}

void TransferListWidget::clearSelectionTags()
{
    applyToSelectedTorrents([](BitTorrent::TorrentHandle *const torrent) { torrent->removeAllTags(); });
}

void TransferListWidget::displayListMenu(const QPoint&)
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if (selectedIndexes.size() == 0) return;

    // Create actions
    QAction actionStart(GuiIconProvider::instance()->getIcon("media-playback-start"), tr("Resume", "Resume/start the torrent"), nullptr);
    connect(&actionStart, &QAction::triggered, this, &TransferListWidget::startSelectedTorrents);
    QAction actionPause(GuiIconProvider::instance()->getIcon("media-playback-pause"), tr("Pause", "Pause the torrent"), nullptr);
    connect(&actionPause, &QAction::triggered, this, &TransferListWidget::pauseSelectedTorrents);
    QAction actionForceStart(GuiIconProvider::instance()->getIcon("media-seek-forward"), tr("Force Resume", "Force Resume/start the torrent"), nullptr);
    connect(&actionForceStart, &QAction::triggered, this, &TransferListWidget::forceStartSelectedTorrents);
    QAction actionDelete(GuiIconProvider::instance()->getIcon("edit-delete"), tr("Delete", "Delete the torrent"), nullptr);
    connect(&actionDelete, &QAction::triggered, this, &TransferListWidget::softDeleteSelectedTorrents);
    QAction actionPreviewFile(GuiIconProvider::instance()->getIcon("view-preview"), tr("Preview file..."), nullptr);
    connect(&actionPreviewFile, &QAction::triggered, this, &TransferListWidget::previewSelectedTorrents);
    QAction actionSetMaxRatio(QIcon(QLatin1String(":/icons/skin/ratio.svg")), tr("Limit share ratio..."), nullptr);
    connect(&actionSetMaxRatio, &QAction::triggered, this, &TransferListWidget::setMaxRatioSelectedTorrents);
    QAction actionSetUploadLimit(GuiIconProvider::instance()->getIcon("kt-set-max-upload-speed"), tr("Limit upload rate..."), nullptr);
    connect(&actionSetUploadLimit, &QAction::triggered, this, &TransferListWidget::setUpLimitSelectedTorrents);
    QAction actionSetDownloadLimit(GuiIconProvider::instance()->getIcon("kt-set-max-download-speed"), tr("Limit download rate..."), nullptr);
    connect(&actionSetDownloadLimit, &QAction::triggered, this, &TransferListWidget::setDlLimitSelectedTorrents);
    QAction actionOpenDestinationFolder(GuiIconProvider::instance()->getIcon("inode-directory"), tr("Open destination folder"), nullptr);
    connect(&actionOpenDestinationFolder, &QAction::triggered, this, &TransferListWidget::openSelectedTorrentsFolder);
    QAction actionIncreasePriority(GuiIconProvider::instance()->getIcon("go-up"), tr("Move up", "i.e. move up in the queue"), nullptr);
    connect(&actionIncreasePriority, &QAction::triggered, this, &TransferListWidget::increasePrioSelectedTorrents);
    QAction actionDecreasePriority(GuiIconProvider::instance()->getIcon("go-down"), tr("Move down", "i.e. Move down in the queue"), nullptr);
    connect(&actionDecreasePriority, &QAction::triggered, this, &TransferListWidget::decreasePrioSelectedTorrents);
    QAction actionTopPriority(GuiIconProvider::instance()->getIcon("go-top"), tr("Move to top", "i.e. Move to top of the queue"), nullptr);
    connect(&actionTopPriority, &QAction::triggered, this, &TransferListWidget::topPrioSelectedTorrents);
    QAction actionBottomPriority(GuiIconProvider::instance()->getIcon("go-bottom"), tr("Move to bottom", "i.e. Move to bottom of the queue"), nullptr);
    connect(&actionBottomPriority, &QAction::triggered, this, &TransferListWidget::bottomPrioSelectedTorrents);
    QAction actionSetTorrentPath(GuiIconProvider::instance()->getIcon("inode-directory"), tr("Set location..."), nullptr);
    connect(&actionSetTorrentPath, &QAction::triggered, this, &TransferListWidget::setSelectedTorrentsLocation);
    QAction actionForceRecheck(GuiIconProvider::instance()->getIcon("document-edit-verify"), tr("Force recheck"), nullptr);
    connect(&actionForceRecheck, &QAction::triggered, this, &TransferListWidget::recheckSelectedTorrents);
    QAction actionForceReannounce(GuiIconProvider::instance()->getIcon("document-edit-verify"), tr("Force reannounce"), nullptr);
    connect(&actionForceReannounce, &QAction::triggered, this, &TransferListWidget::reannounceSelectedTorrents);
    QAction actionCopyMagnetLink(GuiIconProvider::instance()->getIcon("kt-magnet"), tr("Copy magnet link"), nullptr);
    connect(&actionCopyMagnetLink, &QAction::triggered, this, &TransferListWidget::copySelectedMagnetURIs);
    QAction actionCopyName(GuiIconProvider::instance()->getIcon("edit-copy"), tr("Copy name"), nullptr);
    connect(&actionCopyName, &QAction::triggered, this, &TransferListWidget::copySelectedNames);
    QAction actionCopyHash(GuiIconProvider::instance()->getIcon("edit-copy"), tr("Copy hash"), nullptr);
    connect(&actionCopyHash, &QAction::triggered, this, &TransferListWidget::copySelectedHashes);
    QAction actionSuperSeedingMode(tr("Super seeding mode"), nullptr);
    actionSuperSeedingMode.setCheckable(true);
    connect(&actionSuperSeedingMode, &QAction::triggered, this, &TransferListWidget::toggleSelectedTorrentsSuperSeeding);
    QAction actionRename(GuiIconProvider::instance()->getIcon("edit-rename"), tr("Rename..."), nullptr);
    connect(&actionRename, &QAction::triggered, this, &TransferListWidget::renameSelectedTorrent);
    QAction actionSequentialDownload(tr("Download in sequential order"), nullptr);
    actionSequentialDownload.setCheckable(true);
    connect(&actionSequentialDownload, &QAction::triggered, this, &TransferListWidget::toggleSelectedTorrentsSequentialDownload);
    QAction actionFirstLastPiecePrio(tr("Download first and last pieces first"), nullptr);
    actionFirstLastPiecePrio.setCheckable(true);
    connect(&actionFirstLastPiecePrio, &QAction::triggered, this, &TransferListWidget::toggleSelectedFirstLastPiecePrio);
    QAction actionAutoTMM(tr("Automatic Torrent Management"), nullptr);
    actionAutoTMM.setCheckable(true);
    actionAutoTMM.setToolTip(tr("Automatic mode means that various torrent properties(eg save path) will be decided by the associated category"));
    connect(&actionAutoTMM, &QAction::triggered, this, &TransferListWidget::setSelectedAutoTMMEnabled);
    // End of actions

    // Enable/disable pause/start action given the DL state
    bool needsPause = false, needsStart = false, needsForce = false, needsPreview = false;
    bool allSameSuperSeeding = true;
    bool superSeedingMode = false;
    bool allSameSequentialDownloadMode = true, allSamePrioFirstlast = true;
    bool sequentialDownloadMode = false, prioritizeFirstLast = false;
    bool oneHasMetadata = false, oneNotSeed = false;
    bool allSameCategory = true;
    bool allSameAutoTMM = true;
    bool firstAutoTMM = false;
    QString firstCategory;
    bool first = true;
    QSet<QString> tagsInAny;
    QSet<QString> tagsInAll;

    BitTorrent::TorrentHandle *torrent;
    qDebug("Displaying menu");
    for (const QModelIndex &index : selectedIndexes) {
        // Get the file name
        // Get handle and pause the torrent
        torrent = m_listModel->torrentHandle(mapToSource(index));
        if (!torrent) continue;

        if (firstCategory.isEmpty() && first)
            firstCategory = torrent->category();
        if (firstCategory != torrent->category())
            allSameCategory = false;

        tagsInAny.unite(torrent->tags());

        if (first) {
            firstAutoTMM = torrent->isAutoTMMEnabled();
            tagsInAll = torrent->tags();
        }
        else {
            tagsInAll.intersect(torrent->tags());
        }
        if (firstAutoTMM != torrent->isAutoTMMEnabled())
            allSameAutoTMM = false;

        if (torrent->hasMetadata())
            oneHasMetadata = true;
        if (!torrent->isSeed()) {
            oneNotSeed = true;
            if (first) {
                sequentialDownloadMode = torrent->isSequentialDownload();
                prioritizeFirstLast = torrent->hasFirstLastPiecePriority();
            }
            else {
                if (sequentialDownloadMode != torrent->isSequentialDownload())
                    allSameSequentialDownloadMode = false;
                if (prioritizeFirstLast != torrent->hasFirstLastPiecePriority())
                    allSamePrioFirstlast = false;
            }
        }
        else {
            if (!oneNotSeed && allSameSuperSeeding && torrent->hasMetadata()) {
                if (first)
                    superSeedingMode = torrent->superSeeding();
                else if (superSeedingMode != torrent->superSeeding())
                    allSameSuperSeeding = false;
            }
        }
        if (!torrent->isForced())
            needsForce = true;
        else
            needsStart = true;
        if (torrent->isPaused())
            needsStart = true;
        else
            needsPause = true;
        if (torrent->hasMetadata())
            needsPreview = true;

        first = false;

        if (oneHasMetadata && oneNotSeed && !allSameSequentialDownloadMode
            && !allSamePrioFirstlast && !allSameSuperSeeding && !allSameCategory
            && needsStart && needsForce && needsPause && needsPreview && !allSameAutoTMM) {
            break;
        }
    }
    QMenu listMenu(this);
    if (needsStart)
        listMenu.addAction(&actionStart);
    if (needsPause)
        listMenu.addAction(&actionPause);
    if (needsForce)
        listMenu.addAction(&actionForceStart);
    listMenu.addSeparator();
    listMenu.addAction(&actionDelete);
    listMenu.addSeparator();
    listMenu.addAction(&actionSetTorrentPath);
    if (selectedIndexes.size() == 1)
        listMenu.addAction(&actionRename);
    // Category Menu
    QStringList categories = BitTorrent::Session::instance()->categories().keys();
    std::sort(categories.begin(), categories.end(), Utils::String::naturalLessThan<Qt::CaseInsensitive>);
    QList<QAction*> categoryActions;
    QMenu *categoryMenu = listMenu.addMenu(GuiIconProvider::instance()->getIcon("view-categories"), tr("Category"));
    categoryActions << categoryMenu->addAction(GuiIconProvider::instance()->getIcon("list-add"), tr("New...", "New category..."));
    categoryActions << categoryMenu->addAction(GuiIconProvider::instance()->getIcon("edit-clear"), tr("Reset", "Reset category"));
    categoryMenu->addSeparator();
    for (QString category : asConst(categories)) {
        category.replace('&', "&&");  // avoid '&' becomes accelerator key
        QAction *cat = new QAction(GuiIconProvider::instance()->getIcon("inode-directory"), category, categoryMenu);
        if (allSameCategory && (category == firstCategory)) {
            cat->setCheckable(true);
            cat->setChecked(true);
        }
        categoryMenu->addAction(cat);
        categoryActions << cat;
    }

    // Tag Menu
    QStringList tags(BitTorrent::Session::instance()->tags().toList());
    std::sort(tags.begin(), tags.end(), Utils::String::naturalLessThan<Qt::CaseInsensitive>);
    QList<QAction *> tagsActions;
    QMenu *tagsMenu = listMenu.addMenu(GuiIconProvider::instance()->getIcon("view-categories"), tr("Tags"));
    tagsActions << tagsMenu->addAction(GuiIconProvider::instance()->getIcon("list-add"), tr("Add...", "Add / assign multiple tags..."));
    tagsActions << tagsMenu->addAction(GuiIconProvider::instance()->getIcon("edit-clear"), tr("Remove All", "Remove all tags"));
    tagsMenu->addSeparator();
    for (const QString &tag : asConst(tags)) {
        const Qt::CheckState initialState = tagsInAll.contains(tag) ? Qt::Checked
                                            : tagsInAny.contains(tag) ? Qt::PartiallyChecked
                                            : Qt::Unchecked;

        const ToggleFn onToggle = [this, tag](Qt::CheckState newState)
        {
            Q_ASSERT(newState == Qt::CheckState::Checked || newState == Qt::CheckState::Unchecked);
            if (newState == Qt::CheckState::Checked)
                addSelectionTag(tag);
            else
                removeSelectionTag(tag);
        };

        tagsMenu->addAction(new CheckBoxMenuItem(tag, onToggle, initialState, tagsMenu));
    }

    if (allSameAutoTMM) {
        actionAutoTMM.setChecked(firstAutoTMM);
        listMenu.addAction(&actionAutoTMM);
    }

    listMenu.addSeparator();
    if (oneNotSeed)
        listMenu.addAction(&actionSetDownloadLimit);
    listMenu.addAction(&actionSetUploadLimit);
    listMenu.addAction(&actionSetMaxRatio);
    if (!oneNotSeed && allSameSuperSeeding && oneHasMetadata) {
        actionSuperSeedingMode.setChecked(superSeedingMode);
        listMenu.addAction(&actionSuperSeedingMode);
    }
    listMenu.addSeparator();
    bool addedPreviewAction = false;
    if (needsPreview) {
        listMenu.addAction(&actionPreviewFile);
        addedPreviewAction = true;
    }
    if (oneNotSeed) {
        if (allSameSequentialDownloadMode) {
            actionSequentialDownload.setChecked(sequentialDownloadMode);
            listMenu.addAction(&actionSequentialDownload);
            addedPreviewAction = true;
        }
        if (allSamePrioFirstlast) {
            actionFirstLastPiecePrio.setChecked(prioritizeFirstLast);
            listMenu.addAction(&actionFirstLastPiecePrio);
            addedPreviewAction = true;
        }
    }

    if (addedPreviewAction)
        listMenu.addSeparator();
    if (oneHasMetadata) {
        listMenu.addAction(&actionForceRecheck);
        listMenu.addAction(&actionForceReannounce);
        listMenu.addSeparator();
    }
    listMenu.addAction(&actionOpenDestinationFolder);
    if (BitTorrent::Session::instance()->isQueueingSystemEnabled() && oneNotSeed) {
        listMenu.addSeparator();
        QMenu *prioMenu = listMenu.addMenu(tr("Priority"));
        prioMenu->addAction(&actionTopPriority);
        prioMenu->addAction(&actionIncreasePriority);
        prioMenu->addAction(&actionDecreasePriority);
        prioMenu->addAction(&actionBottomPriority);
    }
    listMenu.addSeparator();
    listMenu.addAction(&actionCopyName);
    listMenu.addAction(&actionCopyHash);
    listMenu.addAction(&actionCopyMagnetLink);
    // Call menu
    QAction *act = nullptr;
    act = listMenu.exec(QCursor::pos());
    if (act) {
        // Parse category & tag actions only (others have slots assigned)
        int i = categoryActions.indexOf(act);
        if (i >= 0) {
            // Category action
            if (i == 0) {
                // New Category
                askNewCategoryForSelection();
            }
            else {
                QString category = "";
                if (i > 1)
                    category = categories.at(i - 2);
                // Update Category
                setSelectionCategory(category);
            }
        }
        i = tagsActions.indexOf(act);
        if (i == 0) {
            askAddTagsForSelection();
        }
        else if (i == 1) {
            if (Preferences::instance()->confirmRemoveAllTags())
                confirmRemoveAllTagsForSelection();
            else
                clearSelectionTags();
        }
    }
}

void TransferListWidget::currentChanged(const QModelIndex &current, const QModelIndex&)
{
    qDebug("CURRENT CHANGED");
    BitTorrent::TorrentHandle *torrent = nullptr;
    if (current.isValid()) {
        torrent = m_listModel->torrentHandle(mapToSource(current));
        // Scroll Fix
        scrollTo(current);
    }
    emit currentTorrentChanged(torrent);
}

void TransferListWidget::applyCategoryFilter(QString category)
{
    if (category.isNull())
        m_sortFilterModel->disableCategoryFilter();
    else
        m_sortFilterModel->setCategoryFilter(category);
}

void TransferListWidget::applyTagFilter(const QString &tag)
{
    if (tag.isNull())
        m_sortFilterModel->disableTagFilter();
    else
        m_sortFilterModel->setTagFilter(tag);
}

void TransferListWidget::applyTrackerFilterAll()
{
    m_sortFilterModel->disableTrackerFilter();
}

void TransferListWidget::applyTrackerFilter(const QStringList &hashes)
{
    m_sortFilterModel->setTrackerFilter(hashes);
}

void TransferListWidget::applyNameFilter(const QString &name)
{
    const QRegExp::PatternSyntax patternSyntax = Preferences::instance()->getRegexAsFilteringPatternForTransferList()
                ? QRegExp::RegExp : QRegExp::WildcardUnix;
    m_sortFilterModel->setFilterRegExp(QRegExp(name, Qt::CaseInsensitive, patternSyntax));
}

void TransferListWidget::applyStatusFilter(int f)
{
    m_sortFilterModel->setStatusFilter(static_cast<TorrentFilter::Type>(f));
    // Select first item if nothing is selected
    if (selectionModel()->selectedRows(0).empty() && (m_sortFilterModel->rowCount() > 0)) {
        qDebug("Nothing is selected, selecting first row: %s", qUtf8Printable(m_sortFilterModel->index(0, TransferListModel::TR_NAME).data().toString()));
        selectionModel()->setCurrentIndex(m_sortFilterModel->index(0, TransferListModel::TR_NAME), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    }
}

void TransferListWidget::saveSettings()
{
    Preferences::instance()->setTransHeaderState(header()->saveState());
}

bool TransferListWidget::loadSettings()
{
    return header()->restoreState(Preferences::instance()->getTransHeaderState());
}

void TransferListWidget::wheelEvent(QWheelEvent *event)
{
    event->accept();

    if (event->modifiers() & Qt::ShiftModifier) {
        // Shift + scroll = horizontal scroll
        QWheelEvent scrollHEvent(event->pos(), event->globalPos(), event->delta(), event->buttons(), event->modifiers(), Qt::Horizontal);
        QTreeView::wheelEvent(&scrollHEvent);
        return;
    }

    QTreeView::wheelEvent(event);  // event delegated to base class
}
