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
#include "preferences.h"

#ifdef QT_4_5
#include <QHash>
#else
#include <QMap>
#define QHash QMap
#define toHash toMap
#endif

/** RssFolder **/

RssFolder::RssFolder(RssFolder *parent, RssManager *rssmanager, Bittorrent *BTSession, QString name): parent(parent), rssmanager(rssmanager), BTSession(BTSession), name(name) {
  downloader = new downloadThread(this);
  connect(downloader, SIGNAL(downloadFinished(QString, QString)), this, SLOT(processFinishedDownload(QString, QString)));
  connect(downloader, SIGNAL(downloadFailure(QString, QString)), this, SLOT(handleDownloadFailure(QString, QString)));
}

RssFolder::~RssFolder() {
  qDebug("Deleting a RSS folder, removing elements");
  qDeleteAll(this->values());
  qDebug("Deleting downloader thread");
  delete downloader;
  qDebug("Downloader thread removed");
}

unsigned int RssFolder::getNbUnRead() const {
  unsigned int nb_unread = 0;
  foreach(RssFile *file, this->values()) {
    nb_unread += file->getNbUnRead();
  }
  return nb_unread;
}

RssFile::FileType RssFolder::getType() const {
  return RssFile::FOLDER;
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

void RssFolder::removeFile(QString ID) {
  if(this->contains(ID)) {
    RssFile* child = this->take(ID);
    child->removeAllSettings();
    child->removeAllItems();
    delete child;
  }
}

RssFolder* RssFolder::addFolder(QString name) {
  RssFolder *subfolder;
  if(!this->contains(name)) {
    subfolder = new RssFolder(this, rssmanager, BTSession, name);
    (*this)[name] = subfolder;
  } else {
    subfolder = (RssFolder*)this->value(name);
  }
  return subfolder;
}

RssStream* RssFolder::addStream(QString url) {
  RssStream* stream = new RssStream(this, rssmanager, BTSession, url);
  Q_ASSERT(!this->contains(stream->getUrl()));
  (*this)[stream->getUrl()] = stream;
  refreshStream(stream->getUrl());
  return stream;
}

// Refresh All Children
void RssFolder::refresh() {
  foreach(RssFile *child, this->values()) {
    // Little optimization child->refresh() would work too
    if(child->getType() == RssFile::STREAM)
      refreshStream(child->getID());
    else
      child->refresh();
  }
}

QList<RssItem*> RssFolder::getNewsList() const {
  QList<RssItem*> news;
  foreach(RssFile *child, this->values()) {
    news << child->getNewsList();
  }
  return news;
}

QList<RssItem*> RssFolder::getUnreadNewsList() const {
  QList<RssItem*> unread_news;
  foreach(RssFile *child, this->values()) {
    unread_news << child->getUnreadNewsList();
  }
  return unread_news;
}

void RssFolder::refreshStream(QString url) {
  qDebug("Refreshing feed: %s", url.toLocal8Bit().data());
  Q_ASSERT(this->contains(url));
  RssStream *stream = (RssStream*)this->value(url);
  if(stream->isLoading()) {
    qDebug("Stream %s is already being loaded...", stream->getUrl().toLocal8Bit().data());
    return;
  }
  stream->setLoading(true);
  qDebug("stream %s : loaded=true", stream->getUrl().toLocal8Bit().data());
  downloader->downloadUrl(url);
  if(!stream->hasCustomIcon()){
    downloader->downloadUrl(stream->getIconUrl());
  }else{
    qDebug("No need to download this feed's icon, it was already downloaded");
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
  qDebug("stream %s : loaded=false", stream->getUrl().toLocal8Bit().data());
  // If the feed has no alias, then we use the title as Alias
  // this is more user friendly
  if(stream->getName().isEmpty()){
    if(!stream->getTitle().isEmpty())
      stream->rename(stream->getTitle());
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

void RssFolder::rename(QString new_name) {
  Q_ASSERT(!parent->contains(new_name));
  if(!parent->contains(new_name)) {
    // Update parent
    (*parent)[new_name] = parent->take(name);
    // Actually rename
    name = new_name;
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

void RssFolder::addFile(RssFile * item) {
  if(item->getType() == RssFile::STREAM) {
    Q_ASSERT(!this->contains(((RssStream*)item)->getUrl()));
    (*this)[((RssStream*)item)->getUrl()] = item;
    qDebug("Added feed %s to folder ./%s", ((RssStream*)item)->getUrl().toLocal8Bit().data(), name.toLocal8Bit().data());
  } else {
    Q_ASSERT(!this->contains(((RssFolder*)item)->getName()));
    (*this)[((RssFolder*)item)->getName()] = item;
    qDebug("Added folder %s to folder ./%s", ((RssFolder*)item)->getName().toLocal8Bit().data(), name.toLocal8Bit().data());
  }
  // Update parent
  item->setParent(this);
}

/** RssManager **/

RssManager::RssManager(Bittorrent *BTSession): RssFolder(0, this, BTSession, QString::null) {
  loadStreamList();
  connect(&newsRefresher, SIGNAL(timeout()), this, SLOT(refreshAll()));
  refreshInterval = Preferences::getRSSRefreshInterval();
  newsRefresher.start(refreshInterval*60000);
}

RssManager::~RssManager(){
  qDebug("Deleting RSSManager");
  saveStreamList();
  qDebug("RSSManager deleted");
}

void RssManager::updateRefreshInterval(unsigned int val){
  if(refreshInterval != val) {
    refreshInterval = val;
    newsRefresher.start(refreshInterval*60000);
    qDebug("New RSS refresh interval is now every %dmin", refreshInterval);
  }
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
    QString feed_url = path.takeLast();
    // Create feed path (if it does not exists)
    RssFolder * feed_parent = this;
    foreach(QString folder_name, path) {
      feed_parent = feed_parent->addFolder(folder_name);
    }
    // Create feed
    RssStream *stream = feed_parent->addStream(feed_url);
    QString alias = aliases.at(i);
    if(!alias.isEmpty()) {
      stream->rename(alias);
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

void RssManager::moveFile(RssFile* file, RssFolder* dest_folder) {
  RssFolder* src_folder = file->getParent();
  if(dest_folder != src_folder) {
    // Copy to new Folder
    dest_folder->addFile(file);
    // Remove reference in old folder
    src_folder->remove(file->getID());
  } else {
    qDebug("Nothing to move, same destination folder");
  }
}

void RssManager::saveStreamList(){
  QList<QPair<QString, QString> > streamsList;
  QStringList streamsUrl;
  QStringList aliases;
  QList<RssStream*> streams = getAllFeeds();
  foreach(RssStream *stream, streams) {
    QString stream_path = stream->getPath().join("\\");
    qDebug("Saving stream path: %s", stream_path.toLocal8Bit().data());
    streamsUrl << stream_path;
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

RssStream::RssStream(RssFolder* parent, RssManager *rssmanager, Bittorrent *BTSession, QString _url): parent(parent), rssmanager(rssmanager), BTSession(BTSession), alias(""), iconPath(":/Icons/rss16.png"), refreshed(false), downloadFailure(false), currently_loading(false) {
  qDebug("RSSStream constructed");
  QSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  url = QUrl(_url).toString();
  QHash<QString, QVariant> all_old_items = qBTRSS.value("old_items", QHash<QString, QVariant>()).toHash();
  QVariantList old_items = all_old_items.value(url, QVariantList()).toList();
  qDebug("Loading %d old items for feed %s", old_items.size(), getName().toLocal8Bit().data());
  foreach(const QVariant &var_it, old_items) {
    QHash<QString, QVariant> item = var_it.toHash();
    RssItem *rss_item = RssItem::fromHash(this, item);
    if(rss_item->isValid()) {
      (*this)[rss_item->getTitle()] = rss_item;
    } else {
      delete rss_item;
    }
  }
}

RssStream::~RssStream(){
  qDebug("Deleting a RSS stream: %s", getName().toLocal8Bit().data());
  if(refreshed) {
    QSettings qBTRSS("qBittorrent", "qBittorrent-rss");
    QVariantList old_items;
    foreach(RssItem *item, this->values()) {
      old_items << item->toHash();
    }
    qDebug("Saving %d old items for feed %s", old_items.size(), getName().toLocal8Bit().data());
    QHash<QString, QVariant> all_old_items = qBTRSS.value("old_items", QHash<QString, QVariant>()).toHash();
    all_old_items[url] = old_items;
    qBTRSS.setValue("old_items", all_old_items);
  }
  qDebug("Removing all item from feed");
  removeAllItems();
  qDebug("All items were removed");
  if(QFile::exists(filePath))
    QFile::remove(filePath);
  if(QFile::exists(iconPath) && !iconPath.startsWith(":/"))
    QFile::remove(iconPath);
}

RssFile::FileType RssStream::getType() const {
  return RssFile::STREAM;
}

void RssStream::refresh() {
  parent->refreshStream(url);
}

// delete all the items saved
void RssStream::removeAllItems() {
  qDeleteAll(this->values());
  this->clear();
}

void RssStream::removeAllSettings() {
  QSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  QHash<QString, QVariant> feeds_w_downloader = qBTRSS.value("downloader_on", QHash<QString, QVariant>()).toHash();
  if(feeds_w_downloader.contains(url)) {
    feeds_w_downloader.remove(url);
    qBTRSS.setValue("downloader_on", feeds_w_downloader);
  }
  QHash<QString, QVariant> all_feeds_filters = qBTRSS.value("feed_filters", QHash<QString, QVariant>()).toHash();
  if(all_feeds_filters.contains(url)) {
    all_feeds_filters.remove(url);
    qBTRSS.setValue("feed_filters", all_feeds_filters);
  }
}

bool RssStream::itemAlreadyExists(QString name) {
  return this->contains(name);
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

void RssStream::rename(QString new_name){
  qDebug("Renaming stream to %s", new_name.toLocal8Bit().data());
  alias = new_name;
}

// Return the alias if the stream has one, the url if it has no alias
QString RssStream::getName() const{
  if(!alias.isEmpty()) {
    //qDebug("getName() returned alias: %s", (const char*)alias.toLocal8Bit());
    return alias;
  }
  if(!title.isEmpty()) {
    //qDebug("getName() returned title: %s", (const char*)title.toLocal8Bit());
    return title;
  }
  //qDebug("getName() returned url: %s", (const char*)url.toLocal8Bit());
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

RssItem* RssStream::getItem(QString name) const{
  return this->value(name);
}

unsigned int RssStream::getNbNews() const{
  return this->size();
}

void RssStream::markAllAsRead() {
  foreach(RssItem *item, this->values()){
    item->setRead();
  }
  rssmanager->forwardFeedInfosChanged(url, getName(), 0);
}

unsigned int RssStream::getNbUnRead() const{
  unsigned int nbUnread=0;
  foreach(RssItem *item, this->values()) {
    if(!item->isRead())
      ++nbUnread;
  }
  return nbUnread;
}

QList<RssItem*> RssStream::getNewsList() const{
  return this->values();
}

QList<RssItem*> RssStream::getUnreadNewsList() const {
  QList<RssItem*> unread_news;
  foreach(RssItem *item, this->values()) {
    if(!item->isRead())
      unread_news << item;
  }
  return unread_news;
}

// download the icon from the adress
QString RssStream::getIconUrl() {
  QUrl siteUrl(url);
  return QString::fromUtf8("http://")+siteUrl.host()+QString::fromUtf8("/favicon.ico");
}

// read and create items from a rss document
short RssStream::readDoc(QIODevice* device) {
  qDebug("Parsing RSS file...");
  QXmlStreamReader xml(device);
  // is it a rss file ?
  if (xml.atEnd()) {
    qDebug("ERROR: Could not parse RSS file");
    return -1;
  }
  while (!xml.atEnd()) {
    xml.readNext();
    if(xml.isStartElement()) {
      if(xml.name() != "rss") {
        qDebug("ERROR: this is not a rss file, root tag is <%s>", qPrintable(xml.name().toString()));
        return -1;
      } else {
        break;
      }
    }
  }
  // Read channels
  while(!xml.atEnd()) {
    xml.readNext();

    if(xml.isEndElement())
      break;

    if(xml.isStartElement()) {
      //qDebug("xml.name() == %s", qPrintable(xml.name().toString()));
      if(xml.name() == "channel") {
        qDebug("in channel");

        // Parse channel content
        while(!xml.atEnd()) {
          xml.readNext();

          if(xml.isEndElement() && xml.name() == "channel") {
            break;
          }

          if(xml.isStartElement()) {
            //qDebug("xml.name() == %s", qPrintable(xml.name().toString()));
            if(xml.name() == "title") {
              title = xml.readElementText();
              if(alias == getUrl())
                rename(title);
            }
            else if(xml.name() == "link") {
              link = xml.readElementText();
            }
            else if(xml.name() == "description") {
              description = xml.readElementText();
            }
            else if(xml.name() == "image") {
              image = xml.attributes().value("url").toString();
            }
            else if(xml.name() == "item") {
              RssItem * item = new RssItem(this, xml);
              if(item->isValid() && !itemAlreadyExists(item->getTitle())) {
                this->insert(item->getTitle(), item);
              } else {
                delete item;
              }
            }
          }
        }
        return 0;
      }
    }
  }
  qDebug("XML Error: This is not a valid RSS document");
  return -1;

  resizeList();

  // RSS Feed Downloader
  foreach(RssItem* item, values()) {
    if(item->isRead()) continue;
    QString torrent_url;
    if(item->has_attachment())
      torrent_url = item->getTorrentUrl();
    else
      torrent_url = item->getLink();
    // Check if the item should be automatically downloaded
    FeedFilter * matching_filter = FeedFilters::getFeedFilters(url).matches(item->getTitle());
    if(matching_filter != 0) {
      // Download the torrent
      BTSession->addConsoleMessage(tr("Automatically downloading %1 torrent from %2 RSS feed...").arg(item->getTitle()).arg(getName()));
      if(matching_filter->isValid()) {
        QString save_path = matching_filter->getSavePath();
        if(save_path.isEmpty())
          BTSession->downloadUrlAndSkipDialog(torrent_url);
        else
          BTSession->downloadUrlAndSkipDialog(torrent_url, save_path);
      } else {
        // All torrents are downloaded from this feed
        BTSession->downloadUrlAndSkipDialog(torrent_url);
      }
      // Item was downloaded, consider it as Read
      item->setRead();
      // Clean up
      delete matching_filter;
    }
  }
  return 0;
}

void RssStream::resizeList() {
  unsigned int max_articles = Preferences::getRSSMaxArticlesPerFeed();
  unsigned int nb_articles = this->size();
  if(nb_articles > max_articles) {
    QList<RssItem*> listItem = RssManager::sortNewsList(this->values());
    int excess = nb_articles - max_articles;
    for(int i=0; i<excess; ++i){
      RssItem *lastItem = listItem.takeLast();
      delete this->take(lastItem->getTitle());
    }
  }
}

// existing and opening test after download
short RssStream::openRss(){
  qDebug("openRss() called");
  QFile fileRss(filePath);
  if(!fileRss.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qDebug("openRss error: open failed, no file or locked, %s", (const char*)filePath.toLocal8Bit());
    if(QFile::exists(filePath)) {
      fileRss.remove();
    }
    return -1;
  }

  // start reading the xml
  short return_lecture = readDoc(&fileRss);
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

