/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
 * Copyright (C) 2014  sledgehammer999
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
 * Contact : hammered999@gmail.com
 */

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QTime>
#include <QDateTime>
#include <QList>
#include <QTimer>
#include <QReadWriteLock>
#include <QNetworkCookie>
#include <QVariant>

#include "types.h"

enum scheduler_days
{
    EVERY_DAY,
    WEEK_DAYS,
    WEEK_ENDS,
    MON,
    TUE,
    WED,
    THU,
    FRI,
    SAT,
    SUN
};

namespace Proxy
{
    enum ProxyType
    {
        HTTP = 1,
        SOCKS5 = 2,
        HTTP_PW = 3,
        SOCKS5_PW = 4,
        SOCKS4 = 5
    };
}

namespace TrayIcon
{
    enum Style
    {
        NORMAL = 0,
        MONO_DARK,
        MONO_LIGHT
    };
}

namespace DNS
{
    enum Service
    {
        DYNDNS,
        NOIP,
        NONE = -1
    };
}

class SettingsStorage;

class Preferences: public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Preferences)

    Preferences();

    const QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void setValue(const QString &key, const QVariant &value);

    static Preferences* m_instance;
    int m_randomPort;

signals:
    void changed();

public:
    static void initInstance();
    static void freeInstance();
    static Preferences* instance();

    // General options
    QString getLocale() const;
    void setLocale(const QString &locale);
    bool useProgramNotification() const;
    void useProgramNotification(bool use);
    bool deleteTorrentFilesAsDefault() const;
    void setDeleteTorrentFilesAsDefault(bool del);
    bool confirmOnExit() const;
    void setConfirmOnExit(bool confirm);
    bool speedInTitleBar() const;
    void showSpeedInTitleBar(bool show);
    bool useAlternatingRowColors() const;
    void setAlternatingRowColors(bool b);
    bool getHideZeroValues() const;
    void setHideZeroValues(bool b);
    int getHideZeroComboValues() const;
    void setHideZeroComboValues(int n);
    bool useRandomPort() const;
    void setRandomPort(bool b);
    bool systrayIntegration() const;
    void setSystrayIntegration(bool enabled);
    bool isToolbarDisplayed() const;
    void setToolbarDisplayed(bool displayed);
    bool minimizeToTray() const;
    void setMinimizeToTray(bool b);
    bool closeToTray() const;
    void setCloseToTray(bool b);
    bool startMinimized() const;
    void setStartMinimized(bool b);
    bool isSplashScreenDisabled() const;
    void setSplashScreenDisabled(bool b);
    bool preventFromSuspend() const;
    void setPreventFromSuspend(bool b);
#ifdef Q_OS_WIN
    bool WinStartup() const;
    void setWinStartup(bool b);
