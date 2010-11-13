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

#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QDebug>

#include "automatedrssdownloader.h"
#include "ui_automatedrssdownloader.h"
#include "rssfilters.h"
#include "rsssettings.h"
#include "rssdownloadrulelist.h"
#include "preferences.h"
#include "qinisettings.h"

AutomatedRssDownloader::AutomatedRssDownloader(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::AutomatedRssDownloader)
{
  ui->setupUi(this);
  ui->listRules->setSortingEnabled(true);
  m_ruleList = RssDownloadRuleList::instance();
  initLabelCombobox();
  loadFeedList();
  loadSettings();
  connect(ui->listRules, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), SLOT(updateRuleDefinitionBox(QListWidgetItem*,QListWidgetItem*)));
  connect(ui->listRules, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), SLOT(updateFeedList(QListWidgetItem*,QListWidgetItem*)));
  if(ui->listRules->count() > 0)
    ui->listRules->setCurrentRow(0);
  else
    updateRuleDefinitionBox();
}

AutomatedRssDownloader::~AutomatedRssDownloader()
{
  qDebug() << Q_FUNC_INFO;
  // Save current item on exit
  saveCurrentRule(ui->listRules->currentItem());
  saveSettings();
  delete ui;
}

void AutomatedRssDownloader::loadSettings()
{
  // load dialog geometry
  QIniSettings settings("qBittorrent", "qBittorrent");
  restoreGeometry(settings.value("RssFeedDownloader/geometry").toByteArray());
  ui->checkEnableDownloader->setChecked(RssSettings::isRssDownloadingEnabled());
  // Display download rules
  loadRulesList();
}

void AutomatedRssDownloader::saveSettings()
{
  RssSettings::setRssDownloadingEnabled(ui->checkEnableDownloader->isChecked());
  // Save dialog geometry
  QIniSettings settings("qBittorrent", "qBittorrent");
  settings.setValue("RssFeedDownloader/geometry", saveGeometry());
}

void AutomatedRssDownloader::loadRulesList()
{
  foreach (const QString &rule_name, m_ruleList->ruleNames()) {
    QListWidgetItem *item = new QListWidgetItem(rule_name, ui->listRules);
    item->setFlags(item->flags()|Qt::ItemIsUserCheckable);
    if(m_ruleList->getRule(rule_name).isEnabled())
      item->setCheckState(Qt::Checked);
    else
      item->setCheckState(Qt::Unchecked);
  }
}

void AutomatedRssDownloader::loadFeedList()
{
  const QStringList feed_aliases = RssSettings::getRssFeedsAliases();
  const QStringList feed_urls = RssSettings::getRssFeedsUrls();
  for(int i=0; i<feed_aliases.size(); ++i) {
    QListWidgetItem *item = new QListWidgetItem(feed_aliases.at(i), ui->listFeeds);
    item->setData(Qt::UserRole, feed_urls.at(i));
    item->setFlags(item->flags()|Qt::ItemIsUserCheckable);
  }
}

QStringList AutomatedRssDownloader::getSelectedFeeds() const
{
  QStringList feeds;
  for(int i=0; i<ui->listFeeds->count(); ++i) {
    QListWidgetItem *item = ui->listFeeds->item(i);
    if(item->checkState() != Qt::Unchecked)
      feeds << item->data(Qt::UserRole).toString();
  }
  return feeds;
}

void AutomatedRssDownloader::updateFeedList(QListWidgetItem* current, QListWidgetItem* previous)
{
  Q_UNUSED(previous);
  RssDownloadRule rule = getCurrentRule();
  const QStringList affected_feeds = rule.rssFeeds();
  for(int i=0; i<ui->listFeeds->count(); ++i) {
    QListWidgetItem *item = ui->listFeeds->item(i);
    const QString feed_url = item->data(Qt::UserRole).toString();
    if(affected_feeds.contains(feed_url))
      item->setCheckState(Qt::Checked);
    else
      item->setCheckState(Qt::Unchecked);
  }
  ui->listFeeds->setEnabled(current != 0);
}

bool AutomatedRssDownloader::isRssDownloaderEnabled() const
{
  return ui->checkEnableDownloader->isChecked();
}

