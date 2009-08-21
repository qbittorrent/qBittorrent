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

#ifndef FEEDDOWNLOADER_H
#define FEEDDOWNLOADER_H

#include <QString>
#include <QHash>
#include <QSettings>
#include <QListWidget>
#include <QListWidgetItem>
#include <QInputDialog>
#include <QMessageBox>
#include <QRegExp>
#include <QMenu>

#include "bittorrent.h"
#include "ui_FeedDownloader.h"

class FeedFilter: public QHash<QString, QVariant> {
private:
  bool valid;
public:
  FeedFilter():valid(true) {}
  FeedFilter(bool valid): valid(valid) {}
  FeedFilter(QHash<QString, QVariant> filter): QHash<QString, QVariant>(filter), valid(true) {}

  bool matches(QString s) {
    QStringList match_tokens = getMatchingTokens();
    foreach(const QString& token, match_tokens) {
      if(token.isEmpty() || token == "*")
        continue;
      QRegExp reg(token, Qt::CaseInsensitive);
      if(reg.indexIn(s) < 0) return false;
    }
    // Checking not matching
    QStringList notmatch_tokens = getNotMatchingTokens();
    foreach(const QString& token, notmatch_tokens) {
      if(token.isEmpty()) continue;
      QRegExp reg(token, Qt::CaseInsensitive);
      if(reg.indexIn(s) > -1) return false;
    }
    return true;
  }

  bool isValid() const {
    return valid;
  }

  QStringList getMatchingTokens() const {
    QString matches = this->value("matches", "*").toString();
    return matches.split(" ");
  }

  QString getMatchingTokens_str() const {
    return this->value("matches", "*").toString();
  }

  void setMatchingTokens(QString tokens) {
    tokens = tokens.trimmed();
    if(tokens.isEmpty())
      (*this)["matches"] = "*";
    else
      (*this)["matches"] = tokens;
  }

  QStringList getNotMatchingTokens() const {
    QString notmatching = this->value("not", "").toString();
    return notmatching.split(" ");
  }

  QString getNotMatchingTokens_str() const {
    return this->value("not", "").toString();
  }

  void setNotMatchingTokens(QString tokens) {
    (*this)["not"] = tokens.trimmed();
  }

  QString getSavePath() const {
    return this->value("save_path", "").toString();
  }

  void setSavePath(QString save_path) {
    (*this)["save_path"] = save_path;
  }

};

class FeedFilters : public QHash<QString, QVariant> {

private:
  QString feed_url;

public:
  FeedFilters() {}
  FeedFilters(QString feed_url, QHash<QString, QVariant> filters): QHash<QString, QVariant>(filters), feed_url(feed_url) {}

  bool hasFilter(QString name) const {
    return this->contains(name);
  }

  FeedFilter* matches(QString s) {
    if(!isDownloadingEnabled()) return 0;
    if(this->size() == 0) return new FeedFilter(false);
    foreach(QVariant var_hash_filter, this->values()) {
      QHash<QString, QVariant> hash_filter = var_hash_filter.toHash();
      FeedFilter *filter = new FeedFilter(hash_filter);
      if(filter->matches(s))
        return filter;
      else
        delete filter;
    }
    return 0;
  }

  QStringList names() const {
    return this->keys();
  }

  FeedFilter getFilter(QString name) const {
    if(this->contains(name))
      return FeedFilter(this->value(name).toHash());
    return FeedFilter();
  }

  void setFilter(QString name, FeedFilter f) {
    (*this)[name] = f;
  }

  bool isDownloadingEnabled() const {
    QSettings qBTRSS("qBittorrent", "qBittorrent-rss");
    QHash<QString, QVariant> feeds_w_downloader = qBTRSS.value("downloader_on", QHash<QString, QVariant>()).toHash();
    return feeds_w_downloader.value(feed_url, false).toBool();
  }

  void setDownloadingEnabled(bool enabled) {
    QSettings qBTRSS("qBittorrent", "qBittorrent-rss");
    QHash<QString, QVariant> feeds_w_downloader = qBTRSS.value("downloader_on", QHash<QString, QVariant>()).toHash();
    feeds_w_downloader[feed_url] = enabled;
    qBTRSS.setValue("downloader_on", feeds_w_downloader);
  }

