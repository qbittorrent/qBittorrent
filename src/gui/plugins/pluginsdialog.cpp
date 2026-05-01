/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include "pluginsdialog.h"

#include <QClipboard>
#include <QDropEvent>
#include <QFileDialog>
#include <QHeaderView>
#include <QImageReader>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>

#include "base/path.h"
#include "base/utils/fs.h"
#include "gui/autoexpandabledialog.h"
#include "gui/uithememanager.h"
#include "gui/utils.h"
#include "ui_pluginsdialog.h"

#define SETTINGS_KEY(name) u"PluginsDialog/" name

enum PluginColumns
{
    PLUGIN_ID,
    PLUGIN_NAME,
    PLUGIN_VERSION,
    PLUGIN_INVOCABLE,
    PLUGIN_STATE
};

PluginsDialog::PluginsDialog(PluginsEngine *pluginsEngine, QWidget *parent)
    : QDialog(parent)
    , m_ui {new Ui::PluginsDialog}
    , m_storeDialogSize {SETTINGS_KEY(u"Size"_s)}
    , m_pluginsEngine {pluginsEngine}
{
    m_ui->setupUi(this);

    m_ui->pluginsTree->setRootIsDecorated(false);
    m_ui->pluginsTree->header()->setFirstSectionMovable(true);
    m_ui->pluginsTree->header()->setSortIndicator(0, Qt::AscendingOrder);

    m_ui->actionUninstall->setIcon(UIThemeManager::instance()->getIcon(u"list-remove"_s));

    connect(m_ui->actionEnable, &QAction::toggled, this, &PluginsDialog::enableSelection);
    connect(m_ui->pluginsTree, &QTreeWidget::customContextMenuRequested, this, &PluginsDialog::displayContextMenu);
    connect(m_ui->pluginsTree, &QTreeWidget::itemDoubleClicked, this, &PluginsDialog::togglePluginState);

    connect(m_ui->closeButton, &QAbstractButton::clicked, this, &QDialog::close);
    connect(m_ui->installButton, &QAbstractButton::clicked, this, &PluginsDialog::onActionInstallTriggered);
    connect(m_ui->actionUninstall, &QAction::triggered, this, &PluginsDialog::onActionUninstallTriggered);

    loadPlugins();

    connect(m_pluginsEngine, &PluginsEngine::pluginInstalled, this, &PluginsDialog::pluginInstalled);
    connect(m_pluginsEngine, &PluginsEngine::pluginInstallationFailed, this, &PluginsDialog::pluginInstallationFailed);
    connect(m_pluginsEngine, &PluginsEngine::pluginUpdated, this, &PluginsDialog::pluginUpdated);
    connect(m_pluginsEngine, &PluginsEngine::pluginUpdateFailed, this, &PluginsDialog::pluginUpdateFailed);
    connect(m_pluginsEngine, &PluginsEngine::pluginUninstalled, this, &PluginsDialog::pluginUninstalled);
    connect(m_pluginsEngine, &PluginsEngine::pluginUninstallationFailed, this, &PluginsDialog::pluginUninstallationFailed);
    connect(m_pluginsEngine, &PluginsEngine::pluginEnabledChanged, this, &PluginsDialog::pluginEnabledChanged);

    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);
}

PluginsDialog::~PluginsDialog()
{
    m_storeDialogSize = size();
    delete m_ui;
}

void PluginsDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasText())
    {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
}

void PluginsDialog::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void PluginsDialog::dropEvent(QDropEvent *event)
{
    event->acceptProposedAction();

    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls())
    {
        for (const QUrl &url : asConst(mimeData->urls()))
        {
            if (url.isLocalFile())
                installPlugin(Path(url.toLocalFile()));
        }
    }
    else
    {
        for (const QString &pathStr : asConst(mimeData->text().split(u'\n')))
            installPlugin(Path(pathStr));
    }
}

void PluginsDialog::onActionInstallTriggered()
{
    const QStringList pathsList = QFileDialog::getOpenFileNames(
            nullptr, tr("Select plugins"), QDir::homePath()
            , (tr("qBittorrent plugin") + u" (*.lua)"));
    for (const QString &pathStr : pathsList)
        installPlugin(Path(pathStr));
}

