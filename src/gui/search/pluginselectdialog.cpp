/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#include "pluginselectdialog.h"

#include <QClipboard>
#include <QDropEvent>
#include <QFileDialog>
#include <QHeaderView>
#include <QImageReader>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QTableView>

#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "base/utils/fs.h"
#include "gui/autoexpandabledialog.h"
#include "gui/uithememanager.h"
#include "gui/utils.h"
#include "pluginsourcedialog.h"
#include "searchwidget.h"
#include "ui_pluginselectdialog.h"

enum PluginColumns
{
    PLUGIN_NAME,
    PLUGIN_VERSION,
    PLUGIN_URL,
    PLUGIN_STATE,
    PLUGIN_ID
};

PluginSelectDialog::PluginSelectDialog(SearchPluginManager *pluginManager, QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::PluginSelectDialog())
    , m_pluginManager(pluginManager)
    , m_asyncOps(0)
    , m_pendingUpdates(0)
{
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(m_ui->pluginsTree->header());
    m_ui->pluginsTree->header()->setParent(m_ui->pluginsTree);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));

    m_ui->pluginsTree->setRootIsDecorated(false);
    m_ui->pluginsTree->hideColumn(PLUGIN_ID);
    m_ui->pluginsTree->header()->setSortIndicator(0, Qt::AscendingOrder);

    m_ui->actionUninstall->setIcon(UIThemeManager::instance()->getIcon("list-remove"));

    connect(m_ui->actionEnable, &QAction::toggled, this, &PluginSelectDialog::enableSelection);
    connect(m_ui->pluginsTree, &QTreeWidget::customContextMenuRequested, this, &PluginSelectDialog::displayContextMenu);
    connect(m_ui->pluginsTree, &QTreeWidget::itemDoubleClicked, this, &PluginSelectDialog::togglePluginState);

    loadSupportedSearchPlugins();

    connect(m_pluginManager, &SearchPluginManager::pluginInstalled, this, &PluginSelectDialog::pluginInstalled);
    connect(m_pluginManager, &SearchPluginManager::pluginInstallationFailed, this, &PluginSelectDialog::pluginInstallationFailed);
    connect(m_pluginManager, &SearchPluginManager::pluginUpdated, this, &PluginSelectDialog::pluginUpdated);
    connect(m_pluginManager, &SearchPluginManager::pluginUpdateFailed, this, &PluginSelectDialog::pluginUpdateFailed);
    connect(m_pluginManager, &SearchPluginManager::checkForUpdatesFinished, this, &PluginSelectDialog::checkForUpdatesFinished);
    connect(m_pluginManager, &SearchPluginManager::checkForUpdatesFailed, this, &PluginSelectDialog::checkForUpdatesFailed);

    Utils::Gui::resize(this);
    show();
}

PluginSelectDialog::~PluginSelectDialog()
{
    delete m_ui;
}

void PluginSelectDialog::dropEvent(QDropEvent *event)
{
    event->acceptProposedAction();

    QStringList files;
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : asConst(event->mimeData()->urls())) {
            if (!url.isEmpty()) {
                if (url.scheme().compare("file", Qt::CaseInsensitive) == 0)
                    files << url.toLocalFile();
                else
                    files << url.toString();
            }
        }
    }
    else {
        files = event->mimeData()->text().split('\n');
    }

    if (files.isEmpty()) return;

    for (const QString &file : asConst(files)) {
        qDebug("dropped %s", qUtf8Printable(file));
        startAsyncOp();
        m_pluginManager->installPlugin(file);
    }
}

// Decode if we accept drag 'n drop or not
void PluginSelectDialog::dragEnterEvent(QDragEnterEvent *event)
{
    for (const QString &mime : asConst(event->mimeData()->formats())) {
        qDebug("mimeData: %s", qUtf8Printable(mime));
    }

    if (event->mimeData()->hasFormat(QLatin1String("text/plain")) || event->mimeData()->hasFormat(QLatin1String("text/uri-list"))) {
        event->acceptProposedAction();
    }
}

void PluginSelectDialog::on_updateButton_clicked()
{
    startAsyncOp();
    m_pluginManager->checkForUpdates();
}

