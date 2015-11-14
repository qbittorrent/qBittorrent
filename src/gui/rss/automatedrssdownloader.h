/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
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

#ifndef AUTOMATEDRSSDOWNLOADER_H
#define AUTOMATEDRSSDOWNLOADER_H

#include <QDialog>
#include <QWeakPointer>
#include <QShortcut>
#include <QRegExpValidator>
#include "rssdownloadrule.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class AutomatedRssDownloader;
}
QT_END_NAMESPACE

class RssDownloadRuleList;
class RssManager;

QT_BEGIN_NAMESPACE
class QListWidgetItem;
QT_END_NAMESPACE

class AutomatedRssDownloader : public QDialog
{
  Q_OBJECT

public:
  explicit AutomatedRssDownloader(const QWeakPointer<RssManager>& manager, QWidget *parent = 0);
  ~AutomatedRssDownloader();
  bool isRssDownloaderEnabled() const;

protected slots:
  void loadSettings();
  void saveSettings();
  void loadRulesList();
  void handleFeedCheckStateChange(QListWidgetItem* feed_item);
  void updateRuleDefinitionBox();
  void clearRuleDefinitionBox();
  void saveEditedRule();
  void loadFeedList();
  void updateFeedList();

private slots:
  void displayRulesListMenu(const QPoint& pos);
  void on_addRuleBtn_clicked();
  void on_removeRuleBtn_clicked();
  void on_browseSP_clicked();
  void on_exportBtn_clicked();
  void on_importBtn_clicked();
  void renameSelectedRule();
  void updateMatchingArticles();
  void updateFieldsToolTips(bool regex);
  void updateMustLineValidity();
  void updateMustNotLineValidity();
  void onFinished(int result);

private:
  RssDownloadRulePtr getCurrentRule() const;
  void initLabelCombobox();
  void addFeedArticlesToTree(const RssFeedPtr& feed, const QStringList& articles);

private:
  Ui::AutomatedRssDownloader *ui;
  QWeakPointer<RssManager> m_manager;
  QListWidgetItem* m_editedRule;
  RssDownloadRuleList *m_ruleList;
  RssDownloadRuleList *m_editableRuleList;
  QRegExpValidator *m_episodeValidator;
  QShortcut *editHotkey;
  QShortcut *deleteHotkey;
};

#endif // AUTOMATEDRSSDOWNLOADER_H