#endif

    // Downloads
    bool useIncompleteFilesExtension() const;
    void useIncompleteFilesExtension(bool enabled);
    QString lastLocationPath() const;
    void setLastLocationPath(const QString &path);
    bool preAllocateAllFiles() const;
    void preAllocateAllFiles(bool enabled);
    QVariantHash getScanDirs() const;
    void setScanDirs(const QVariantHash &dirs);
    QString getScanDirsLastPath() const;
    void setScanDirsLastPath(const QString &path);
    bool isTorrentExportEnabled() const;
    QString getTorrentExportDir() const;
    void setTorrentExportDir(QString path);
    bool isFinishedTorrentExportEnabled() const;
    QString getFinishedTorrentExportDir() const;
    void setFinishedTorrentExportDir(QString path);
    bool isMailNotificationEnabled() const;
    void setMailNotificationEnabled(bool enabled);
    QString getMailNotificationEmail() const;
    void setMailNotificationEmail(const QString &mail);
    QString getMailNotificationSMTP() const;
    void setMailNotificationSMTP(const QString &smtp_server);
    bool getMailNotificationSMTPSSL() const;
    void setMailNotificationSMTPSSL(bool use);
    bool getMailNotificationSMTPAuth() const;
    void setMailNotificationSMTPAuth(bool use);
    QString getMailNotificationSMTPUsername() const;
    void setMailNotificationSMTPUsername(const QString &username);
    QString getMailNotificationSMTPPassword() const;
    void setMailNotificationSMTPPassword(const QString &password);
    int getActionOnDblClOnTorrentDl() const;
    void setActionOnDblClOnTorrentDl(int act);
    int getActionOnDblClOnTorrentFn() const;
    void setActionOnDblClOnTorrentFn(int act);

    // Connection options
    int getSessionPort() const;
    void setSessionPort(int port);
    bool isUPnPEnabled() const;
    void setUPnPEnabled(bool enabled);
    int getGlobalDownloadLimit() const;
    void setGlobalDownloadLimit(int limit);
    int getGlobalUploadLimit() const;
    void setGlobalUploadLimit(int limit);
    int getAltGlobalDownloadLimit() const;
    void setAltGlobalDownloadLimit(int limit);
    int getAltGlobalUploadLimit() const;
    void setAltGlobalUploadLimit(int limit);
    bool isAltBandwidthEnabled() const;
    void setAltBandwidthEnabled(bool enabled);
    bool isSchedulerEnabled() const;
    void setSchedulerEnabled(bool enabled);
    QTime getSchedulerStartTime() const;
    void setSchedulerStartTime(const QTime &time);
    QTime getSchedulerEndTime() const;
    void setSchedulerEndTime(const QTime &time);
    scheduler_days getSchedulerDays() const;
    void setSchedulerDays(scheduler_days days);

    // Proxy options
    bool isProxyEnabled() const;
    bool isProxyAuthEnabled() const;
    void setProxyAuthEnabled(bool enabled);
    QString getProxyIp() const;
    void setProxyIp(const QString &ip);
    unsigned short getProxyPort() const;
    void setProxyPort(unsigned short port);
    QString getProxyUsername() const;
    void setProxyUsername(const QString &username);
    QString getProxyPassword() const;
    void setProxyPassword(const QString &password);
    int getProxyType() const;
    void setProxyType(int type);
    bool proxyPeerConnections() const;
    void setProxyPeerConnections(bool enabled);
    bool getForceProxy() const;
    void setForceProxy(bool enabled);
    void setProxyOnlyForTorrents(bool enabled);
    bool isProxyOnlyForTorrents() const;

    // Bittorrent options
    int getMaxConnecs() const;
    void setMaxConnecs(int val);
    int getMaxConnecsPerTorrent() const;
    void setMaxConnecsPerTorrent(int val);
    int getMaxUploads() const;
    void setMaxUploads(int val);
    int getMaxUploadsPerTorrent() const;
    void setMaxUploadsPerTorrent(int val);
    bool isuTPEnabled() const;
    void setuTPEnabled(bool enabled);
    bool isuTPRateLimited() const;
    void setuTPRateLimited(bool enabled);
    bool isDHTEnabled() const;
    void setDHTEnabled(bool enabled);
    bool isPeXEnabled() const;
    void setPeXEnabled(bool enabled);
    bool isLSDEnabled() const;
    void setLSDEnabled(bool enabled);
    int getEncryptionSetting() const;
    void setEncryptionSetting(int val);
    bool isAddTrackersEnabled() const;
    void setAddTrackersEnabled(bool enabled);
    QString getTrackersList() const;
    void setTrackersList(const QString &val);
    qreal getGlobalMaxRatio() const;
    void setGlobalMaxRatio(qreal ratio);

    // IP Filter
    bool isFilteringEnabled() const;
    void setFilteringEnabled(bool enabled);
    bool isFilteringTrackerEnabled() const;
    void setFilteringTrackerEnabled(bool enabled);
    QString getFilter() const;
    void setFilter(const QString &path);
    QStringList bannedIPs() const;
    void banIP(const QString &ip);

    // Search
    bool isSearchEnabled() const;
    void setSearchEnabled(bool enabled);

    // Execution Log
    bool isExecutionLogEnabled() const;
    void setExecutionLogEnabled(bool b);

    // Queueing system
    bool isQueueingSystemEnabled() const;
    void setQueueingSystemEnabled(bool enabled);
    int getMaxActiveDownloads() const;
    void setMaxActiveDownloads(int val);
    int getMaxActiveUploads() const;
    void setMaxActiveUploads(int val);
    int getMaxActiveTorrents() const;
    void setMaxActiveTorrents(int val);
    bool ignoreSlowTorrentsForQueueing() const;
    void setIgnoreSlowTorrentsForQueueing(bool ignore);
    bool isWebUiEnabled() const;
    void setWebUiEnabled(bool enabled);
    bool isWebUiLocalAuthEnabled() const;
    void setWebUiLocalAuthEnabled(bool enabled);
    quint16 getWebUiPort() const;
    void setWebUiPort(quint16 port);
    bool useUPnPForWebUIPort() const;
    void setUPnPForWebUIPort(bool enabled);
    QStringList getWebUiAuthenticationTokens() const;
    void setWebUiAuthenticationTokens(const QStringList &tokens);
    bool isAuthenticationTokenValid(const QString& token) const;
    QString getWebUiUsername() const;
    void setWebUiUsername(const QString &username);
    QString getWebUiPassword() const;
    void setWebUiPassword(const QString &new_password);
    bool isWebUiHttpsEnabled() const;
    void setWebUiHttpsEnabled(bool enabled);
    QByteArray getWebUiHttpsCertificate() const;
    void setWebUiHttpsCertificate(const QByteArray &data);
    QByteArray getWebUiHttpsKey() const;
    void setWebUiHttpsKey(const QByteArray &data);
    bool isDynDNSEnabled() const;
    void setDynDNSEnabled(bool enabled);
    DNS::Service getDynDNSService() const;
    void setDynDNSService(int service);
    QString getDynDomainName() const;
    void setDynDomainName(const QString &name);
    QString getDynDNSUsername() const;
    void setDynDNSUsername(const QString &username);
    QString getDynDNSPassword() const;
    void setDynDNSPassword(const QString &password);

    // Advanced settings
    void setUILockPassword(const QString &clear_password);
    void clearUILockPassword();
    QString getUILockPasswordMD5() const;
    bool isUILocked() const;
    void setUILocked(bool locked);
    bool isAutoRunEnabled() const;
    void setAutoRunEnabled(bool enabled);
    QString getAutoRunProgram() const;
    void setAutoRunProgram(const QString &program);
    bool shutdownWhenDownloadsComplete() const;
    void setShutdownWhenDownloadsComplete(bool shutdown);
    bool suspendWhenDownloadsComplete() const;
    void setSuspendWhenDownloadsComplete(bool suspend);
    bool hibernateWhenDownloadsComplete() const;
    void setHibernateWhenDownloadsComplete(bool hibernate);
    bool shutdownqBTWhenDownloadsComplete() const;
    void setShutdownqBTWhenDownloadsComplete(bool shutdown);
    uint diskCacheSize() const;
    void setDiskCacheSize(uint size);
    uint diskCacheTTL() const;
    void setDiskCacheTTL(uint ttl);
    bool osCache() const;
    void setOsCache(bool enable);
    uint saveResumeDataInterval() const;
    void setSaveResumeDataInterval(uint m);
    uint outgoingPortsMin() const;
    void setOutgoingPortsMin(uint val);
    uint outgoingPortsMax() const;
    void setOutgoingPortsMax(uint val);
    bool getIgnoreLimitsOnLAN() const;
    void setIgnoreLimitsOnLAN(bool ignore);
    bool includeOverheadInLimits() const;
    void includeOverheadInLimits(bool include);
    bool trackerExchangeEnabled() const;
    void setTrackerExchangeEnabled(bool enable);
    bool recheckTorrentsOnCompletion() const;
    void recheckTorrentsOnCompletion(bool recheck);
    unsigned int getRefreshInterval() const;
    void setRefreshInterval(uint interval);
    bool resolvePeerCountries() const;
    void resolvePeerCountries(bool resolve);
    bool resolvePeerHostNames() const;
    void resolvePeerHostNames(bool resolve);
    int getMaxHalfOpenConnections() const;
    void setMaxHalfOpenConnections(int value);
    QString getNetworkInterface() const;
    void setNetworkInterface(const QString& iface);
    QString getNetworkInterfaceName() const;
    void setNetworkInterfaceName(const QString& iface);
    bool getListenIPv6() const;
    void setListenIPv6(bool enable);
    QString getNetworkAddress() const;
    void setNetworkAddress(const QString& addr);
    bool isAnonymousModeEnabled() const;
    void enableAnonymousMode(bool enabled);
    bool isSuperSeedingEnabled() const;
    void enableSuperSeeding(bool enabled);
    bool announceToAllTrackers() const;
    void setAnnounceToAllTrackers(bool enabled);
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    bool useSystemIconTheme() const;
    void useSystemIconTheme(bool enabled);
