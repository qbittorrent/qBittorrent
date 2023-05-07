/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017, 2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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

#include <utility>

#include <QDialog>
#include <QHash>
#include <QSet>

#include "base/rss/rss_autodownloadrule.h"
#include "base/settingvalue.h"

class QListWidgetItem;
class QRegularExpression;

namespace RSS
{
    class Feed;
}

namespace Ui
{
    class AutomatedRssDownloader;
}

class AddTorrentParamsWidget;

class AutomatedRssDownloader : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AutomatedRssDownloader)

public:
    explicit AutomatedRssDownloader(QWidget *parent = nullptr);
    ~AutomatedRssDownloader() override;

private slots:
    void onAddRuleBtnClicked();
    void onRemoveRuleBtnClicked();
    void onExportBtnClicked();
    void onImportBtnClicked();
    void onRenameRuleBtnClicked();
    void handleRuleCheckStateChange(QListWidgetItem *ruleItem);
    void handleFeedCheckStateChange(QListWidgetItem *feedItem);
    void displayRulesListMenu();
    void renameSelectedRule();
    void updateRuleDefinitionBox();
    void clearSelectedRuleDownloadedEpisodeList();
    void updateFieldsToolTips(bool regex);
    void updateMustLineValidity();
    void updateMustNotLineValidity();
    void updateEpisodeFilterValidity();
    void handleRuleDefinitionChanged();
    void handleRuleAdded(const QString &ruleName);
    void handleRuleRenamed(const QString &ruleName, const QString &oldRuleName);
    void handleRuleChanged(const QString &ruleName);
    void handleRuleAboutToBeRemoved(const QString &ruleName);

    void handleProcessingStateChanged(bool enabled);

private:
    void loadSettings();
    void saveSettings();
    void createRuleItem(const RSS::AutoDownloadRule &rule);
    void clearRuleDefinitionBox();
    void updateEditedRule();
    void updateMatchingArticles();
    void saveEditedRule();
    void loadFeedList();
    void updateFeedList();
    void addFeedArticlesToTree(RSS::Feed *feed, const QStringList &articles);

    const QString m_formatFilterJSON;
    const QString m_formatFilterLegacy;

    Ui::AutomatedRssDownloader *m_ui = nullptr;
    AddTorrentParamsWidget *m_addTorrentParamsWidget = nullptr;
    QListWidgetItem *m_currentRuleItem = nullptr;
    QSet<std::pair<QString, QString>> m_treeListEntries;
    RSS::AutoDownloadRule m_currentRule;
    QHash<QString, QListWidgetItem *> m_itemsByRuleName;
    QRegularExpression *m_episodeRegex = nullptr;

    SettingValue<QSize> m_storeDialogSize;
    SettingValue<QByteArray> m_storeMainSplitterState;
    SettingValue<QByteArray> m_storeRuleDefSplitterState;
};
