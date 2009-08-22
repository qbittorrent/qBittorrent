/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez, Arnaud Demaiziere
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
 * Contact : chris@qbittorrent.org arnaud@qbittorrent.org
 */

#include "rss.h"
#include <QTimer>

/** RssFolder **/

RssFolder::RssFolder(RssFolder *parent, RssManager *rssmanager, bittorrent *BTSession, QString name): parent(parent), rssmanager(rssmanager), BTSession(BTSession), name(name) {
  downloader = new downloadThread(this);
  connect(downloader, SIGNAL(downloadFinished(QString, QString)), this, SLOT(processFinishedDownload(QString, QString)));
  connect(downloader, SIGNAL(downloadFailure(QString, QString)), this, SLOT(handleDownloadFailure(QString, QString)));
}

RssFolder::~RssFolder() {
  qDebug("Deleting downloader thread");
  qDeleteAll(this->values());
  delete downloader;
}

unsigned int RssFolder::getNbUnRead() const {
  // FIXME
  return 0;
}

RssFile::FileType RssFolder::getType() const {
  return RssFile::FOLDER;
}

QStringList RssFolder::getPath() const {
  QStringList path;
  if(parent) {
    path = parent->getPath();
    path.append(name);
  }
  return path;
}

void RssFolder::refreshAll(){
  qDebug("Refreshing all rss feeds");
  QList<RssFile*> items = this->values();
  for(int i=0; i<items.size(); ++i) {
    //foreach(RssFile *item, *this){
    RssFile *item = items.at(i);
    if(item->getType() == RssFile::STREAM) {
      RssStream* stream = (RssStream*) item;
      QString url = stream->getUrl();
      if(stream->isLoading()) return;
      stream->setLoading(true);
      downloader->downloadUrl(url);
      if(!stream->hasCustomIcon()){
        downloader->downloadUrl(stream->getIconUrl());
      }
    } else {
      RssFolder *folder = (RssFolder*)item;
      folder->refreshAll();
    }
  }
}

void RssFolder::removeFile(QStringList full_path) {
  QString name = full_path.last();
  if(full_path.size() == 1) {
    Q_ASSERT(this->contains(name));
    delete this->take(name);
  } else {
    QString subfolder_name = full_path.takeFirst();
    Q_ASSERT(this->contains(subfolder_name));
    RssFolder *subfolder = (RssFolder*)this->value(subfolder_name);
    subfolder->removeFile(full_path);
  }
}

RssFolder* RssFolder::addFolder(QStringList full_path) {
  QString name = full_path.last();
  if(full_path.size() == 1) {
    Q_ASSERT(!this->contains(name));
    RssFolder *subfolder = new RssFolder(this, rssmanager, BTSession, name);
    (*this)[name] = subfolder;
    return subfolder;
  } else {
    QString subfolder_name = full_path.takeFirst();
    // Check if the subfolder exists and create it if it does not
    if(!this->contains(subfolder_name)) {
      qDebug("Creating subfolder %s which did not exist", subfolder_name.toLocal8Bit().data());
      (*this)[subfolder_name] = new RssFolder(this, rssmanager, BTSession, subfolder_name);
    }
    Q_ASSERT(this->contains(subfolder_name));
    RssFolder *subfolder = (RssFolder*)this->value(subfolder_name);
    return subfolder->addFolder(full_path);
  }
}

RssStream* RssFolder::addStream(QStringList full_path) {
  QString url = full_path.last();
  if(full_path.size() == 1) {
    if(this->contains(url)){
      qDebug("Not adding the Rss stream because it is already in the list");
      return 0;
    }
    RssStream* stream = new RssStream(this, rssmanager, BTSession, url);
    (*this)[url] = stream;
    refresh(full_path);
    return stream;
  } else {
    QString subfolder_name = full_path.takeFirst();
    // Check if the subfolder exists and create it if it does not
    if(!this->contains(subfolder_name)) {
      qDebug("Creating subfolder %s which did not exist", subfolder_name.toLocal8Bit().data());
      (*this)[subfolder_name] = new RssFolder(this, rssmanager, BTSession, subfolder_name);
    }
    Q_ASSERT(this->contains(subfolder_name));
    RssFolder *subfolder = (RssFolder*)this->value(subfolder_name);
    return subfolder->addStream(full_path);
  }
}