void AutomatedRssDownloader::updateRuleDefinitionBox(QListWidgetItem* current, QListWidgetItem* previous)
{
  qDebug() << Q_FUNC_INFO << current << previous;
  // Save previous item
  saveCurrentRule(previous);
  // Update rule definition box
  RssDownloadRule rule = getCurrentRule();
  if(rule.isValid()) {
    ui->lineContains->setText(rule.mustContain());
    ui->lineNotContains->setText(rule.mustNotContain());
    ui->saveDiffDir_check->setChecked(!rule.savePath().isEmpty());
    ui->lineSavePath->setText(rule.savePath());
    if(rule.label().isEmpty()) {
      ui->comboLabel->setCurrentIndex(-1);
      ui->comboLabel->clearEditText();
    } else {
      ui->comboLabel->setCurrentIndex(ui->comboLabel->findText(rule.label()));
    }
    // Enable
    ui->ruleDefBox->setEnabled(true);
  } else {
    // Clear
    ui->lineNotContains->clear();
    ui->saveDiffDir_check->setChecked(false);
    ui->lineSavePath->clear();
    ui->comboLabel->clearEditText();
    if(current) {
      // Use the rule name as a default for the "contains" field
      ui->lineContains->setText(current->text());
      ui->ruleDefBox->setEnabled(true);
    } else {
      ui->lineContains->clear();
      ui->ruleDefBox->setEnabled(false);
    }
  }
}

RssDownloadRule AutomatedRssDownloader::getCurrentRule() const
{
  QListWidgetItem * current_item = ui->listRules->currentItem();
  if(current_item)
    return m_ruleList->getRule(current_item->text());
  return RssDownloadRule();
}

void AutomatedRssDownloader::initLabelCombobox()
{
  // Load custom labels
  const QStringList customLabels = Preferences::getTorrentLabels();
  foreach(const QString& label, customLabels) {
    ui->comboLabel->addItem(label);
  }
}

void AutomatedRssDownloader::saveCurrentRule(QListWidgetItem * item)
{
  qDebug() << Q_FUNC_INFO << item;
  if(!item) return;
  RssDownloadRule rule = m_ruleList->getRule(item->text());
  if(!rule.isValid()) {
    rule.setName(item->text());
  }
  if(item->checkState() == Qt::Unchecked)
    rule.setEnabled(false);
  else
    rule.setEnabled(true);
  rule.setMustContain(ui->lineContains->text());
  rule.setMustNotContain(ui->lineNotContains->text());
  if(ui->saveDiffDir_check->isChecked())
    rule.setSavePath(ui->lineSavePath->text());
  else
    rule.setSavePath("");
  rule.setLabel(ui->comboLabel->currentText());
  // Save new label
  if(!rule.label().isEmpty())
    Preferences::addTorrentLabel(rule.label());
  rule.setRssFeeds(getSelectedFeeds());
  // Save it
  m_ruleList->saveRule(rule);
}


void AutomatedRssDownloader::on_addRuleBtn_clicked()
{
  // Ask for a rule name
  const QString rule = QInputDialog::getText(this, tr("New rule name"), tr("Please type the name of the new download rule."));
  if(rule.isEmpty()) return;
  // Check if this rule name already exists
  if(m_ruleList->getRule(rule).isValid()) {
    QMessageBox::warning(this, tr("Rule name conflict"), tr("A rule with this name already exists, please choose another name."));
    return;
  }
  // Add the new rule to the list
  QListWidgetItem * item = new QListWidgetItem(rule, ui->listRules);
  item->setFlags(item->flags()|Qt::ItemIsUserCheckable);
  item->setCheckState(Qt::Checked); // Enable as a default
  ui->listRules->setCurrentItem(item);
}

void AutomatedRssDownloader::on_removeRuleBtn_clicked()
{
  QListWidgetItem * item = ui->listRules->currentItem();
  if(!item) return;
  // Ask for confirmation
  if(QMessageBox::question(this, tr("Rule deletion confirmation"), tr("Are you sure you want to remove the download rule named %1?").arg(item->text())) != QMessageBox::Yes)
    return;
  // Actually remove the item
  ui->listRules->removeItemWidget(item);
  // Clean up memory
  delete item;
}

void AutomatedRssDownloader::on_browseSP_clicked()
{
  QString save_path = QFileDialog::getExistingDirectory(this, tr("Destination directory"), QDir::homePath());
  if(!save_path.isEmpty())
    ui->lineSavePath->setText(save_path);
}

void AutomatedRssDownloader::on_exportBtn_clicked()
{
  if(m_ruleList->isEmpty()) {
    QMessageBox::warning(this, tr("Invalid action"), tr("The list is empty, there is nothing to export."));
    return;
  }
  // Ask for a save path
  QString save_path = QFileDialog::getSaveFileName(this, tr("Where would you like to save the list?"), QDir::homePath(), tr("Rule list (*.rssrules *.filters)"));
  if(save_path.isEmpty()) return;
  if(!save_path.endsWith(".rssrules", Qt::CaseInsensitive))
    save_path += ".rssrules";
  if(!m_ruleList->serialize(save_path)) {
    QMessageBox::warning(this, tr("I/O Error"), tr("Failed to create the destination file"));
    return;
  }
}

void AutomatedRssDownloader::on_importBtn_clicked()
{
  // TODO
}
