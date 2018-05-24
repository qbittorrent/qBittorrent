/*
 * Bittorrent Client using Qt4 and libtorrent.
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
#include "base/logger.h"
#include "base/preferences.h"
#include "base/torrentfilter.h"
#include "base/utils/fs.h"
#include "base/utils/string.h"
#include "autoexpandabledialog.h"
#include "deletionconfirmationdlg.h"
#include "guiiconprovider.h"
#include "mainwindow.h"
#include "optionsdlg.h"
#include "previewselectdialog.h"
#include "speedlimitdlg.h"
#include "torrentcategorydialog.h"
#include "torrentmodel.h"
#include "transferlistdelegate.h"
#include "transferlistsortmodel.h"
#include "updownratiodlg.h"

#ifdef Q_OS_MAC
#include "macutilities.h"
#endif

namespace
{
    using ToggleFn = std::function<void (Qt::CheckState)>;

    QStringList extractHashes(const QList<BitTorrent::TorrentHandle *> &torrents)
    {
        QStringList hashes;
        foreach (BitTorrent::TorrentHandle *const torrent, torrents)
            hashes << torrent->hash();

        return hashes;
    }

    // Helper for setting style parameters when painting check box primitives.
    class CheckBoxIconHelper: public QCheckBox
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
    class MenuCheckBox: public QWidget
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

    class CheckBoxMenuItem: public QWidgetAction
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
    bool column_loaded = loadSettings();

    // Create and apply delegate
    m_listDelegate = new TransferListDelegate(this);
    setItemDelegate(m_listDelegate);

    // Create transfer list model
    m_listModel = new TorrentModel(this);

    m_sortFilterModel = new TransferListSortModel();
    m_sortFilterModel->setDynamicSortFilter(true);
    m_sortFilterModel->setSourceModel(m_listModel);
    m_sortFilterModel->setFilterKeyColumn(TorrentModel::TR_NAME);
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
    if (!column_loaded) {
        setColumnHidden(TorrentModel::TR_ADD_DATE, true);
        setColumnHidden(TorrentModel::TR_SEED_DATE, true);
        setColumnHidden(TorrentModel::TR_UPLIMIT, true);
        setColumnHidden(TorrentModel::TR_DLLIMIT, true);
        setColumnHidden(TorrentModel::TR_TRACKER, true);
        setColumnHidden(TorrentModel::TR_AMOUNT_DOWNLOADED, true);
        setColumnHidden(TorrentModel::TR_AMOUNT_UPLOADED, true);
        setColumnHidden(TorrentModel::TR_AMOUNT_DOWNLOADED_SESSION, true);
        setColumnHidden(TorrentModel::TR_AMOUNT_UPLOADED_SESSION, true);
        setColumnHidden(TorrentModel::TR_AMOUNT_LEFT, true);
        setColumnHidden(TorrentModel::TR_TIME_ELAPSED, true);
        setColumnHidden(TorrentModel::TR_SAVE_PATH, true);
        setColumnHidden(TorrentModel::TR_COMPLETED, true);
        setColumnHidden(TorrentModel::TR_RATIO_LIMIT, true);
        setColumnHidden(TorrentModel::TR_SEEN_COMPLETE_DATE, true);
        setColumnHidden(TorrentModel::TR_LAST_ACTIVITY, true);
        setColumnHidden(TorrentModel::TR_TOTAL_SIZE, true);
    }

    //Ensure that at least one column is visible at all times
    bool atLeastOne = false;
    for (unsigned int i = 0; i<TorrentModel::NB_COLUMNS; i++) {
        if (!isColumnHidden(i)) {
            atLeastOne = true;
            break;
        }
    }
    if (!atLeastOne)
        setColumnHidden(TorrentModel::TR_NAME, false);

    //When adding/removing columns between versions some may
    //end up being size 0 when the new version is launched with
    //a conf file from the previous version.
    for (unsigned int i = 0; i<TorrentModel::NB_COLUMNS; i++)
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

TorrentModel* TransferListWidget::getSourceModel() const
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

    switch(action) {
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
    foreach (const QModelIndex &index, selectionModel()->selectedRows())
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
    foreach (BitTorrent::TorrentHandle *const torrent, torrents) {
        Logger::instance()->addMessage(tr("Set location: moving \"%1\", from \"%2\" to \"%3\""
            , "Set location: moving \"ubuntu_16_04.iso\", from \"/home/dir1\" to \"/home/dir2\"")
            .arg(torrent->name(), Utils::Fs::toNativePath(torrent->savePath())
                , Utils::Fs::toNativePath(newLocation)));
        torrent->move(Utils::Fs::expandPathAbs(newLocation));
    }
}

void TransferListWidget::pauseAllTorrents()
{
    foreach (BitTorrent::TorrentHandle *const torrent, BitTorrent::Session::instance()->torrents())
        torrent->pause();
}

void TransferListWidget::resumeAllTorrents()
{
    foreach (BitTorrent::TorrentHandle *const torrent, BitTorrent::Session::instance()->torrents())
        torrent->resume();
}

void TransferListWidget::startSelectedTorrents()
{
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
        torrent->resume();
}

void TransferListWidget::forceStartSelectedTorrents()
{
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
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
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
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
        && !DeletionConfirmationDlg::askForDeletionConfirmation(this, deleteLocalFiles, torrents.size(), torrents[0]->name()))
        return;
    foreach (BitTorrent::TorrentHandle *const torrent, torrents)
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
        && !DeletionConfirmationDlg::askForDeletionConfirmation(this, deleteLocalFiles, torrents.size(), torrents[0]->name()))
        return;

    foreach (BitTorrent::TorrentHandle *const torrent, torrents)
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
    QStringList magnet_uris;
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
        magnet_uris << torrent->toMagnetUri();

    qApp->clipboard()->setText(magnet_uris.join("\n"));
}

void TransferListWidget::copySelectedNames() const
{
    QStringList torrent_names;
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
        torrent_names << torrent->name();

    qApp->clipboard()->setText(torrent_names.join("\n"));
}

void TransferListWidget::copySelectedHashes() const
{
    QStringList torrentHashes;
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
        torrentHashes << torrent->hash();

    qApp->clipboard()->setText(torrentHashes.join('\n'));
}

void TransferListWidget::hidePriorityColumn(bool hide)
{
    qDebug("hidePriorityColumn(%d)", hide);
    setColumnHidden(TorrentModel::TR_PRIORITY, hide);
    if (!hide && !columnWidth(TorrentModel::TR_PRIORITY))
        resizeColumnToContents(TorrentModel::TR_PRIORITY);
}

void TransferListWidget::openSelectedTorrentsFolder() const
{
    QSet<QString> pathsList;
#ifdef Q_OS_MAC
    // On macOS you expect both the files and folders to be opened in their parent
    // folders prehilighted for opening, so we use a custom method.
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents()) {
        QString path = torrent->contentPath(true);
        pathsList.insert(path);
    }
    MacUtils::openFiles(pathsList);
#else
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents()) {
        QString path = torrent->contentPath(true);
        if (!pathsList.contains(path)) {
            if (torrent->filesCount() == 1)
                Utils::Misc::openFolderSelect(path);
            else
                Utils::Misc::openPath(path);
        }
        pathsList.insert(path);
    }
#endif
}

void TransferListWidget::previewSelectedTorrents()
{
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents()) {
        if (torrent->hasMetadata())
            new PreviewSelectDialog(this, torrent);
    }
}

void TransferListWidget::setDlLimitSelectedTorrents()
{
    QList<BitTorrent::TorrentHandle *> TorrentsList;
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents()) {
        if (torrent->isSeed())
            continue;
        TorrentsList += torrent;
    }
    if (TorrentsList.empty()) return;

    int oldLimit = TorrentsList.first()->downloadLimit();
    foreach (BitTorrent::TorrentHandle *const torrent, TorrentsList) {
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

    foreach (BitTorrent::TorrentHandle *const torrent, TorrentsList) {
        qDebug("Applying download speed limit of %ld Kb/s to torrent %s", (newLimit / 1024l), qUtf8Printable(torrent->hash()));
        torrent->setDownloadLimit(newLimit);
    }
}

void TransferListWidget::setUpLimitSelectedTorrents()
{
    QList<BitTorrent::TorrentHandle *> TorrentsList = getSelectedTorrents();
    if (TorrentsList.empty()) return;

    int oldLimit = TorrentsList.first()->uploadLimit();
    foreach (BitTorrent::TorrentHandle *const torrent, TorrentsList) {
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

    foreach (BitTorrent::TorrentHandle *const torrent, TorrentsList) {
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

    UpDownRatioDlg dlg(useGlobalValue, currentMaxRatio, BitTorrent::TorrentHandle::MAX_RATIO,
                       currentMaxSeedingTime, BitTorrent::TorrentHandle::MAX_SEEDING_TIME, this);
    if (dlg.exec() != QDialog::Accepted) return;

    foreach (BitTorrent::TorrentHandle *const torrent, torrents) {
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

    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
        torrent->forceRecheck();
}

void TransferListWidget::reannounceSelectedTorrents()
{
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
        torrent->forceReannounce();
}

// hide/show columns menu
void TransferListWidget::displayDLHoSMenu(const QPoint&)
{
    QMenu hideshowColumn(this);
    hideshowColumn.setTitle(tr("Column visibility"));
    QList<QAction*> actions;
    for (int i = 0; i < m_listModel->columnCount(); ++i) {
        if (!BitTorrent::Session::instance()->isQueueingSystemEnabled() && i == TorrentModel::TR_PRIORITY) {
            actions.append(0);
            continue;
        }
        QAction *myAct = hideshowColumn.addAction(m_listModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString());
        myAct->setCheckable(true);
        myAct->setChecked(!isColumnHidden(i));
        actions.append(myAct);
    }
    int visibleCols = 0;
    for (unsigned int i = 0; i<TorrentModel::NB_COLUMNS; i++) {
        if (!isColumnHidden(i))
            visibleCols++;

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
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents()) {
        if (torrent->hasMetadata())
            torrent->setSuperSeeding(!torrent->superSeeding());
    }
}

void TransferListWidget::toggleSelectedTorrentsSequentialDownload() const
{
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
        torrent->toggleSequentialDownload();
}

void TransferListWidget::toggleSelectedFirstLastPiecePrio() const
{
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
        torrent->toggleFirstLastPiecePriority();
}

void TransferListWidget::setSelectedAutoTMMEnabled(bool enabled) const
{
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
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
    foreach (const QString &tag, tags)
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
    foreach (const QModelIndex &index, selectionModel()->selectedRows()) {
        BitTorrent::TorrentHandle *const torrent = m_listModel->torrentHandle(mapToSource(index));
        Q_ASSERT(torrent);
        fn(torrent);
    }
}

void TransferListWidget::renameSelectedTorrent()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if ((selectedIndexes.size() != 1) || !selectedIndexes.first().isValid()) return;

    const QModelIndex mi = m_listModel->index(mapToSource(selectedIndexes.first()).row(), TorrentModel::TR_NAME);
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
    foreach (const QModelIndex &index, selectionModel()->selectedRows())
        m_listModel->setData(m_listModel->index(mapToSource(index).row(), TorrentModel::TR_CATEGORY), category, Qt::DisplayRole);
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
    QModelIndexList selectedIndexes = selectionModel()->selectedRows();
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
    QAction actionPreview_file(GuiIconProvider::instance()->getIcon("view-preview"), tr("Preview file..."), nullptr);
    connect(&actionPreview_file, &QAction::triggered, this, &TransferListWidget::previewSelectedTorrents);
    QAction actionSet_max_ratio(QIcon(QLatin1String(":/icons/skin/ratio.png")), tr("Limit share ratio..."), nullptr);
    connect(&actionSet_max_ratio, &QAction::triggered, this, &TransferListWidget::setMaxRatioSelectedTorrents);
    QAction actionSet_upload_limit(GuiIconProvider::instance()->getIcon("kt-set-max-upload-speed"), tr("Limit upload rate..."), nullptr);
    connect(&actionSet_upload_limit, &QAction::triggered, this, &TransferListWidget::setUpLimitSelectedTorrents);
    QAction actionSet_download_limit(GuiIconProvider::instance()->getIcon("kt-set-max-download-speed"), tr("Limit download rate..."), nullptr);
    connect(&actionSet_download_limit, &QAction::triggered, this, &TransferListWidget::setDlLimitSelectedTorrents);
    QAction actionOpen_destination_folder(GuiIconProvider::instance()->getIcon("inode-directory"), tr("Open destination folder"), nullptr);
    connect(&actionOpen_destination_folder, &QAction::triggered, this, &TransferListWidget::openSelectedTorrentsFolder);
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
    QAction actionForce_recheck(GuiIconProvider::instance()->getIcon("document-edit-verify"), tr("Force recheck"), nullptr);
    connect(&actionForce_recheck, &QAction::triggered, this, &TransferListWidget::recheckSelectedTorrents);
    QAction actionForce_reannounce(GuiIconProvider::instance()->getIcon("document-edit-verify"), tr("Force reannounce"), nullptr);
    connect(&actionForce_reannounce, &QAction::triggered, this, &TransferListWidget::reannounceSelectedTorrents);
    QAction actionCopy_magnet_link(GuiIconProvider::instance()->getIcon("kt-magnet"), tr("Copy magnet link"), nullptr);
    connect(&actionCopy_magnet_link, &QAction::triggered, this, &TransferListWidget::copySelectedMagnetURIs);
    QAction actionCopy_name(GuiIconProvider::instance()->getIcon("edit-copy"), tr("Copy name"), nullptr);
    connect(&actionCopy_name, &QAction::triggered, this, &TransferListWidget::copySelectedNames);
    QAction actionCopyHash(GuiIconProvider::instance()->getIcon("edit-copy"), tr("Copy hash"), nullptr);
    connect(&actionCopyHash, &QAction::triggered, this, &TransferListWidget::copySelectedHashes);
    QAction actionSuper_seeding_mode(tr("Super seeding mode"), nullptr);
    actionSuper_seeding_mode.setCheckable(true);
    connect(&actionSuper_seeding_mode, &QAction::triggered, this, &TransferListWidget::toggleSelectedTorrentsSuperSeeding);
    QAction actionRename(GuiIconProvider::instance()->getIcon("edit-rename"), tr("Rename..."), nullptr);
    connect(&actionRename, &QAction::triggered, this, &TransferListWidget::renameSelectedTorrent);
    QAction actionSequential_download(tr("Download in sequential order"), nullptr);
    actionSequential_download.setCheckable(true);
    connect(&actionSequential_download, &QAction::triggered, this, &TransferListWidget::toggleSelectedTorrentsSequentialDownload);
    QAction actionFirstLastPiece_prio(tr("Download first and last pieces first"), nullptr);
    actionFirstLastPiece_prio.setCheckable(true);
    connect(&actionFirstLastPiece_prio, &QAction::triggered, this, &TransferListWidget::toggleSelectedFirstLastPiecePrio);
    QAction actionAutoTMM(tr("Automatic Torrent Management"), nullptr);
    actionAutoTMM.setCheckable(true);
    actionAutoTMM.setToolTip(tr("Automatic mode means that various torrent properties(eg save path) will be decided by the associated category"));
    connect(&actionAutoTMM, &QAction::triggered, this, &TransferListWidget::setSelectedAutoTMMEnabled);
    // End of actions

    // Enable/disable pause/start action given the DL state
    bool needs_pause = false, needs_start = false, needs_force = false, needs_preview = false;
    bool all_same_super_seeding = true;
    bool super_seeding_mode = false;
    bool all_same_sequential_download_mode = true, all_same_prio_firstlast = true;
    bool sequential_download_mode = false, prioritize_first_last = false;
    bool one_has_metadata = false, one_not_seed = false;
    bool allSameCategory = true;
    bool allSameAutoTMM = true;
    bool firstAutoTMM = false;
    QString firstCategory;
    bool first = true;
    QSet<QString> tagsInAny;
    QSet<QString> tagsInAll;

    BitTorrent::TorrentHandle *torrent;
    qDebug("Displaying menu");
    foreach (const QModelIndex &index, selectedIndexes) {
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
            one_has_metadata = true;
        if (!torrent->isSeed()) {
            one_not_seed = true;
            if (torrent->hasMetadata()) {
                if (first) {
                    sequential_download_mode = torrent->isSequentialDownload();
                    prioritize_first_last = torrent->hasFirstLastPiecePriority();
                }
                else {
                    if (sequential_download_mode != torrent->isSequentialDownload())
                        all_same_sequential_download_mode = false;
                    if (prioritize_first_last != torrent->hasFirstLastPiecePriority())
                        all_same_prio_firstlast = false;
                }
            }
        }
        else {
            if (!one_not_seed && all_same_super_seeding && torrent->hasMetadata()) {
                if (first) {
                    super_seeding_mode = torrent->superSeeding();
                }
                else if (super_seeding_mode != torrent->superSeeding())
                    all_same_super_seeding = false;

            }
        }
        if (!torrent->isForced())
            needs_force = true;
        else
            needs_start = true;
        if (torrent->isPaused())
            needs_start = true;
        else
            needs_pause = true;
        if (torrent->hasMetadata())
            needs_preview = true;

        first = false;

        if (one_has_metadata && one_not_seed && !all_same_sequential_download_mode
            && !all_same_prio_firstlast && !all_same_super_seeding && !allSameCategory
            && needs_start && needs_force && needs_pause && needs_preview && !allSameAutoTMM) {
            break;
        }
    }
    QMenu listMenu(this);
    if (needs_start)
        listMenu.addAction(&actionStart);
    if (needs_pause)
        listMenu.addAction(&actionPause);
    if (needs_force)
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
    foreach (QString category, categories) {
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
    foreach (QString tag, tags) {
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
    if (one_not_seed)
        listMenu.addAction(&actionSet_download_limit);
    listMenu.addAction(&actionSet_upload_limit);
    listMenu.addAction(&actionSet_max_ratio);
    if (!one_not_seed && all_same_super_seeding && one_has_metadata) {
        actionSuper_seeding_mode.setChecked(super_seeding_mode);
        listMenu.addAction(&actionSuper_seeding_mode);
    }
    listMenu.addSeparator();
    bool added_preview_action = false;
    if (needs_preview) {
        listMenu.addAction(&actionPreview_file);
        added_preview_action = true;
    }
    if (one_not_seed && one_has_metadata) {
        if (all_same_sequential_download_mode) {
            actionSequential_download.setChecked(sequential_download_mode);
            listMenu.addAction(&actionSequential_download);
            added_preview_action = true;
        }
        if (all_same_prio_firstlast) {
            actionFirstLastPiece_prio.setChecked(prioritize_first_last);
            listMenu.addAction(&actionFirstLastPiece_prio);
            added_preview_action = true;
        }
    }

    if (added_preview_action)
        listMenu.addSeparator();
    if (one_has_metadata) {
        listMenu.addAction(&actionForce_recheck);
        listMenu.addAction(&actionForce_reannounce);
        listMenu.addSeparator();
    }
    listMenu.addAction(&actionOpen_destination_folder);
    if (BitTorrent::Session::instance()->isQueueingSystemEnabled() && one_not_seed) {
        listMenu.addSeparator();
        QMenu *prioMenu = listMenu.addMenu(tr("Priority"));
        prioMenu->addAction(&actionTopPriority);
        prioMenu->addAction(&actionIncreasePriority);
        prioMenu->addAction(&actionDecreasePriority);
        prioMenu->addAction(&actionBottomPriority);
    }
    listMenu.addSeparator();
    listMenu.addAction(&actionCopy_name);
    listMenu.addAction(&actionCopyHash);
    listMenu.addAction(&actionCopy_magnet_link);
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

void TransferListWidget::currentChanged(const QModelIndex& current, const QModelIndex&)
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

void TransferListWidget::applyNameFilter(const QString& name)
{
    m_sortFilterModel->setFilterRegExp(QRegExp(name, Qt::CaseInsensitive, QRegExp::WildcardUnix));
}

void TransferListWidget::applyStatusFilter(int f)
{
    m_sortFilterModel->setStatusFilter(static_cast<TorrentFilter::Type>(f));
    // Select first item if nothing is selected
    if (selectionModel()->selectedRows(0).empty() && m_sortFilterModel->rowCount() > 0) {
        qDebug("Nothing is selected, selecting first row: %s", qUtf8Printable(m_sortFilterModel->index(0, TorrentModel::TR_NAME).data().toString()));
        selectionModel()->setCurrentIndex(m_sortFilterModel->index(0, TorrentModel::TR_NAME), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
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