void RssFolder::refresh(QStringList full_path) {
  QString url = full_path.last();
  if(full_path.size() == 1) {
    qDebug("Refreshing feed: %s", url.toLocal8Bit().data());
    Q_ASSERT(this->contains(url));
    RssStream *stream = (RssStream*)this->value(url);
    if(stream->isLoading()) return;
    stream->setLoading(true);
    downloader->downloadUrl(url);
    if(!stream->hasCustomIcon()){
      downloader->downloadUrl(stream->getIconUrl());
    }else{
      qDebug("No need to download this feed's icon, it was already downloaded");
    }
  } else {
    QString subfolder_name = full_path.takeFirst();
    Q_ASSERT(this->contains(subfolder_name));
    RssFolder *subfolder = (RssFolder*)this->value(subfolder_name);
    subfolder->refresh(full_path);
  }
}

RssFile* RssFolder::getFile(QStringList full_path) const {
  QString name = full_path.last();
  if(full_path.size() == 1) {
    Q_ASSERT(this->contains(name));
    return (*this)[name];
  } else {
    QString subfolder_name = full_path.takeFirst();
    Q_ASSERT(this->contains(subfolder_name));
    RssFolder *subfolder = (RssFolder*)this->value(subfolder_name);
    return subfolder->getFile(full_path);
  }
}

QList<RssFile*> RssFolder::getContent() const {
  return this->values();
}

unsigned int RssFolder::getNbFeeds() const {
  unsigned int nbFeeds = 0;
  foreach(RssFile* item, this->values()) {
    if(item->getType() == RssFile::FOLDER)
      nbFeeds += ((RssFolder*)item)->getNbFeeds();
    else
      nbFeeds += 1;
  }
  return nbFeeds;
}

void RssFolder::processFinishedDownload(QString url, QString path) {
  if(url.endsWith("favicon.ico")){
    // Icon downloaded
    QImage fileIcon;
    if(fileIcon.load(path)) {
      QList<RssStream*> res = findFeedsWithIcon(url);
      RssStream* stream;
      foreach(stream, res){
        stream->setIconPath(path);
        if(!stream->isLoading())
          rssmanager->forwardFeedIconChanged(stream->getUrl(), stream->getIconPath());
      }
    }else{
      qDebug("Unsupported icon format at %s", (const char*)url.toLocal8Bit());
    }
    return;
  }
  RssStream *stream = (RssStream*)this->value(url, 0);
  if(!stream){
    qDebug("This rss stream was deleted in the meantime, nothing to update");
    return;
  }
  stream->processDownloadedFile(path);
  stream->setLoading(false);
  // If the feed has no alias, then we use the title as Alias
  // this is more user friendly
  if(stream->getName().isEmpty()){
    if(!stream->getTitle().isEmpty())
      stream->rename(QStringList(), stream->getTitle());
  }
  rssmanager->forwardFeedInfosChanged(url, stream->getName(), stream->getNbUnRead());
}

void RssFolder::handleDownloadFailure(QString url, QString reason) {
  if(url.endsWith("favicon.ico")){
    // Icon download failure
    qDebug("Could not download icon at %s, reason: %s", (const char*)url.toLocal8Bit(), (const char*)reason.toLocal8Bit());
    return;
  }
  RssStream *stream = (RssStream*)this->value(url, 0);
  if(!stream){
    qDebug("This rss stream was deleted in the meantime, nothing to update");
    return;
  }
  stream->setLoading(false);
  qDebug("Could not download Rss at %s, reason: %s", (const char*)url.toLocal8Bit(), (const char*)reason.toLocal8Bit());
  stream->setDownloadFailed();
  rssmanager->forwardFeedInfosChanged(url, stream->getName(), stream->getNbUnRead());
}

QList<RssStream*> RssFolder::findFeedsWithIcon(QString icon_url) const {
  QList<RssStream*> res;
  RssFile* item;
  foreach(item, this->values()){
    if(item->getType() == RssFile::STREAM && ((RssStream*)item)->getIconUrl() == icon_url)
      res << (RssStream*)item;
  }
  return res;
}

QString RssFolder::getName() const {
  return name;
}

void RssFolder::rename(QStringList full_path, QString new_name) {
  if(full_path.size() == 1) {
    name = new_name;
  } else {
    QString child_name = full_path.takeFirst();
    Q_ASSERT(this->contains(child_name));
    RssFile *child = (RssFile*)this->value(child_name);
    if(full_path.empty()) {
      // Child is renamed, update QHash
      Q_ASSERT(!this->contains(new_name));
      (*this)[new_name] = this->take(child_name);
    }
    child->rename(full_path, new_name);
  }
}