  static FeedFilters getFeedFilters(QString url) {
    QSettings qBTRSS("qBittorrent", "qBittorrent-rss");
    QHash<QString, QVariant> all_feeds_filters = qBTRSS.value("feed_filters", QHash<QString, QVariant>()).toHash();
    return FeedFilters(url, all_feeds_filters.value(url, QHash<QString, QVariant>()).toHash());
  }

  void rename(QString old_name, QString new_name) {
    Q_ASSERT(this->contains(old_name));
    (*this)[new_name] = this->take(old_name);
  }

  void save() {
    QSettings qBTRSS("qBittorrent", "qBittorrent-rss");
    QHash<QString, QVariant> all_feeds_filters = qBTRSS.value("feed_filters", QHash<QString, QVariant>()).toHash();
    qDebug("Saving filters for feed: %s (%d filters)", feed_url.toLocal8Bit().data(), (*this).size());
    all_feeds_filters[feed_url] = *this;
    qBTRSS.setValue("feed_filters", all_feeds_filters);
  }
};

class FeedDownloaderDlg : public QDialog, private Ui_FeedDownloader{
  Q_OBJECT

private:
  QString feed_url;
  QString feed_name;
  FeedFilters filters;
  bittorrent *BTSession;
  QString selected_filter; // name

public:
  FeedDownloaderDlg(QWidget *parent, QString feed_url, QString feed_name, bittorrent* BTSession): QDialog(parent), feed_url(feed_url), feed_name(feed_name), BTSession(BTSession), selected_filter(QString::null){
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    Q_ASSERT(!feed_name.isEmpty());
    rssfeed_lbl->setText(feed_name);
    filters = FeedFilters::getFeedFilters(feed_url);
    // Connect Signals/Slots
    connect(filtersList, SIGNAL(currentItemChanged(QListWidgetItem* , QListWidgetItem *)), this, SLOT(showFilterSettings(QListWidgetItem *)));
    connect(filtersList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayFiltersListMenu(const QPoint&)));
    connect(actionAdd_filter, SIGNAL(triggered()), this, SLOT(addFilter()));
    connect(actionRemove_filter, SIGNAL(triggered()), this, SLOT(deleteFilter()));
    connect(actionRename_filter, SIGNAL(triggered()), this, SLOT(renameFilter()));
    connect(del_button, SIGNAL(clicked(bool)), this, SLOT(deleteFilter()));
    connect(add_button, SIGNAL(clicked(bool)), this, SLOT(addFilter()));
    connect(enableDl_cb, SIGNAL(stateChanged(int)), this, SLOT(enableFilterBox(int)));
    // Restore saved info
    enableDl_cb->setChecked(filters.isDownloadingEnabled());
    // Fill filter list
    foreach(QString filter_name, filters.names()) {
      new QListWidgetItem(filter_name, filtersList);
    }
    if(filters.size() > 0) {
      // Select first filter
      filtersList->setCurrentItem(filtersList->item(0));
      //showFilterSettings(filtersList->item(0));
    }
    // Show
    show();
  }

  ~FeedDownloaderDlg() {
    // Make sure we save everything
    saveCurrentFilterSettings();
    filters.save();
  }