void PluginsDialog::onActionUninstallTriggered()
{
    for (QTreeWidgetItem *item : asConst(m_ui->pluginsTree->selectedItems()))
    {
        int index = m_ui->pluginsTree->indexOfTopLevelItem(item);
        Q_ASSERT(index != -1);
        if (index == -1) [[unlikely]]
            continue;

        const QString pluginID = item->text(PLUGIN_ID);
        if (m_pluginsEngine->uninstallPlugin(pluginID))
        {
            startAsyncOp();
            m_uninstallingPlugins.insert(pluginID);
        }
    }
}

void PluginsDialog::togglePluginState(QTreeWidgetItem *item)
{
    const QString pluginID = item->text(PLUGIN_ID);
    const bool isEnabled = item->data(PLUGIN_STATE, Qt::UserRole).toBool();
    m_pluginsEngine->setPluginEnabled(pluginID, !isEnabled);
}

void PluginsDialog::displayContextMenu()
{
    const QList<QTreeWidgetItem *> items = m_ui->pluginsTree->selectedItems();
    if (items.isEmpty())
        return;

    QMenu *myContextMenu = new QMenu(this);
    myContextMenu->setAttribute(Qt::WA_DeleteOnClose);

    QTreeWidgetItem *firstItem = items.first();
    const bool isEnabled = firstItem->data(PLUGIN_STATE, Qt::UserRole).toBool();
    m_ui->actionEnable->setChecked(isEnabled);
    myContextMenu->addAction(m_ui->actionEnable);
    myContextMenu->addSeparator();
    myContextMenu->addAction(m_ui->actionUninstall);

    myContextMenu->popup(QCursor::pos());
}

void PluginsDialog::enableSelection(bool enable)
{
    for (QTreeWidgetItem *item : asConst(m_ui->pluginsTree->selectedItems()))
    {
        int index = m_ui->pluginsTree->indexOfTopLevelItem(item);
        Q_ASSERT(index != -1);
        QString id = item->text(PLUGIN_ID);
        m_pluginsEngine->setPluginEnabled(id, enable);
        if (enable)
        {
            item->setText(PLUGIN_STATE, tr("Yes"));
            setRowColor(index, u"green"_s);
        }
        else
        {
            item->setText(PLUGIN_STATE, tr("No"));
            setRowColor(index, u"red"_s);
        }
    }
}

void PluginsDialog::setRowColor(const int row, const QString &color)
{
    QTreeWidgetItem *item = m_ui->pluginsTree->topLevelItem(row);
    for (int i = 0; i < m_ui->pluginsTree->columnCount(); ++i)
    {
        item->setData(i, Qt::ForegroundRole, QColor(color));
    }
}

QTreeWidgetItem *PluginsDialog::findItem(const QString &pluginID)
{
    for (int i = 0; i < m_ui->pluginsTree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = m_ui->pluginsTree->topLevelItem(i);
        if (pluginID == item->text(PLUGIN_ID))
            return item;
    }

    return nullptr;
}

void PluginsDialog::loadPlugins()
{
    // Some clean up first
    m_ui->pluginsTree->clear();
    for (const PluginInfo &pluginInfo : asConst(m_pluginsEngine->allPlugins()))
        addNewPlugin(pluginInfo);
}

void PluginsDialog::addNewPlugin(const PluginInfo &pluginInfo)
{
    auto *item = new QTreeWidgetItem(m_ui->pluginsTree);
    item->setText(PLUGIN_ID, pluginInfo.id);
    item->setText(PLUGIN_NAME, pluginInfo.name);
    item->setText(PLUGIN_INVOCABLE, (pluginInfo.invocable ? tr("Yes") : tr("No")));
    item->setData(PLUGIN_STATE, Qt::UserRole, pluginInfo.enabled);
    if (pluginInfo.enabled)
    {
        item->setText(PLUGIN_STATE, tr("Yes"));
        setRowColor(m_ui->pluginsTree->indexOfTopLevelItem(item), u"green"_s);
    }
    else
    {
        item->setText(PLUGIN_STATE, tr("No"));
        setRowColor(m_ui->pluginsTree->indexOfTopLevelItem(item), u"red"_s);
    }

    item->setText(PLUGIN_VERSION, pluginInfo.version.toString());
}