void RssFolder::markAllAsRead() {
  foreach(RssFile *item, this->values()) {
    item->markAllAsRead();
  }
}

QList<RssStream*> RssFolder::getAllFeeds() const {
  QList<RssStream*> streams;
  foreach(RssFile *item, this->values()) {
    if(item->getType() == RssFile::STREAM) {
      streams << ((RssStream*)item);
    } else {
      foreach(RssStream* stream, ((RssFolder*)item)->getAllFeeds()) {
        streams << stream;
      }
    }
  }
  return streams;
}

void RssFolder::removeFileRef(RssFile* item) {
  if(item->getType() == RssFile::STREAM) {
    Q_ASSERT(this->contains(((RssStream*)item)->getUrl()));
    this->remove(((RssStream*)item)->getUrl());
  } else {
    Q_ASSERT(this->contains(((RssFolder*)item)->getName()));
    this->remove(((RssFolder*)item)->getName());
  }
}

void RssFolder::addFile(RssFile * item) {
  if(item->getType() == RssFile::STREAM) {
    Q_ASSERT(!this->contains(((RssStream*)item)->getUrl()));
    (*this)[((RssStream*)item)->getUrl()] = item;
  } else {
    Q_ASSERT(!this->contains(((RssFolder*)item)->getName()));
    (*this)[((RssFolder*)item)->getName()] = item;
  }
}

/** RssManager **/

RssManager::RssManager(bittorrent *BTSession): RssFolder(0, this, BTSession, QString::null) {
  loadStreamList();
  connect(&newsRefresher, SIGNAL(timeout()), this, SLOT(refreshAll()));
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  refreshInterval = settings.value(QString::fromUtf8("Preferences/RSS/RSSRefresh"), 5).toInt();
  newsRefresher.start(refreshInterval*60000);
}

RssManager::~RssManager(){
  qDebug("Deleting RSSManager");
  saveStreamList();
  qDebug("RSSManager deleted");
}

void RssManager::loadStreamList(){
  QSettings settings("qBittorrent", "qBittorrent");
  QStringList streamsUrl = settings.value("Rss/streamList").toStringList();
  QStringList aliases =  settings.value("Rss/streamAlias").toStringList();
  if(streamsUrl.size() != aliases.size()){
    std::cerr << "Corrupted Rss list, not loading it\n";
    return;
  }
  unsigned int i = 0;
  foreach(QString s, streamsUrl){
    QStringList path = s.split("\\");
    if(path.empty()) continue;
    RssStream *stream = this->addStream(path);
    QString alias = aliases.at(i);
    if(!alias.isEmpty()) {
      stream->rename(QStringList(), alias);
    }
    ++i;
  }
  qDebug("NB RSS streams loaded: %d", streamsUrl.size());
}

void RssManager::forwardFeedInfosChanged(QString url, QString aliasOrUrl, unsigned int nbUnread) {
  emit feedInfosChanged(url, aliasOrUrl, nbUnread);
}

void RssManager::forwardFeedIconChanged(QString url, QString icon_path) {
  emit feedIconChanged(url, icon_path);
}

void RssManager::moveFile(QStringList old_path, QStringList new_path) {
  RssFile* item = getFile(old_path);
  RssFolder* src_folder = item->getParent();
  QString new_name = new_path.takeLast();
  RssFolder* dest_folder = (RssFolder*)getFile(new_path);
  dest_folder->addFile(item);
  src_folder->removeFileRef(item);
}

void RssManager::saveStreamList(){
  QList<QPair<QString, QString> > streamsList;
  QStringList streamsUrl;
  QStringList aliases;
  QList<RssStream*> streams = getAllFeeds();
  foreach(RssStream *stream, streams) {
    streamsUrl << stream->getPath().join("\\");
    aliases << stream->getName();
  }
  QSettings settings("qBittorrent", "qBittorrent");
  settings.beginGroup("Rss");
  // FIXME: Empty folder are not saved
  settings.setValue("streamList", streamsUrl);
  settings.setValue("streamAlias", aliases);
  settings.endGroup();
}

/** RssStream **/

