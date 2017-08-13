/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#include "transferlistwidget.h"

#include <QClipboard>
#include <QDebug>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QStylePainter>
#include <QRegExp>
#include <QShortcut>
#include <QTableView>
#include <QWheelEvent>
#include <QWidgetAction>

#include "autoexpandabledialog.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/torrentfilter.h"
#include "base/utils/fs.h"
#include "base/utils/string.h"
#include "deletionconfirmationdlg.h"
#include "guiiconprovider.h"
#include "mainwindow.h"
#include "optionsdlg.h"
#include "previewselect.h"
#include "speedlimitdlg.h"
#include "torrentmodel.h"
#include "transferlistdelegate.h"
#include "transferlistsortmodel.h"
#include "updownratiodlg.h"

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
        connect(m_checkBox, &QCheckBox::stateChanged, [this, onToggle](int newState)
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

TransferListWidget::TransferListWidget(QWidget *parent, MainWindow *main_window)
    : QTreeView(parent)
    , main_window(main_window)
{

    setUniformRowHeights(true);
    // Load settings
    bool column_loaded = loadSettings();

    // Create and apply delegate
    listDelegate = new TransferListDelegate(this);
    setItemDelegate(listDelegate);

    // Create transfer list model
    listModel = new TorrentModel(this);

    nameFilterModel = new TransferListSortModel();
    nameFilterModel->setDynamicSortFilter(true);
    nameFilterModel->setSourceModel(listModel);
    nameFilterModel->setFilterKeyColumn(TorrentModel::TR_NAME);
    nameFilterModel->setFilterRole(Qt::DisplayRole);
    nameFilterModel->setSortCaseSensitivity(Qt::CaseInsensitive);

    setModel(nameFilterModel);

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
    connect(this, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(torrentDoubleClicked()));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(displayListMenu(const QPoint &)));
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(displayDLHoSMenu(const QPoint &)));
    connect(header(), SIGNAL(sectionMoved(int, int, int)), this, SLOT(saveSettings()));
    connect(header(), SIGNAL(sectionResized(int, int, int)), this, SLOT(saveSettings()));
    connect(header(), SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), this, SLOT(saveSettings()));

    editHotkey = new QShortcut(Qt::Key_F2, this, SLOT(renameSelectedTorrent()), 0, Qt::WidgetShortcut);
    deleteHotkey = new QShortcut(QKeySequence::Delete, this, SLOT(softDeleteSelectedTorrents()), 0, Qt::WidgetShortcut);
    permDeleteHotkey = new QShortcut(Qt::SHIFT + Qt::Key_Delete, this, SLOT(permDeleteSelectedTorrents()), 0, Qt::WidgetShortcut);
    doubleClickHotkey = new QShortcut(Qt::Key_Return, this, SLOT(torrentDoubleClicked()), 0, Qt::WidgetShortcut);
    recheckHotkey = new QShortcut(Qt::CTRL + Qt::Key_R, this, SLOT(recheckSelectedTorrents()), 0, Qt::WidgetShortcut);

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
    delete nameFilterModel;
    delete listModel;
    delete listDelegate;
    qDebug() << Q_FUNC_INFO << "EXIT";
}

TorrentModel* TransferListWidget::getSourceModel() const
{
    return listModel;
}

void TransferListWidget::previewFile(QString filePath)
{
    Utils::Misc::openPath(filePath);
}

inline QModelIndex TransferListWidget::mapToSource(const QModelIndex &index) const
{
    Q_ASSERT(index.isValid());
    if (index.model() == nameFilterModel)
        return nameFilterModel->mapToSource(index);
    return index;
}

inline QModelIndex TransferListWidget::mapFromSource(const QModelIndex &index) const
{
    Q_ASSERT(index.isValid());
    Q_ASSERT(index.model() == nameFilterModel);
    return nameFilterModel->mapFromSource(index);
}

void TransferListWidget::torrentDoubleClicked()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if ((selectedIndexes.size() != 1) || !selectedIndexes.first().isValid()) return;

    const QModelIndex index = listModel->index(mapToSource(selectedIndexes.first()).row());
    BitTorrent::TorrentHandle *const torrent = listModel->torrentHandle(index);
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
        if (torrent->filesCount() == 1)
            Utils::Misc::openFolderSelect(torrent->contentPath(true));
        else
            Utils::Misc::openPath(torrent->contentPath(true));
        break;
    }
}

