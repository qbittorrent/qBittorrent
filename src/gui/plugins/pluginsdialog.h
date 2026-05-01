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

#pragma once

#include <QDialog>
#include <QSet>

#include "base/plugins/pluginsengine.h"
#include "base/settingvalue.h"

class QDropEvent;
class QTreeWidgetItem;

namespace Net
{
    struct DownloadResult;
}

namespace Ui
{
    class PluginsDialog;
}

class PluginsDialog final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PluginsDialog)

public:
    explicit PluginsDialog(PluginsEngine *pluginsEngine, QWidget *parent = nullptr);
    ~PluginsDialog() override;

    QTreeWidgetItem *findItem(const QString &pluginID);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onActionInstallTriggered();
    void onActionUninstallTriggered();
    void togglePluginState(QTreeWidgetItem*);
    void setRowColor(int row, const QString &color);
    void displayContextMenu();
    void enableSelection(bool enable);

    void pluginInstalled(const Path &pluginPath, const PluginInfo &pluginInfo);
    void pluginInstallationFailed(const Path &pluginPath, const QString &reason);
    void pluginUpdated(const Path &pluginPath, const PluginInfo &oldPluginInfo
            , const PluginInfo &newPluginInfo);
    void pluginUpdateFailed(const Path &pluginPath, const PluginInfo &currentPluginInfo
            , const PluginInfo &newPluginInfo, const QString &reason);
    void pluginUninstalled(const QString &pluginID);
    void pluginUninstallationFailed(const QString &pluginID, const QString &reason);
    void pluginEnabledChanged(const QString &pluginID, bool isEnabled);

private:
    void loadPlugins();
    void addNewPlugin(const PluginInfo &pluginInfo);
    void updatePlugin(const PluginInfo &pluginInfo);
    void installPlugin(const Path &pluginPath);
    void startAsyncOp();
    void finishAsyncOp();

    Ui::PluginsDialog *m_ui = nullptr;
    PluginsEngine *m_pluginsEngine = nullptr;

    SettingValue<QSize> m_storeDialogSize;
    SettingValue<QByteArray> m_storeHeaderState;
    SettingValue<QString> m_storeLastPath;

    int m_asyncOps = 0;
    QSet<Path> m_installingPlugins;
    QSet<QString> m_uninstallingPlugins;
};