RssStream::RssStream(RssFolder* parent, RssManager *rssmanager, bittorrent *BTSession, QString _url): parent(parent), rssmanager(rssmanager), BTSession(BTSession), url(_url), alias(""), iconPath(":/Icons/rss16.png"), refreshed(false), downloadFailure(false), currently_loading(false) {
  qDebug("RSSStream constructed");
  QSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  QHash<QString, QVariant> all_old_items = qBTRSS.value("old_items", QHash<QString, QVariant>()).toHash();
  QVariantList old_items = all_old_items.value(url, QVariantList()).toList();
  qDebug("Loading %d old items for feed %s", old_items.size(), getName().toLocal8Bit().data());
  foreach(const QVariant &var_it, old_items) {
    QHash<QString, QVariant> item = var_it.toHash();
    RssItem *rss_item = RssItem::fromHash(item);
    if(rss_item->isValid())
      listItem << rss_item;
  }
}

RssStream::~RssStream(){
  if(refreshed) {
    QSettings qBTRSS("qBittorrent", "qBittorrent-rss");
    QVariantList old_items;
    foreach(RssItem *item, listItem) {
      old_items << item->toHash();
    }
    qDebug("Saving %d old items for feed %s", old_items.size(), getName().toLocal8Bit().data());
    QHash<QString, QVariant> all_old_items = qBTRSS.value("old_items", QHash<QString, QVariant>()).toHash();
    all_old_items[url] = old_items;
    qBTRSS.setValue("old_items", all_old_items);
  }
  removeAllItems();
  if(QFile::exists(filePath))
    QFile::remove(filePath);
  if(QFile::exists(iconPath) && !iconPath.startsWith(":/"))
    QFile::remove(iconPath);
}

RssFile::FileType RssStream::getType() const {
  return RssFile::STREAM;
}

void RssStream::refresh() {
  QStringList path;
  path << url;
  parent->refresh(path);
}

QStringList RssStream::getPath() const {
  QStringList path = parent->getPath();
  path.append(url);
  return path;
}

// delete all the items saved
void RssStream::removeAllItems() {
  qDeleteAll(listItem);
  listItem.clear();
}

bool RssStream::itemAlreadyExists(QString hash) {
  RssItem * item;
  foreach(item, listItem) {
    if(item->getHash() == hash) return true;
  }
  return false;
}

void RssStream::setLoading(bool val) {
  currently_loading = val;
}

bool RssStream::isLoading() {
  return currently_loading;
}

QString RssStream::getTitle() const{
  return title;
}

void RssStream::rename(QStringList, QString new_name){
  qDebug("Renaming stream to %s", new_name.toLocal8Bit().data());
  alias = new_name;
}

// Return the alias if the stream has one, the url if it has no alias
QString RssStream::getName() const{
  if(!alias.isEmpty()) {
    qDebug("getName() returned alias: %s", (const char*)alias.toLocal8Bit());
    return alias;
  }
  if(!title.isEmpty()) {
    qDebug("getName() returned title: %s", (const char*)title.toLocal8Bit());
    return title;
  }
  qDebug("getName() returned url: %s", (const char*)url.toLocal8Bit());
  return url;
}

QString RssStream::getLink() const{
  return link;
}

QString RssStream::getUrl() const{
  return url;
}

QString RssStream::getDescription() const{
  return description;
}

QString RssStream::getImage() const{
  return image;
}

QString RssStream::getFilePath() const{
  return filePath;
}

QString RssStream::getIconPath() const{
  if(downloadFailure)
    return ":/Icons/oxygen/unavailable.png";
  return iconPath;
}

bool RssStream::hasCustomIcon() const{
  return !iconPath.startsWith(":/");
}

void RssStream::setIconPath(QString path) {
  iconPath = path;
}

RssItem* RssStream::getItem(unsigned int index) const{
  return listItem.at(index);
}

unsigned int RssStream::getNbNews() const{
  return listItem.size();
}

void RssStream::markAllAsRead() {
  RssItem *item;
  foreach(item, listItem){
    if(!item->isRead())
      item->setRead();
  }
}

unsigned int RssStream::getNbUnRead() const{
  unsigned int nbUnread=0;
  RssItem *item;
  foreach(item, listItem){
    if(!item->isRead())
      ++nbUnread;
  }
  return nbUnread;
}

QList<RssItem*> RssStream::getNewsList() const{
  return listItem;
}

// download the icon from the adress
QString RssStream::getIconUrl() {
  QUrl siteUrl(url);
  return QString::fromUtf8("http://")+siteUrl.host()+QString::fromUtf8("/favicon.ico");
}