QList<BitTorrent::TorrentHandle *> TransferListWidget::getSelectedTorrents() const
{
    QList<BitTorrent::TorrentHandle *> torrents;
    foreach (const QModelIndex &index, selectionModel()->selectedRows())
        torrents << listModel->torrentHandle(mapToSource(index));

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
        Logger::instance()->addMessage(tr("Set location: moving \"%1\", from \"%2\" to \"%3\"", "Set location: moving \"ubuntu_16_04.iso\", from \"/home/dir1\" to \"/home/dir2\"").arg(torrent->name()).arg(torrent->savePath()).arg(newLocation));
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
    for (int i = 0; i < nameFilterModel->rowCount(); ++i) {
        BitTorrent::TorrentHandle *const torrent = listModel->torrentHandle(mapToSource(nameFilterModel->index(i, 0)));
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
    for (int i = 0; i < nameFilterModel->rowCount(); ++i) {
        BitTorrent::TorrentHandle *const torrent = listModel->torrentHandle(mapToSource(nameFilterModel->index(i, 0)));
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
    if (main_window->currentTabWidget() != this) return;

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
    if (nameFilterModel->rowCount() <= 0) return;

    QList<BitTorrent::TorrentHandle *> torrents;
    for (int i = 0; i < nameFilterModel->rowCount(); ++i)
        torrents << listModel->torrentHandle(mapToSource(nameFilterModel->index(i, 0)));

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
    if (main_window->currentTabWidget() == this)
        BitTorrent::Session::instance()->increaseTorrentsPriority(extractHashes(getSelectedTorrents()));
}

void TransferListWidget::decreasePrioSelectedTorrents()
{
    qDebug() << Q_FUNC_INFO;
    if (main_window->currentTabWidget() == this)
        BitTorrent::Session::instance()->decreaseTorrentsPriority(extractHashes(getSelectedTorrents()));
}

void TransferListWidget::topPrioSelectedTorrents()
{
    if (main_window->currentTabWidget() == this)
        BitTorrent::Session::instance()->topTorrentsPriority(extractHashes(getSelectedTorrents()));
}

void TransferListWidget::bottomPrioSelectedTorrents()
{
    if (main_window->currentTabWidget() == this)
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
}

void TransferListWidget::previewSelectedTorrents()
{
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents()) {
        if (torrent->hasMetadata())
            new PreviewSelect(this, torrent);
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

    bool useGlobalValue = true;
    qreal currentMaxRatio = BitTorrent::Session::instance()->globalMaxRatio();
    if (torrents.count() == 1)
        currentMaxRatio = torrents[0]->maxRatio(&useGlobalValue);

    int currentMaxSeedingTime = BitTorrent::Session::instance()->globalMaxSeedingMinutes();
    if (torrents.count() == 1)
        currentMaxSeedingTime = torrents[0]->maxSeedingTime(&useGlobalValue);

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

// hide/show columns menu
void TransferListWidget::displayDLHoSMenu(const QPoint&)
{
    QMenu hideshowColumn(this);
    hideshowColumn.setTitle(tr("Column visibility"));
    QList<QAction*> actions;
    for (int i = 0; i < listModel->columnCount(); ++i) {
        if (!BitTorrent::Session::instance()->isQueueingSystemEnabled() && i == TorrentModel::TR_PRIORITY) {
            actions.append(0);
            continue;
        }
        QAction *myAct = hideshowColumn.addAction(listModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString());
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
        qDebug("Toggling column %d visibility", col);
        setColumnHidden(col, !isColumnHidden(col));
        if (!isColumnHidden(col) && columnWidth(col) <= 5)
            setColumnWidth(col, 100);
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
    // Ask for category
    bool ok;
    bool invalid;
    do {
        invalid = false;
        const QString category = AutoExpandableDialog::getText(this, tr("New Category"), tr("Category:"), QLineEdit::Normal, "", &ok).trimmed();
        if (ok && !category.isEmpty()) {
            if (!BitTorrent::Session::isValidCategoryName(category)) {
                QMessageBox::warning(this, tr("Invalid category name"),
                                     tr("Category name must not contain '\\'.\n"
                                        "Category name must not start/end with '/'.\n"
                                        "Category name must not contain '//' sequence."));
                invalid = true;
            }
            else {
                setSelectionCategory(category);
            }
        }
    } while(invalid);
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
        BitTorrent::TorrentHandle *const torrent = listModel->torrentHandle(mapToSource(index));
        Q_ASSERT(torrent);
        fn(torrent);
    }
}

void TransferListWidget::renameSelectedTorrent()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if ((selectedIndexes.size() != 1) || !selectedIndexes.first().isValid()) return;

    const QModelIndex mi = listModel->index(mapToSource(selectedIndexes.first()).row(), TorrentModel::TR_NAME);
    BitTorrent::TorrentHandle *const torrent = listModel->torrentHandle(mi);
    if (!torrent) return;

    // Ask for a new Name
    bool ok;
    QString name = AutoExpandableDialog::getText(this, tr("Rename"), tr("New name:"), QLineEdit::Normal, torrent->name(), &ok);
    if (ok && !name.isEmpty()) {
        name.replace(QRegExp("\r?\n|\r"), " ");
        // Rename the torrent
        listModel->setData(mi, name, Qt::DisplayRole);
    }
}

void TransferListWidget::setSelectionCategory(QString category)
{
    foreach (const QModelIndex &index, selectionModel()->selectedRows())
        listModel->setData(listModel->index(mapToSource(index).row(), TorrentModel::TR_CATEGORY), category, Qt::DisplayRole);
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
    QAction actionStart(GuiIconProvider::instance()->getIcon("media-playback-start"), tr("Resume", "Resume/start the torrent"), 0);
    connect(&actionStart, SIGNAL(triggered()), this, SLOT(startSelectedTorrents()));
    QAction actionPause(GuiIconProvider::instance()->getIcon("media-playback-pause"), tr("Pause", "Pause the torrent"), 0);
    connect(&actionPause, SIGNAL(triggered()), this, SLOT(pauseSelectedTorrents()));
    QAction actionForceStart(GuiIconProvider::instance()->getIcon("media-seek-forward"), tr("Force Resume", "Force Resume/start the torrent"), 0);
    connect(&actionForceStart, SIGNAL(triggered()), this, SLOT(forceStartSelectedTorrents()));
    QAction actionDelete(GuiIconProvider::instance()->getIcon("edit-delete"), tr("Delete", "Delete the torrent"), 0);
    connect(&actionDelete, SIGNAL(triggered()), this, SLOT(softDeleteSelectedTorrents()));
    QAction actionPreview_file(GuiIconProvider::instance()->getIcon("view-preview"), tr("Preview file..."), 0);
    connect(&actionPreview_file, SIGNAL(triggered()), this, SLOT(previewSelectedTorrents()));
    QAction actionSet_max_ratio(QIcon(QString::fromUtf8(":/icons/skin/ratio.png")), tr("Limit share ratio..."), 0);
    connect(&actionSet_max_ratio, SIGNAL(triggered()), this, SLOT(setMaxRatioSelectedTorrents()));
    QAction actionSet_upload_limit(GuiIconProvider::instance()->getIcon("kt-set-max-upload-speed"), tr("Limit upload rate..."), 0);
    connect(&actionSet_upload_limit, SIGNAL(triggered()), this, SLOT(setUpLimitSelectedTorrents()));
    QAction actionSet_download_limit(GuiIconProvider::instance()->getIcon("kt-set-max-download-speed"), tr("Limit download rate..."), 0);
    connect(&actionSet_download_limit, SIGNAL(triggered()), this, SLOT(setDlLimitSelectedTorrents()));
    QAction actionOpen_destination_folder(GuiIconProvider::instance()->getIcon("inode-directory"), tr("Open destination folder"), 0);
    connect(&actionOpen_destination_folder, SIGNAL(triggered()), this, SLOT(openSelectedTorrentsFolder()));
    QAction actionIncreasePriority(GuiIconProvider::instance()->getIcon("go-up"), tr("Move up", "i.e. move up in the queue"), 0);
    connect(&actionIncreasePriority, SIGNAL(triggered()), this, SLOT(increasePrioSelectedTorrents()));
    QAction actionDecreasePriority(GuiIconProvider::instance()->getIcon("go-down"), tr("Move down", "i.e. Move down in the queue"), 0);
    connect(&actionDecreasePriority, SIGNAL(triggered()), this, SLOT(decreasePrioSelectedTorrents()));
    QAction actionTopPriority(GuiIconProvider::instance()->getIcon("go-top"), tr("Move to top", "i.e. Move to top of the queue"), 0);
    connect(&actionTopPriority, SIGNAL(triggered()), this, SLOT(topPrioSelectedTorrents()));
    QAction actionBottomPriority(GuiIconProvider::instance()->getIcon("go-bottom"), tr("Move to bottom", "i.e. Move to bottom of the queue"), 0);
    connect(&actionBottomPriority, SIGNAL(triggered()), this, SLOT(bottomPrioSelectedTorrents()));
    QAction actionSetTorrentPath(GuiIconProvider::instance()->getIcon("inode-directory"), tr("Set location..."), 0);
    connect(&actionSetTorrentPath, SIGNAL(triggered()), this, SLOT(setSelectedTorrentsLocation()));
    QAction actionForce_recheck(GuiIconProvider::instance()->getIcon("document-edit-verify"), tr("Force recheck"), 0);
    connect(&actionForce_recheck, SIGNAL(triggered()), this, SLOT(recheckSelectedTorrents()));
    QAction actionCopy_magnet_link(GuiIconProvider::instance()->getIcon("kt-magnet"), tr("Copy magnet link"), 0);
    connect(&actionCopy_magnet_link, SIGNAL(triggered()), this, SLOT(copySelectedMagnetURIs()));
    QAction actionCopy_name(GuiIconProvider::instance()->getIcon("edit-copy"), tr("Copy name"), 0);
    connect(&actionCopy_name, SIGNAL(triggered()), this, SLOT(copySelectedNames()));
    QAction actionCopyHash(GuiIconProvider::instance()->getIcon("edit-copy"), tr("Copy hash"), 0);
    connect(&actionCopyHash, &QAction::triggered, this, &TransferListWidget::copySelectedHashes);
    QAction actionSuper_seeding_mode(tr("Super seeding mode"), 0);
    actionSuper_seeding_mode.setCheckable(true);
    connect(&actionSuper_seeding_mode, SIGNAL(triggered()), this, SLOT(toggleSelectedTorrentsSuperSeeding()));
    QAction actionRename(GuiIconProvider::instance()->getIcon("edit-rename"), tr("Rename..."), 0);
    connect(&actionRename, SIGNAL(triggered()), this, SLOT(renameSelectedTorrent()));
    QAction actionSequential_download(tr("Download in sequential order"), 0);
    actionSequential_download.setCheckable(true);
    connect(&actionSequential_download, SIGNAL(triggered()), this, SLOT(toggleSelectedTorrentsSequentialDownload()));
    QAction actionFirstLastPiece_prio(tr("Download first and last pieces first"), 0);
    actionFirstLastPiece_prio.setCheckable(true);
    connect(&actionFirstLastPiece_prio, SIGNAL(triggered()), this, SLOT(toggleSelectedFirstLastPiecePrio()));
    QAction actionAutoTMM(tr("Automatic Torrent Management"), 0);
    actionAutoTMM.setCheckable(true);
    actionAutoTMM.setToolTip(tr("Automatic mode means that various torrent properties(eg save path) will be decided by the associated category"));
    connect(&actionAutoTMM, SIGNAL(triggered(bool)), this, SLOT(setSelectedAutoTMMEnabled(bool)));
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
        torrent = listModel->torrentHandle(mapToSource(index));
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
    QStringList categories = BitTorrent::Session::instance()->categories();
    std::sort(categories.begin(), categories.end(), Utils::String::naturalCompareCaseInsensitive);
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
    std::sort(tags.begin(), tags.end(), Utils::String::naturalCompareCaseInsensitive);
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
    QAction *act = 0;
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
    BitTorrent::TorrentHandle *torrent = 0;
    if (current.isValid()) {
        torrent = listModel->torrentHandle(mapToSource(current));
        // Scroll Fix
        scrollTo(current);
    }
    emit currentTorrentChanged(torrent);
}

void TransferListWidget::applyCategoryFilter(QString category)
{
    if (category.isNull())
        nameFilterModel->disableCategoryFilter();
    else
        nameFilterModel->setCategoryFilter(category);
}

void TransferListWidget::applyTagFilter(const QString &tag)
{
    if (tag.isNull())
        nameFilterModel->disableTagFilter();
    else
        nameFilterModel->setTagFilter(tag);
}

void TransferListWidget::applyTrackerFilterAll()
{
    nameFilterModel->disableTrackerFilter();
}

void TransferListWidget::applyTrackerFilter(const QStringList &hashes)
{
    nameFilterModel->setTrackerFilter(hashes);
}

void TransferListWidget::applyNameFilter(const QString& name)
{
    nameFilterModel->setFilterRegExp(QRegExp(name, Qt::CaseInsensitive, QRegExp::WildcardUnix));
}

void TransferListWidget::applyStatusFilter(int f)
{
    nameFilterModel->setStatusFilter(static_cast<TorrentFilter::Type>(f));
    // Select first item if nothing is selected
    if (selectionModel()->selectedRows(0).empty() && nameFilterModel->rowCount() > 0) {
        qDebug("Nothing is selected, selecting first row: %s", qUtf8Printable(nameFilterModel->index(0, TorrentModel::TR_NAME).data().toString()));
        selectionModel()->setCurrentIndex(nameFilterModel->index(0, TorrentModel::TR_NAME), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    }
}

void TransferListWidget::saveSettings()
{
    Preferences::instance()->setTransHeaderState(header()->saveState());
}

bool TransferListWidget::loadSettings()
{
    bool ok = header()->restoreState(Preferences::instance()->getTransHeaderState());
    if (!ok)
        header()->resizeSection(0, 200); // Default
    return ok;
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