#endif
    bool recursiveDownloadDisabled() const;
    void disableRecursiveDownload(bool disable = true);
#ifdef Q_OS_WIN
    static QString getPythonPath();
    bool neverCheckFileAssoc() const;
    void setNeverCheckFileAssoc(bool check = true);
    static bool isTorrentFileAssocSet();
    static bool isMagnetLinkAssocSet();
    static void setTorrentFileAssoc(bool set);
    static void setMagnetLinkAssoc(bool set);
#endif
#ifdef Q_OS_MAC
    static bool isTorrentFileAssocSet();
    static bool isMagnetLinkAssocSet();
    static void setTorrentFileAssoc();
    static void setMagnetLinkAssoc();
#endif
    bool isTrackerEnabled() const;
    void setTrackerEnabled(bool enabled);
    int getTrackerPort() const;
    void setTrackerPort(int port);
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    bool isUpdateCheckEnabled() const;
    void setUpdateCheckEnabled(bool enabled);
#endif
    bool confirmTorrentDeletion() const;
    void setConfirmTorrentDeletion(bool enabled);
    bool confirmTorrentRecheck() const;
    void setConfirmTorrentRecheck(bool enabled);
    TrayIcon::Style trayIconStyle() const;
    void setTrayIconStyle(TrayIcon::Style style);

    // Stuff that don't appear in the Options GUI but are saved
    // in the same file.
    QDateTime getDNSLastUpd() const;
    void setDNSLastUpd(const QDateTime &date);
    QString getDNSLastIP() const;
    void setDNSLastIP(const QString &ip);
    bool getAcceptedLegal() const;
    void setAcceptedLegal(const bool accepted);
    QByteArray getMainGeometry() const;
    void setMainGeometry(const QByteArray &geometry);
    QByteArray getMainVSplitterState() const;
    void setMainVSplitterState(const QByteArray &state);
    QString getMainLastDir() const;
    void setMainLastDir(const QString &path);