void PluginSelectDialog::togglePluginState(QTreeWidgetItem *item, int)
{
    PluginInfo *plugin = m_pluginManager->pluginInfo(item->text(PLUGIN_ID));
    m_pluginManager->enablePlugin(plugin->name, !plugin->enabled);
    if (plugin->enabled) {
        item->setText(PLUGIN_STATE, tr("Yes"));
        setRowColor(m_ui->pluginsTree->indexOfTopLevelItem(item), "green");
    }
    else {
        item->setText(PLUGIN_STATE, tr("No"));
        setRowColor(m_ui->pluginsTree->indexOfTopLevelItem(item), "red");
    }
}

void PluginSelectDialog::displayContextMenu(const QPoint &)
{
    // Enable/disable pause/start action given the DL state
    const QList<QTreeWidgetItem *> items = m_ui->pluginsTree->selectedItems();
    if (items.isEmpty()) return;

    QMenu *myContextMenu = new QMenu(this);
    myContextMenu->setAttribute(Qt::WA_DeleteOnClose);

    const QString firstID = items.first()->text(PLUGIN_ID);
    m_ui->actionEnable->setChecked(m_pluginManager->pluginInfo(firstID)->enabled);
    myContextMenu->addAction(m_ui->actionEnable);
    myContextMenu->addSeparator();
    myContextMenu->addAction(m_ui->actionUninstall);

    myContextMenu->popup(QCursor::pos());
}

void PluginSelectDialog::on_closeButton_clicked()
{
    close();
}

void PluginSelectDialog::on_actionUninstall_triggered()
{
    bool error = false;
    for (QTreeWidgetItem *item : asConst(m_ui->pluginsTree->selectedItems())) {
        int index = m_ui->pluginsTree->indexOfTopLevelItem(item);
        Q_ASSERT(index != -1);
        QString id = item->text(PLUGIN_ID);
        if (m_pluginManager->uninstallPlugin(id)) {
            delete item;
        }
        else {
            error = true;
            // Disable it instead
            m_pluginManager->enablePlugin(id, false);
            item->setText(PLUGIN_STATE, tr("No"));
            setRowColor(index, "red");
        }
    }

    if (error)
        QMessageBox::warning(this, tr("Uninstall warning"), tr("Some plugins could not be uninstalled because they are included in qBittorrent. Only the ones you added yourself can be uninstalled.\nThose plugins were disabled."));
    else
        QMessageBox::information(this, tr("Uninstall success"), tr("All selected plugins were uninstalled successfully"));
}

void PluginSelectDialog::enableSelection(bool enable)
{
    for (QTreeWidgetItem *item : asConst(m_ui->pluginsTree->selectedItems())) {
        int index = m_ui->pluginsTree->indexOfTopLevelItem(item);
        Q_ASSERT(index != -1);
        QString id = item->text(PLUGIN_ID);
        m_pluginManager->enablePlugin(id, enable);
        if (enable) {
            item->setText(PLUGIN_STATE, tr("Yes"));
            setRowColor(index, "green");
        }
        else {
            item->setText(PLUGIN_STATE, tr("No"));
            setRowColor(index, "red");
        }
    }
}

// Set the color of a row in data model
void PluginSelectDialog::setRowColor(const int row, const QString &color)
{
    QTreeWidgetItem *item = m_ui->pluginsTree->topLevelItem(row);
    for (int i = 0; i < m_ui->pluginsTree->columnCount(); ++i) {
        item->setData(i, Qt::ForegroundRole, QColor(color));
    }
}

QVector<QTreeWidgetItem*> PluginSelectDialog::findItemsWithUrl(const QString &url)
{
    QVector<QTreeWidgetItem*> res;
    res.reserve(m_ui->pluginsTree->topLevelItemCount());

    for (int i = 0; i < m_ui->pluginsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_ui->pluginsTree->topLevelItem(i);
        if (url.startsWith(item->text(PLUGIN_URL), Qt::CaseInsensitive))
            res << item;
    }

    return res;
}