protected slots:
  void saveCurrentFilterSettings() {
    if(!selected_filter.isEmpty()) {
      FeedFilter filter = filters.getFilter(selected_filter);
      filter.setMatchingTokens(match_line->text());
      filter.setNotMatchingTokens(notmatch_line->text());
      QString save_path = savepath_line->text().trimmed();
      if(save_path.isEmpty())
        save_path = BTSession->getDefaultSavePath();
      filter.setSavePath(save_path);
      // Save updated filter
      filters.setFilter(selected_filter, filter);
    }
  }

  void displayFiltersListMenu(const QPoint&) {
    QMenu myFiltersListMenu(this);
    if(filtersList->selectedItems().size() > 0) {
      myFiltersListMenu.addAction(actionRename_filter);
      myFiltersListMenu.addAction(actionRemove_filter);
    } else {
      myFiltersListMenu.addAction(actionAdd_filter);
    }
    // Call menu
    myFiltersListMenu.exec(QCursor::pos());
  }

  void showFilterSettings(QListWidgetItem *item) {
    // First, save current filter settings
    saveCurrentFilterSettings();
    // Clear all fields
    clearFields();
    if(!item) {
      qDebug("No new selected item");
      return;
    }
    // Actually show filter settings
    QString filter_name = item->text();
    FeedFilter filter = filters.getFilter(filter_name);
    filterSettingsBox->setEnabled(true);
    match_line->setText(filter.getMatchingTokens_str());
    notmatch_line->setText(filter.getNotMatchingTokens_str());
    QString save_path = filter.getSavePath();
    if(save_path.isEmpty())
      save_path = BTSession->getDefaultSavePath();
    savepath_line->setText(save_path);
    // Update selected filter
    selected_filter = filter_name;
  }

  void deleteFilter() {
    QList<QListWidgetItem *> items = filtersList->selectedItems();
    if(items.size() == 1) {
      QListWidgetItem * item = items.first();
      filters.remove(item->text());
      selected_filter = QString::null;
      delete item;
      // Reset Filter settings view
      if(filters.size() == 0) {
        clearFields();
        filterSettingsBox->setEnabled(false);
      }
    }
  }

  void renameFilter() {
    QList<QListWidgetItem *> items = filtersList->selectedItems();
    if(items.size() == 1) {
      QListWidgetItem *item = items.first();
      QString current_name = item->text();
      QString new_name;
      bool validated = false;
      do {
        new_name = askFilterName(current_name);
        if(new_name.isNull() || new_name == current_name) return;
        if(!filters.hasFilter(new_name)) {
          validated = true;
        } else {
          QMessageBox::critical(0, tr("Invalid filter name"), tr("This filter name is already in use."));
        }
      }while(!validated);
      // Rename the filter
      filters.rename(current_name, new_name);
      if(selected_filter == current_name)
        selected_filter = new_name;
      item->setText(new_name);
    }
  }

  void enableFilterBox(int state) {
    if(state == Qt::Checked) {
      filtersBox->setEnabled(true);
      filters.setDownloadingEnabled(true);
    } else {
      filtersBox->setEnabled(false);
      filters.setDownloadingEnabled(false);
    }
  }

  QString askFilterName(QString name=QString::null) {
    QString name_prop;
    if(name.isEmpty())
      name_prop = tr("New filter");
    else
      name_prop = name;
    QString new_name;
    bool validated = false;
    do {
      bool ok;
      new_name = QInputDialog::getText(this, tr("Please choose a name for this filter"), tr("Filter name:"), QLineEdit::Normal, name_prop, &ok);
      if(!ok) {
        return QString::null;
      }
      // Validate filter name
      new_name = new_name.trimmed();
      if(new_name.isEmpty()) {
        // Cannot be left empty
        QMessageBox::critical(0, tr("Invalid filter name"), tr("The filter name cannot be left empty."));
      } else {
        validated = true;
      }
    } while(!validated);
    return new_name;
  }

  void addFilter() {
    QString filter_name = QString::null;
    bool validated = false;
    do {
      filter_name = askFilterName();
      if(filter_name.isNull()) return;
      if(filters.hasFilter(filter_name)) {
        // Filter alread exists
        QMessageBox::critical(0, tr("Invalid filter name"), tr("This filter name is already in use."));
      } else {
        validated = true;
      }
    }while(!validated);
    QListWidgetItem *it = new QListWidgetItem(filter_name, filtersList);
    filtersList->setCurrentItem(it);
    //showFilterSettings(it);
  }

  void clearFields() {
    match_line->clear();
    notmatch_line->clear();
    savepath_line->clear();
    test_res_lbl->setText("");
    test_line->clear();
  }

  void on_testButton_clicked(bool) {
    if(selected_filter.isEmpty()) return;
    QString s = test_line->text().trimmed();
    if(s.isEmpty()) {
      QMessageBox::critical(0, tr("Filter testing error"), tr("Please specify a test torrent name."));
      return;
    }
    // Get current filter
    saveCurrentFilterSettings();
    FeedFilter f = filters.getFilter(selected_filter);
    if(f.matches(s))
      test_res_lbl->setText("<b><font color=\"green\">"+tr("matches")+"</font></b>");
    else
      test_res_lbl->setText("<b><font color=\"red\">"+tr("does not match")+"</font></b>");
  }

};

#endif // FEEDDOWNLOADER_H