#ifndef DISABLE_GUI
    QSize getPrefSize(const QSize &defaultSize) const;
    void setPrefSize(const QSize &size);
#endif
    QPoint getPrefPos() const;
    void setPrefPos(const QPoint &pos);
    QStringList getPrefHSplitterSizes() const;
    void setPrefHSplitterSizes(const QStringList &sizes);
    QByteArray getPeerListState() const;
    void setPeerListState(const QByteArray &state);
    QString getPropSplitterSizes() const;
    void setPropSplitterSizes(const QString &sizes);
    QByteArray getPropFileListState() const;
    void setPropFileListState(const QByteArray &state);
    int getPropCurTab() const;
    void setPropCurTab(const int &tab);
    bool getPropVisible() const;
    void setPropVisible(const bool visible);
    QByteArray getPropTrackerListState() const;
    void setPropTrackerListState(const QByteArray &state);
    QByteArray getRssGeometry() const;
    void setRssGeometry(const QByteArray &geometry);
    QByteArray getRssHSplitterSizes() const;
    void setRssHSplitterSizes(const QByteArray &sizes);
    QStringList getRssOpenFolders() const;
    void setRssOpenFolders(const QStringList &folders);
    QByteArray getRssHSplitterState() const;
    void setRssHSplitterState(const QByteArray &state);
    QByteArray getRssVSplitterState() const;
    void setRssVSplitterState(const QByteArray &state);
    QString getSearchColsWidth() const;
    void setSearchColsWidth(const QString &width);
    QStringList getSearchEngDisabled() const;
    void setSearchEngDisabled(const QStringList &engines);
    QString getCreateTorLastAddPath() const;
    void setCreateTorLastAddPath(const QString &path);
    QString getCreateTorLastSavePath() const;
    void setCreateTorLastSavePath(const QString &path);
    QString getCreateTorTrackers() const;
    void setCreateTorTrackers(const QString &path);
    QByteArray getCreateTorGeometry() const;
    void setCreateTorGeometry(const QByteArray &geometry);
    bool getCreateTorIgnoreRatio() const;
    void setCreateTorIgnoreRatio(const bool ignore);
    QString getTorImportLastContentDir() const;
    void setTorImportLastContentDir(const QString &path);
    QByteArray getTorImportGeometry() const;
    void setTorImportGeometry(const QByteArray &geometry);
    bool getStatusFilterState() const;
    bool getCategoryFilterState() const;
    bool getTrackerFilterState() const;
    int getTransSelFilter() const;
    void setTransSelFilter(const int &index);
    QByteArray getTransHeaderState() const;
    void setTransHeaderState(const QByteArray &state);
    int getToolbarTextPosition() const;
    void setToolbarTextPosition(const int position);

    //From old RssSettings class
    bool isRSSEnabled() const;
    void setRSSEnabled(const bool enabled);
    uint getRSSRefreshInterval() const;
    void setRSSRefreshInterval(const uint &interval);
    int getRSSMaxArticlesPerFeed() const;
    void setRSSMaxArticlesPerFeed(const int &nb);
    bool isRssDownloadingEnabled() const;
    void setRssDownloadingEnabled(const bool b);
    QStringList getRssFeedsUrls() const;
    void setRssFeedsUrls(const QStringList &rssFeeds);
    QStringList getRssFeedsAliases() const;
    void setRssFeedsAliases(const QStringList &rssAliases);

    // Network
    QList<QNetworkCookie> getNetworkCookies() const;
    void setNetworkCookies(const QList<QNetworkCookie> &cookies);

    // SpeedWidget
    int getSpeedWidgetPeriod() const;
    void setSpeedWidgetPeriod(const int period);
    bool getSpeedWidgetGraphEnable(int id) const;
    void setSpeedWidgetGraphEnable(int id, const bool enable);

    void upgrade();

public slots:
    void setStatusFilterState(bool checked);
    void setCategoryFilterState(bool checked);
    void setTrackerFilterState(bool checked);

    void apply();
};

#endif // PREFERENCES_H