QTreeWidgetItem *PluginSelectDialog::findItemWithID(const QString &id)
{
    for (int i = 0; i < m_ui->pluginsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_ui->pluginsTree->topLevelItem(i);
        if (id == item->text(PLUGIN_ID))
            return item;
    }

    return nullptr;
}

void PluginSelectDialog::loadSupportedSearchPlugins()
{
    // Some clean up first
    m_ui->pluginsTree->clear();
    for (const QString &name : asConst(m_pluginManager->allPlugins()))
        addNewPlugin(name);
}

void PluginSelectDialog::addNewPlugin(const QString &pluginName)
{
    auto *item = new QTreeWidgetItem(m_ui->pluginsTree);
    PluginInfo *plugin = m_pluginManager->pluginInfo(pluginName);
    item->setText(PLUGIN_NAME, plugin->fullName);
    item->setText(PLUGIN_URL, plugin->url);
    item->setText(PLUGIN_ID, plugin->name);
    if (plugin->enabled) {
        item->setText(PLUGIN_STATE, tr("Yes"));
        setRowColor(m_ui->pluginsTree->indexOfTopLevelItem(item), "green");
    }
    else {
        item->setText(PLUGIN_STATE, tr("No"));
        setRowColor(m_ui->pluginsTree->indexOfTopLevelItem(item), "red");
    }
    // Handle icon
    if (QFile::exists(plugin->iconPath)) {
        // Good, we already have the icon
        item->setData(PLUGIN_NAME, Qt::DecorationRole, QIcon(plugin->iconPath));
    }
    else {
        // Icon is missing, we must download it
        using namespace Net;
        DownloadManager::instance()->download(
                    DownloadRequest(plugin->url + "/favicon.ico").saveToFile(true)
                    , this, &PluginSelectDialog::iconDownloadFinished);
    }
    item->setText(PLUGIN_VERSION, plugin->version);
}

void PluginSelectDialog::startAsyncOp()
{
    ++m_asyncOps;
    if (m_asyncOps == 1)
        setCursor(QCursor(Qt::WaitCursor));
}

void PluginSelectDialog::finishAsyncOp()
{
    --m_asyncOps;
    if (m_asyncOps == 0)
        setCursor(QCursor(Qt::ArrowCursor));
}

void PluginSelectDialog::finishPluginUpdate()
{
    --m_pendingUpdates;
    if ((m_pendingUpdates == 0) && !m_updatedPlugins.isEmpty()) {
        m_updatedPlugins.sort(Qt::CaseInsensitive);
        QMessageBox::information(this, tr("Search plugin update"), tr("Plugins installed or updated: %1").arg(m_updatedPlugins.join(", ")));
        m_updatedPlugins.clear();
    }
}

void PluginSelectDialog::on_installButton_clicked()
{
    auto *dlg = new PluginSourceDialog(this);
    connect(dlg, &PluginSourceDialog::askForLocalFile, this, &PluginSelectDialog::askForLocalPlugin);
    connect(dlg, &PluginSourceDialog::askForUrl, this, &PluginSelectDialog::askForPluginUrl);
}

void PluginSelectDialog::askForPluginUrl()
{
    bool ok = false;
    QString clipTxt = qApp->clipboard()->text();
    QString defaultUrl = "http://";
    if (Net::DownloadManager::hasSupportedScheme(clipTxt) && clipTxt.endsWith(".py"))
      defaultUrl = clipTxt;
    QString url = AutoExpandableDialog::getText(
                this, tr("New search engine plugin URL"),
                tr("URL:"), QLineEdit::Normal, defaultUrl, &ok
                );

    while (ok && !url.isEmpty() && !url.endsWith(".py")) {
        QMessageBox::warning(this, tr("Invalid link"), tr("The link doesn't seem to point to a search engine plugin."));
        url = AutoExpandableDialog::getText(
                    this, tr("New search engine plugin URL"),
                    tr("URL:"), QLineEdit::Normal, url, &ok
                    );
    }

    if (ok && !url.isEmpty()) {
        startAsyncOp();
        m_pluginManager->installPlugin(url);
    }
}

