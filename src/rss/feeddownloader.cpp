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

#include "feeddownloader.h"

FeedDownloaderDlg::FeedDownloaderDlg(QWidget *parent, QString feed_url, QString feed_name, Bittorrent* BTSession):
  QDialog(parent), feed_url(feed_url), feed_name(feed_name), BTSession(BTSession), selected_filter(QString::null)
{
  setupUi(this);
  setAttribute(Qt::WA_DeleteOnClose);
  Q_ASSERT(!feed_name.isEmpty());
  rssfeed_lbl->setText(feed_name);
  filters = RssFilters::getFeedFilters(feed_url);
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
  fillFiltersList();
  if(filters.size() > 0) {
    // Select first filter
    filtersList->setCurrentItem(filtersList->item(0));
    //showFilterSettings(filtersList->item(0));
  }
  // Show
  show();
}

FeedDownloaderDlg::~FeedDownloaderDlg() {
  if(enableDl_cb->isChecked())
    emit filteringEnabled();
  // Make sure we save everything
  saveCurrentFilterSettings();
  filters.save();
}

void FeedDownloaderDlg::saveCurrentFilterSettings() {
  if(!selected_filter.isEmpty()) {
    RssFilter filter = filters.getFilter(selected_filter);
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

void FeedDownloaderDlg::on_browse_button_clicked() {
  QString default_path = savepath_line->text();
  if(default_path.isEmpty() || !QDir(default_path).exists()) {
    default_path = QDir::homePath();
  }
  QString dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), QDir::homePath());
  if(!dir.isNull() && QDir(dir).exists()) {
    savepath_line->setText(dir);
  }
}

void FeedDownloaderDlg::fillFiltersList() {
  // Fill filter list
  foreach(QString filter_name, filters.names()) {
    new QListWidgetItem(filter_name, filtersList);
  }
}

void FeedDownloaderDlg::displayFiltersListMenu(const QPoint&) {
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

void FeedDownloaderDlg::showFilterSettings(QListWidgetItem *item) {
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
  RssFilter filter = filters.getFilter(filter_name);
  filterSettingsBox->setEnabled(true);
  match_line->setText(filter.getMatchingTokens_str());
  if(match_line->text().trimmed().isEmpty()) {
    match_line->setText(filter_name);
  }
  notmatch_line->setText(filter.getNotMatchingTokens_str());
  QString save_path = filter.getSavePath();
  if(save_path.isEmpty())
    save_path = BTSession->getDefaultSavePath();
  savepath_line->setText(save_path);
  // Update selected filter
  selected_filter = filter_name;
}

void FeedDownloaderDlg::deleteFilter() {
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

void FeedDownloaderDlg::renameFilter() {
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
        QMessageBox::warning(0, tr("Invalid filter name"), tr("This filter name is already in use."));
      }
    }while(!validated);
    // Save the current filter
    saveCurrentFilterSettings();
    // Rename the filter
    filters.rename(current_name, new_name);
    if(selected_filter == current_name)
      selected_filter = new_name;
    item->setText(new_name);
  }
}

void FeedDownloaderDlg::enableFilterBox(int state) {
  if(state == Qt::Checked) {
    filtersBox->setEnabled(true);
    filters.setDownloadingEnabled(true);
  } else {
    filtersBox->setEnabled(false);
    filters.setDownloadingEnabled(false);
  }
}

QString FeedDownloaderDlg::askFilterName(QString name) {
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
      QMessageBox::warning(0, tr("Invalid filter name"), tr("The filter name cannot be left empty."));
    } else {
      validated = true;
    }
  } while(!validated);
  return new_name;
}

void FeedDownloaderDlg::addFilter() {
  QString filter_name = QString::null;
  bool validated = false;
  do {
    filter_name = askFilterName();
    if(filter_name.isNull()) return;
    if(filters.hasFilter(filter_name)) {
      // Filter alread exists
      QMessageBox::warning(0, tr("Invalid filter name"), tr("This filter name is already in use."));
    } else {
      validated = true;
    }
  }while(!validated);
  QListWidgetItem *it = new QListWidgetItem(filter_name, filtersList);
  filtersList->setCurrentItem(it);
  //showFilterSettings(it);
}

void FeedDownloaderDlg::clearFields() {
  match_line->clear();
  notmatch_line->clear();
  savepath_line->clear();
  test_res_lbl->setText("");
  test_line->clear();
}

void FeedDownloaderDlg::on_testButton_clicked(bool) {
  test_res_lbl->clear();
  if(selected_filter.isEmpty()) {
    qDebug("No filter is selected!!!");
    return;
  }
  QString s = test_line->text().trimmed();
  if(s.isEmpty()) {
    QMessageBox::warning(0, tr("Filter testing error"), tr("Please specify a test torrent name."));
    return;
  }
  // Get current filter
  saveCurrentFilterSettings();
  RssFilter f = filters.getFilter(selected_filter);
  if(f.matches(s))
    test_res_lbl->setText("<b><font color=\"green\">"+tr("matches")+"</font></b>");
  else
    test_res_lbl->setText("<b><font color=\"red\">"+tr("does not match")+"</font></b>");
}

void FeedDownloaderDlg::on_importButton_clicked(bool) {
  QString source = QFileDialog::getOpenFileName(0, tr("Select file to import"), QDir::homePath(),  tr("Filters Files")+QString::fromUtf8(" (*.filters)"));
  if(source.isEmpty()) return;
  if(filters.unserialize(source)) {
    // Clean up first
    clearFields();
    filtersList->clear();
    selected_filter = QString::null;
    fillFiltersList();
    if(filters.size() > 0)
      filtersList->setCurrentItem(filtersList->item(0));
    QMessageBox::information(0, tr("Import successful"), tr("Filters import was successful."));
  } else {
    QMessageBox::warning(0, tr("Import failure"), tr("Filters could not be imported due to an I/O error."));
  }
}

void FeedDownloaderDlg::on_exportButton_clicked(bool) {
  QString destination = QFileDialog::getSaveFileName(this, tr("Select destination file"), QDir::homePath(), tr("Filters Files")+QString::fromUtf8(" (*.filters)"));
  if(destination.isEmpty()) return;
  // Append file extension
  if(!destination.endsWith(".filters"))
    destination += ".filters";
  /*if(QFile::exists(destination)) {
    int ret = QMessageBox::question(0, tr("Overwriting confirmation"), tr("Are you sure you want to overwrite existing file?"), QMessageBox::Yes|QMessageBox::No);
    if(ret != QMessageBox::Yes) return;
  }*/
  if(filters.serialize(destination))
    QMessageBox::information(0, tr("Export successful"), tr("Filters export was successful."));
  else
    QMessageBox::warning(0, tr("Export failure"), tr("Filters could not be exported due to an I/O error."));
}
