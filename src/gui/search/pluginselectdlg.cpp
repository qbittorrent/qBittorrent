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
 *
 * Contact : chris@qbittorrent.org
 */

#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QFileDialog>
#include <QDropEvent>
#include <QTemporaryFile>
#include <QMimeData>
#include <QClipboard>
#ifdef QBT_USES_QT5
#include <QTableView>
#endif

#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/net/downloadmanager.h"
#include "base/net/downloadhandler.h"
#include "base/searchengine.h"
#include "ico.h"
#include "searchwidget.h"
#include "pluginsourcedlg.h"
#include "guiiconprovider.h"
#include "autoexpandabledialog.h"
#include "pluginselectdlg.h"

enum PluginColumns
{
    PLUGIN_NAME,
    PLUGIN_VERSION,
    PLUGIN_URL,
    PLUGIN_STATE,
    PLUGIN_ID
};

PluginSelectDlg::PluginSelectDlg(SearchEngine *pluginManager, QWidget *parent)
    : QDialog(parent)
    , m_pluginManager(pluginManager)
    , m_asyncOps(0)
{
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

#ifdef QBT_USES_QT5
    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(pluginsTree->header());
    pluginsTree->header()->setParent(pluginsTree);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));
#endif
    pluginsTree->setRootIsDecorated(false);
    pluginsTree->header()->resizeSection(0, 160);
    pluginsTree->header()->resizeSection(1, 80);
    pluginsTree->header()->resizeSection(2, 200);
    pluginsTree->hideColumn(PLUGIN_ID);

    actionUninstall->setIcon(GuiIconProvider::instance()->getIcon("list-remove"));

    connect(actionEnable, SIGNAL(toggled(bool)), this, SLOT(enableSelection(bool)));
    connect(pluginsTree, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayContextMenu(const QPoint&)));
    connect(pluginsTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(togglePluginState(QTreeWidgetItem*, int)));

    loadSupportedSearchPlugins();

    connect(m_pluginManager, SIGNAL(pluginInstalled(QString)), SLOT(pluginInstalled(QString)));
    connect(m_pluginManager, SIGNAL(pluginInstallationFailed(QString, QString)), SLOT(pluginInstallationFailed(QString, QString)));
    connect(m_pluginManager, SIGNAL(pluginUpdated(QString)), SLOT(pluginUpdated(QString)));
    connect(m_pluginManager, SIGNAL(pluginUpdateFailed(QString, QString)), SLOT(pluginUpdateFailed(QString, QString)));
    connect(m_pluginManager, SIGNAL(checkForUpdatesFinished(QHash<QString, qreal>)), SLOT(checkForUpdatesFinished(QHash<QString, qreal>)));
    connect(m_pluginManager, SIGNAL(checkForUpdatesFailed(QString)), SLOT(checkForUpdatesFailed(QString)));

    show();
}

PluginSelectDlg::~PluginSelectDlg()
{
    emit pluginsChanged();
}

void PluginSelectDlg::dropEvent(QDropEvent *event)
{
    event->acceptProposedAction();

    QStringList files;
    if (event->mimeData()->hasUrls()) {
        foreach (const QUrl &url, event->mimeData()->urls()) {
            if (!url.isEmpty()) {
                if (url.scheme().compare("file", Qt::CaseInsensitive) == 0)
                    files << url.toLocalFile();
                else
                    files << url.toString();
            }
        }
    }
    else {
        files = event->mimeData()->text().split(QLatin1String("\n"));
    }

    if (files.isEmpty()) return;

    foreach (QString file, files) {
        qDebug("dropped %s", qPrintable(file));
        startAsyncOp();
        m_pluginManager->installPlugin(file);
    }
}

// Decode if we accept drag 'n drop or not
void PluginSelectDlg::dragEnterEvent(QDragEnterEvent *event)
{
    QString mime;
    foreach (mime, event->mimeData()->formats()) {
        qDebug("mimeData: %s", qPrintable(mime));
    }

    if (event->mimeData()->hasFormat(QLatin1String("text/plain")) || event->mimeData()->hasFormat(QLatin1String("text/uri-list"))) {
        event->acceptProposedAction();
    }
}

void PluginSelectDlg::on_updateButton_clicked()
{
    startAsyncOp();
    m_pluginManager->checkForUpdates();
}