void PluginsDialog::updatePlugin(const PluginInfo &pluginInfo)
{
    QTreeWidgetItem *item = findItem(pluginInfo.id);
    if (!item)
        return;

    item->setText(PLUGIN_NAME, pluginInfo.name);
    item->setText(PLUGIN_INVOCABLE, (pluginInfo.invocable ? tr("Yes") : tr("No")));
    item->setText(PLUGIN_VERSION, pluginInfo.version.toString());
}

void PluginsDialog::installPlugin(const Path &pluginPath)
{
    if (m_pluginsEngine->installPlugin(pluginPath))
    {
        startAsyncOp();
        m_installingPlugins.insert(pluginPath);
    }
}

void PluginsDialog::startAsyncOp()
{
    ++m_asyncOps;
    if (m_asyncOps == 1)
        setCursor(QCursor(Qt::WaitCursor));
}

void PluginsDialog::finishAsyncOp()
{
    --m_asyncOps;
    if (m_asyncOps == 0)
        setCursor(QCursor(Qt::ArrowCursor));
}

void PluginsDialog::pluginInstalled(const Path &pluginPath, const PluginInfo &pluginInfo)
{
    addNewPlugin(pluginInfo);
    if (m_installingPlugins.remove(pluginPath))
    {
        finishAsyncOp();
        QMessageBox::information(this, tr("Plugin install"), tr("Plugin '%1' installed.").arg(pluginInfo.id));
    }
}

void PluginsDialog::pluginInstallationFailed(const Path &pluginPath, const QString &reason)
{
    if (m_installingPlugins.remove(pluginPath))
    {
        finishAsyncOp();
        QMessageBox::information(this, tr("Plugin install")
                , tr("Couldn't install plugin from file '%1'.\n%2").arg(pluginPath.toString(), reason));
    }
}

void PluginsDialog::pluginUpdated(const Path &pluginPath
        , const PluginInfo &, const PluginInfo &newPluginInfo)
{
    updatePlugin(newPluginInfo);
    if (m_installingPlugins.remove(pluginPath))
    {
        finishAsyncOp();
        QMessageBox::information(this, tr("Plugin update"), tr("Plugin '%1' updated to version %2.")
                .arg(newPluginInfo.id, newPluginInfo.version.toString()));
    }
}

void PluginsDialog::pluginUpdateFailed(const Path &pluginPath, const PluginInfo &currentPluginInfo
        , const PluginInfo &, const QString &reason)
{
    if (m_installingPlugins.remove(pluginPath))
    {
        finishAsyncOp();
        QMessageBox::information(this, tr("Plugin update")
                , tr("Couldn't update plugin '%1' from file '%2'.\n%3")
                        .arg(currentPluginInfo.id, pluginPath.toString(), reason));
    }
}

void PluginsDialog::pluginUninstalled(const QString &pluginID)
{
    QTreeWidgetItem *item = findItem(pluginID);
    if (!item)
        return;

    if (m_uninstallingPlugins.remove(pluginID))
    {
        delete item;
        finishAsyncOp();
        QMessageBox::information(this, tr("Plugin uninstall"), tr("Plugin '%1' uninstalled.").arg(pluginID));
    }
}

void PluginsDialog::pluginUninstallationFailed(const QString &pluginID, const QString &reason)
{
    QTreeWidgetItem *item = findItem(pluginID);
    if (!item)
        return;

    if (m_uninstallingPlugins.remove(pluginID))
    {
        // Disable it instead
        m_pluginsEngine->setPluginEnabled(pluginID, false);
        QMessageBox::warning(this, tr("Plugin uninstall")
                , tr("Failed to uninstall plugin '%1'.\n%2").arg(pluginID, reason));
    }
}

void PluginsDialog::pluginEnabledChanged(const QString &pluginID, bool isEnabled)
{
    QTreeWidgetItem *item = findItem(pluginID);
    if (!item)
        return;

    item->setData(PLUGIN_STATE, Qt::UserRole, isEnabled);
    if (isEnabled)
    {
        item->setText(PLUGIN_STATE, tr("Yes"));
        setRowColor(m_ui->pluginsTree->indexOfTopLevelItem(item), u"green"_s);
    }
    else
    {
        item->setText(PLUGIN_STATE, tr("No"));
        setRowColor(m_ui->pluginsTree->indexOfTopLevelItem(item), u"red"_s);
    }
}