void PluginSelectDialog::askForLocalPlugin()
{
    const QStringList pathsList = QFileDialog::getOpenFileNames(
                nullptr, tr("Select search plugins"), QDir::homePath(),
                tr("qBittorrent search plugin") + QLatin1String(" (*.py)")
                );
    for (const QString &path : pathsList) {
        startAsyncOp();
        m_pluginManager->installPlugin(path);
    }
}

void PluginSelectDialog::iconDownloadFinished(const Net::DownloadResult &result)
{
    if (result.status != Net::DownloadStatus::Success) {
        qDebug("Could not download favicon: %s, reason: %s", qUtf8Printable(result.url), qUtf8Printable(result.errorString));
        return;
    }

    const QString filePath = Utils::Fs::toUniformPath(result.filePath);

    // Icon downloaded
    QIcon icon(filePath);
    // Detect a non-decodable icon
    QList<QSize> sizes = icon.availableSizes();
    bool invalid = (sizes.isEmpty() || icon.pixmap(sizes.first()).isNull());
    if (!invalid) {
        for (QTreeWidgetItem *item : asConst(findItemsWithUrl(result.url))) {
            QString id = item->text(PLUGIN_ID);
            PluginInfo *plugin = m_pluginManager->pluginInfo(id);
            if (!plugin) continue;

            QString iconPath = QString("%1/%2.%3")
                .arg(SearchPluginManager::pluginsLocation()
                    , id
                    , result.url.endsWith(".ico", Qt::CaseInsensitive) ? "ico" : "png");
            if (QFile::copy(filePath, iconPath)) {
                // This 2nd check is necessary. Some favicons (eg from piratebay)
                // decode fine without an ext, but fail to do so when appending the ext
                // from the url. Probably a Qt bug.
                QIcon iconWithExt(iconPath);
                QList<QSize> sizesExt = iconWithExt.availableSizes();
                bool invalidExt = (sizesExt.isEmpty() || iconWithExt.pixmap(sizesExt.first()).isNull());
                if (invalidExt) {
                    Utils::Fs::forceRemove(iconPath);
                    continue;
                }

                item->setData(PLUGIN_NAME, Qt::DecorationRole, iconWithExt);
                m_pluginManager->updateIconPath(plugin);
            }
        }
    }
    // Delete tmp file
    Utils::Fs::forceRemove(filePath);
}

void PluginSelectDialog::checkForUpdatesFinished(const QHash<QString, PluginVersion> &updateInfo)
{
    finishAsyncOp();
    if (updateInfo.isEmpty()) {
        QMessageBox::information(this, tr("Search plugin update"), tr("All your plugins are already up to date."));
        return;
    }

    for (auto i = updateInfo.cbegin(); i != updateInfo.cend(); ++i) {
        startAsyncOp();
        ++m_pendingUpdates;
        m_pluginManager->updatePlugin(i.key());
    }
}

void PluginSelectDialog::checkForUpdatesFailed(const QString &reason)
{
    finishAsyncOp();
    QMessageBox::warning(this, tr("Search plugin update"), tr("Sorry, couldn't check for plugin updates. %1").arg(reason));
}

void PluginSelectDialog::pluginInstalled(const QString &name)
{
    addNewPlugin(name);
    finishAsyncOp();
    m_updatedPlugins.append(name);
    finishPluginUpdate();
}

void PluginSelectDialog::pluginInstallationFailed(const QString &name, const QString &reason)
{
    finishAsyncOp();
    QMessageBox::information(this, tr("Search plugin install")
        , tr("Couldn't install \"%1\" search engine plugin. %2").arg(name, reason));
    finishPluginUpdate();
}

void PluginSelectDialog::pluginUpdated(const QString &name)
{
    finishAsyncOp();
    PluginVersion version = m_pluginManager->pluginInfo(name)->version;
    QTreeWidgetItem *item = findItemWithID(name);
    item->setText(PLUGIN_VERSION, version);
    m_updatedPlugins.append(name);
    finishPluginUpdate();
}

void PluginSelectDialog::pluginUpdateFailed(const QString &name, const QString &reason)
{
    finishAsyncOp();
    QMessageBox::information(this, tr("Search plugin update")
        , tr("Couldn't update \"%1\" search engine plugin. %2").arg(name, reason));
    finishPluginUpdate();
}