void PluginSelectDlg::togglePluginState(QTreeWidgetItem *item, int)
{
    PluginInfo *plugin = m_pluginManager->pluginInfo(item->text(PLUGIN_ID));
    m_pluginManager->enablePlugin(plugin->name, !plugin->enabled);
    if (plugin->enabled) {
        item->setText(PLUGIN_STATE, tr("Yes"));
        setRowColor(pluginsTree->indexOfTopLevelItem(item), "green");
    }
    else {
        item->setText(PLUGIN_STATE, tr("No"));
        setRowColor(pluginsTree->indexOfTopLevelItem(item), "red");
    }
}

void PluginSelectDlg::displayContextMenu(const QPoint&)
{
    QMenu myContextMenu(this);
    // Enable/disable pause/start action given the DL state
    QList<QTreeWidgetItem *> items = pluginsTree->selectedItems();
    if (items.isEmpty()) return;

    QString first_id = items.first()->text(PLUGIN_ID);
    actionEnable->setChecked(m_pluginManager->pluginInfo(first_id)->enabled);
    myContextMenu.addAction(actionEnable);
    myContextMenu.addSeparator();
    myContextMenu.addAction(actionUninstall);
    myContextMenu.exec(QCursor::pos());
}

void PluginSelectDlg::on_closeButton_clicked()
{
    close();
}