// read and create items from a rss document
short RssStream::readDoc(const QDomDocument& doc) {
  // is it a rss file ?
  QDomElement root = doc.documentElement();
  if(root.tagName() == QString::fromUtf8("html")){
    qDebug("the file is empty, maybe the url is invalid or the server is too busy");
    return -1;
  }
  else if(root.tagName() != QString::fromUtf8("rss")){
    qDebug("the file is not a rss stream, <rss> omitted: %s", root.tagName().toLocal8Bit().data());
    return -1;
  }
  QDomNode rss = root.firstChild();
  QDomElement channel = root.firstChild().toElement();

  while(!channel.isNull()) {
    // we are reading the rss'main info
    if (channel.tagName() == "channel") {
      QDomElement property = channel.firstChild().toElement();
      while(!property.isNull()) {
        if (property.tagName() == "title") {
          title = property.text();
          if(alias==getUrl())
            rename(QStringList(), title);
        }
        else if (property.tagName() == "link")
          link = property.text();
        else if (property.tagName() == "description")
          description = property.text();
        else if (property.tagName() == "image")
          image = property.text();
        else if(property.tagName() == "item") {
          RssItem * item = new RssItem(property);
          if(item->isValid() && !itemAlreadyExists(item->getHash())) {
            listItem.append(item);
            // Check if the item should be automatically downloaded
            FeedFilter * matching_filter = FeedFilters::getFeedFilters(url).matches(item->getTitle());
            if(matching_filter != 0) {
              // Download the torrent
              BTSession->addConsoleMessage(tr("Automatically downloading %1 torrent from %2 RSS feed...").arg(item->getTitle()).arg(getName()));
              if(matching_filter->isValid()) {
                QString save_path = matching_filter->getSavePath();
                if(save_path.isEmpty())
                  BTSession->downloadUrlAndSkipDialog(item->getTorrentUrl());
                else
                  BTSession->downloadUrlAndSkipDialog(item->getTorrentUrl(), save_path);
              } else {
                // All torrents are downloaded from this feed
                BTSession->downloadUrlAndSkipDialog(item->getTorrentUrl());
              }
              // Item was downloaded, consider it as Read
              item->setRead();
              // Clean up
              delete matching_filter;
            }
          } else {
            delete item;
          }
        }
        property = property.nextSibling().toElement();
      }
    }
    channel = channel.nextSibling().toElement();
  }
  sortList();
  resizeList();
  return 0;
}

void RssStream::insertSortElem(QList<RssItem*> &list, RssItem *item) {
  int i = 0;
  while(i < list.size() && item->getDate() < list.at(i)->getDate()) {
    ++i;
  }
  list.insert(i, item);
}

void RssStream::sortList() {
  QList<RssItem*> new_list;
  RssItem *item;
  foreach(item, listItem) {
    insertSortElem(new_list, item);
  }
  listItem = new_list;
}

void RssStream::resizeList() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  unsigned int max_articles = settings.value(QString::fromUtf8("Preferences/RSS/RSSMaxArticlesPerFeed"), 100).toInt();
  int excess = listItem.size() - max_articles;
  if(excess <= 0) return;
  for(int i=0; i<excess; ++i){
    delete listItem.takeLast();
  }
}

// existing and opening test after download
short RssStream::openRss(){
  qDebug("openRss() called");
  QDomDocument doc("Rss Seed");
  QFile fileRss(filePath);
  if(!fileRss.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qDebug("openRss error: open failed, no file or locked, %s", (const char*)filePath.toLocal8Bit());
    if(QFile::exists(filePath)) {
      fileRss.remove();
    }
    return -1;
  }
  if(!doc.setContent(&fileRss)) {
    qDebug("can't read temp file, might be empty");
    fileRss.close();
    if(QFile::exists(filePath)) {
      fileRss.remove();
    }
    return -1;
  }
  // start reading the xml
  short return_lecture = readDoc(doc);
  fileRss.close();
  if(QFile::exists(filePath)) {
    fileRss.remove();
  }
  return return_lecture;
}

// read and store the downloaded rss' informations
void RssStream::processDownloadedFile(QString file_path) {
  filePath = file_path;
  downloadFailure = false;
  if(openRss() >= 0) {
    refreshed = true;
  } else {
    qDebug("OpenRss: Feed update Failed");
  }
}

void RssStream::setDownloadFailed(){
  downloadFailure = true;
}