void PluginSelectDlg::on_actionUninstall_triggered()
{
    bool error = false;
    foreach (QTreeWidgetItem *item, pluginsTree->selectedItems()) {
        int index = pluginsTree->indexOfTopLevelItem(item);
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
        QMessageBox::warning(0, tr("Uninstall warning"), tr("Some plugins could not be uninstalled because they are included in qBittorrent. Only the ones you added yourself can be uninstalled.\nThose plugins were disabled."));
    else
        QMessageBox::information(0, tr("Uninstall success"), tr("All selected plugins were uninstalled successfully"));
}

void PluginSelectDlg::enableSelection(bool enable)
{
    foreach (QTreeWidgetItem *item, pluginsTree->selectedItems()) {
        int index = pluginsTree->indexOfTopLevelItem(item);
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
void PluginSelectDlg::setRowColor(int row, QString color)
{
    QTreeWidgetItem *item = pluginsTree->topLevelItem(row);
    for (int i = 0; i < pluginsTree->columnCount(); ++i) {
        item->setData(i, Qt::ForegroundRole, QVariant(QColor(color)));
    }
}

QList<QTreeWidgetItem*> PluginSelectDlg::findItemsWithUrl(QString url)
{
    QList<QTreeWidgetItem*> res;

    for (int i = 0; i < pluginsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = pluginsTree->topLevelItem(i);
        if (url.startsWith(item->text(PLUGIN_URL), Qt::CaseInsensitive))
            res << item;
    }

    return res;
}

QTreeWidgetItem* PluginSelectDlg::findItemWithID(QString id)
{
    for (int i = 0; i < pluginsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = pluginsTree->topLevelItem(i);
        if (id == item->text(PLUGIN_ID))
            return item;
    }

    return 0;
}

void PluginSelectDlg::loadSupportedSearchPlugins()
{
    // Some clean up first
    pluginsTree->clear();
    foreach (QString name, m_pluginManager->allPlugins())
        addNewPlugin(name);
}

void PluginSelectDlg::addNewPlugin(QString pluginName)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(pluginsTree);
    PluginInfo *plugin = m_pluginManager->pluginInfo(pluginName);
    item->setText(PLUGIN_NAME, plugin->fullName);
    item->setText(PLUGIN_URL, plugin->url);
    item->setText(PLUGIN_ID, plugin->name);
    if (plugin->enabled) {
        item->setText(PLUGIN_STATE, tr("Yes"));
        setRowColor(pluginsTree->indexOfTopLevelItem(item), "green");
    }
    else {
        item->setText(PLUGIN_STATE, tr("No"));
        setRowColor(pluginsTree->indexOfTopLevelItem(item), "red");
    }
    // Handle icon
    if (QFile::exists(plugin->iconPath)) {
        // Good, we already have the icon
        item->setData(PLUGIN_NAME, Qt::DecorationRole, QVariant(QIcon(plugin->iconPath)));
    }
    else {
        // Icon is missing, we must download it
        Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(plugin->url + "/favicon.ico", true);
        connect(handler, SIGNAL(downloadFinished(QString, QString)), this, SLOT(iconDownloaded(QString, QString)));
        connect(handler, SIGNAL(downloadFailed(QString, QString)), this, SLOT(iconDownloadFailed(QString, QString)));
    }
    item->setText(PLUGIN_VERSION, QString::number(plugin->version, 'f', 2));
}

void PluginSelectDlg::startAsyncOp()
{
    ++m_asyncOps;
    if (m_asyncOps == 1)
        setCursor(QCursor(Qt::WaitCursor));
}

void PluginSelectDlg::finishAsyncOp()
{
    --m_asyncOps;
    if (m_asyncOps == 0)
        setCursor(QCursor(Qt::ArrowCursor));
}

void PluginSelectDlg::on_installButton_clicked()
{
    PluginSourceDlg *dlg = new PluginSourceDlg(this);
    connect(dlg, SIGNAL(askForLocalFile()), this, SLOT(askForLocalPlugin()));
    connect(dlg, SIGNAL(askForUrl()), this, SLOT(askForPluginUrl()));
}

void PluginSelectDlg::askForPluginUrl()
{
    bool ok = false;
    QString clipTxt = qApp->clipboard()->text();
    QString defaultUrl = "http://";
    if (Utils::Misc::isUrl(clipTxt) && clipTxt.endsWith(".py"))
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

void PluginSelectDlg::askForLocalPlugin()
{
    QStringList pathsList = QFileDialog::getOpenFileNames(
                0, tr("Select search plugins"), QDir::homePath(),
                tr("qBittorrent search plugin") + QLatin1String(" (*.py)")
                );
    foreach (QString path, pathsList) {
        startAsyncOp();
        m_pluginManager->installPlugin(path);
    }
}

void PluginSelectDlg::iconDownloaded(const QString &url, QString filePath)
{
    filePath = Utils::Fs::fromNativePath(filePath);

    // Icon downloaded
    QImage fileIcon;
    if (fileIcon.load(filePath)) {
        foreach (QTreeWidgetItem *item, findItemsWithUrl(url)) {
            QString id = item->text(PLUGIN_ID);
            PluginInfo *plugin = m_pluginManager->pluginInfo(id);
            if (!plugin) continue;

            QFile icon(filePath);
            icon.open(QIODevice::ReadOnly);
            QString iconPath = QString("%1/%2.%3").arg(SearchEngine::pluginsLocation()).arg(id).arg(ICOHandler::canRead(&icon) ? "ico" : "png");
            if (QFile::copy(filePath, iconPath))
                item->setData(PLUGIN_NAME, Qt::DecorationRole, QVariant(QIcon(iconPath)));
        }
    }
    // Delete tmp file
    Utils::Fs::forceRemove(filePath);
}

void PluginSelectDlg::iconDownloadFailed(const QString &url, const QString &reason)
{
    qDebug("Could not download favicon: %s, reason: %s", qPrintable(url), qPrintable(reason));
}

void PluginSelectDlg::checkForUpdatesFinished(const QHash<QString, qreal> &updateInfo)
{
    finishAsyncOp();
    if (updateInfo.isEmpty()) {
        QMessageBox::information(this, tr("Search plugin update"), tr("All your plugins are already up to date."));
        return;
    }

    foreach (const QString &pluginName, updateInfo.keys()) {
        startAsyncOp();
        m_pluginManager->updatePlugin(pluginName);
    }
}

void PluginSelectDlg::checkForUpdatesFailed(const QString &reason)
{
    finishAsyncOp();
    QMessageBox::warning(this, tr("Search plugin update"), tr("Sorry, couldn't check for plugin updates. %1").arg(reason));
}

void PluginSelectDlg::pluginInstalled(const QString &name)
{
    addNewPlugin(name);
    finishAsyncOp();
    QMessageBox::information(this, tr("Search plugin install"), tr("\"%1\" search engine plugin was successfully installed.", "%1 is the name of the search engine").arg(name));
}

void PluginSelectDlg::pluginInstallationFailed(const QString &name, const QString &reason)
{
    finishAsyncOp();
    QMessageBox::information(this, tr("Search plugin install"), tr("Couldn't install \"%1\" search engine plugin. %2").arg(name).arg(reason));
}

void PluginSelectDlg::pluginUpdated(const QString &name)
{
    finishAsyncOp();
    qreal version = m_pluginManager->pluginInfo(name)->version;
    QTreeWidgetItem *item = findItemWithID(name);
    item->setText(PLUGIN_VERSION, QString::number(version, 'f', 2));
    QMessageBox::information(this, tr("Search plugin install"), tr("\"%1\" search engine plugin was successfully updated.", "%1 is the name of the search engine").arg(name));

}

void PluginSelectDlg::pluginUpdateFailed(const QString &name, const QString &reason)
{
    finishAsyncOp();
    QMessageBox::information(this, tr("Search plugin update"), tr("Couldn't update \"%1\" search engine plugin. %2").arg(name).arg(reason));
}
