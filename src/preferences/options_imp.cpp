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
#include <QTextStream>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QSystemTrayIcon>
#include <QApplication>
#include <QDialogButtonBox>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QTranslator>
#include <QDesktopServices>

#include <libtorrent/version.hpp>
#include <time.h>
#include <stdlib.h>

#include "options_imp.h"
#include "preferences.h"
#include "fs_utils.h"
#include "advancedsettings.h"
#include "scannedfoldersmodel.h"
#include "qinisettings.h"
#include "qbtsession.h"
#include "iconprovider.h"
#include "dnsupdater.h"

#ifndef QT_NO_OPENSSL
#include <QSslKey>
#include <QSslCertificate>
#endif

using namespace libtorrent;

// Constructor
options_imp::options_imp(QWidget *parent):
  QDialog(parent), m_refreshingIpFilter(false) {
  qDebug("-> Constructing Options");
  setupUi(this);
  setAttribute(Qt::WA_DeleteOnClose);
  setModal(true);
  // Icons
  tabSelection->item(TAB_UI)->setIcon(IconProvider::instance()->getIcon("preferences-desktop"));
  tabSelection->item(TAB_BITTORRENT)->setIcon(IconProvider::instance()->getIcon("preferences-system-network"));
  tabSelection->item(TAB_CONNECTION)->setIcon(IconProvider::instance()->getIcon("network-wired"));
  tabSelection->item(TAB_DOWNLOADS)->setIcon(IconProvider::instance()->getIcon("download"));
  tabSelection->item(TAB_SPEED)->setIcon(IconProvider::instance()->getIcon("chronometer"));
  tabSelection->item(TAB_WEBUI)->setIcon(IconProvider::instance()->getIcon("network-server"));
  tabSelection->item(TAB_ADVANCED)->setIcon(IconProvider::instance()->getIcon("preferences-other"));
  IpFilterRefreshBtn->setIcon(IconProvider::instance()->getIcon("view-refresh"));

  hsplitter->setCollapsible(0, false);
  hsplitter->setCollapsible(1, false);
  // Get apply button in button box
  QList<QAbstractButton *> buttons = buttonBox->buttons();
  foreach (QAbstractButton *button, buttons) {
    if (buttonBox->buttonRole(button) == QDialogButtonBox::ApplyRole) {
      applyButton = button;
      break;
    }
  }

  scanFoldersView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
  scanFoldersView->setModel(ScanFoldersModel::instance());
  connect(ScanFoldersModel::instance(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(enableApplyButton()));
  connect(scanFoldersView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(handleScanFolderViewSelectionChanged()));

  connect(buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(applySettings(QAbstractButton*)));
  // Languages supported
  initializeLanguageCombo();

  // Load week days (scheduler)
  for (uint i=1; i<=7; ++i) {
    schedule_days->addItem(QDate::longDayName(i, QDate::StandaloneFormat));
  }

  // Load options
  loadOptions();
  // Disable systray integration if it is not supported by the system
#ifndef Q_WS_MAC
  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
#endif
    checkShowSystray->setChecked(false);
    checkShowSystray->setEnabled(false);
#ifndef Q_WS_MAC
  }
#endif
#if !defined(Q_WS_X11)
  label_trayIconStyle->setVisible(false);
  comboTrayIcon->setVisible(false);
#endif
#if defined(QT_NO_OPENSSL)
  checkWebUiHttps->setVisible(false);
#endif

#ifndef Q_WS_WIN
  checkStartup->setVisible(false);
  groupFileAssociation->setVisible(false);
#endif
#if LIBTORRENT_VERSION_MINOR < 16
  checkAnonymousMode->setVisible(false);
  label_anonymous->setVisible(false);
#endif

  // Connect signals / slots
  connect(comboProxyType, SIGNAL(currentIndexChanged(int)),this, SLOT(enableProxy(int)));
  connect(checkAnonymousMode, SIGNAL(toggled(bool)), this, SLOT(toggleAnonymousMode(bool)));

  // Apply button is activated when a value is changed
  // General tab
  connect(comboI18n, SIGNAL(currentIndexChanged(int)), this, SLOT(enableApplyButton()));
  connect(checkAltRowColors, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkShowSystray, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkCloseToSystray, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkMinimizeToSysTray, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkStartMinimized, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
#ifdef Q_WS_WIN
  connect(checkStartup, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
#endif
  connect(checkShowSplash, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkProgramExitConfirm, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkPreventFromSuspend, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(comboTrayIcon, SIGNAL(currentIndexChanged(int)), this, SLOT(enableApplyButton()));
#if defined(Q_WS_X11) && !defined(QT_DBUS_LIB)
  checkPreventFromSuspend->setDisabled(true);
#endif
#ifdef Q_WS_WIN
  connect(checkAssociateTorrents, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkAssociateMagnetLinks, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
#endif
  // Downloads tab
  connect(textSavePath, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(textTempPath, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(checkAppendLabel, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkAppendqB, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkPreallocateAll, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkAdditionDialog, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkStartPaused, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkExportDir, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkExportDirFin, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(textExportDir, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(textExportDirFin, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(actionTorrentDlOnDblClBox, SIGNAL(currentIndexChanged(int)), this, SLOT(enableApplyButton()));
  connect(actionTorrentFnOnDblClBox, SIGNAL(currentIndexChanged(int)), this, SLOT(enableApplyButton()));
  connect(checkTempFolder, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(addScanFolderButton, SIGNAL(clicked()), this, SLOT(enableApplyButton()));
  connect(removeScanFolderButton, SIGNAL(clicked()), this, SLOT(enableApplyButton()));
  connect(groupMailNotification, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(dest_email_txt, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(smtp_server_txt, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(checkSmtpSSL, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(groupMailNotifAuth, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(mailNotifUsername, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(mailNotifPassword, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(autoRunBox, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(autoRun_txt, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  // Connection tab
  connect(spinPort, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(checkUPnP, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkUploadLimit, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkDownloadLimit, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(spinUploadLimit, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spinDownloadLimit, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spinUploadLimitAlt, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spinDownloadLimitAlt, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(check_schedule, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(schedule_from, SIGNAL(timeChanged(QTime)), this, SLOT(enableApplyButton()));
  connect(schedule_to, SIGNAL(timeChanged(QTime)), this, SLOT(enableApplyButton()));
  connect(schedule_days, SIGNAL(currentIndexChanged(int)), this, SLOT(enableApplyButton()));
  connect(checkuTP, SIGNAL(toggled(bool)), SLOT(enableApplyButton()));
  connect(checkLimituTPConnections, SIGNAL(toggled(bool)), SLOT(enableApplyButton()));
  connect(checkLimitTransportOverhead, SIGNAL(toggled(bool)), SLOT(enableApplyButton()));
  // Bittorrent tab
  connect(checkMaxConnecs, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkMaxConnecsPerTorrent, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkMaxUploadsPerTorrent, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(spinMaxConnec, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spinMaxConnecPerTorrent, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spinMaxUploadsPerTorrent, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(checkDHT, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
#if LIBTORRENT_VERSION_MINOR > 15
  connect(checkAnonymousMode, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
#endif
  connect(checkPeX, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkDifferentDHTPort, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(spinDHTPort, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(checkLSD, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(comboEncryption, SIGNAL(currentIndexChanged(int)), this, SLOT(enableApplyButton()));
  connect(checkMaxRatio, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(spinMaxRatio, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(comboRatioLimitAct, SIGNAL(currentIndexChanged(int)), this, SLOT(enableApplyButton()));
  // Proxy tab
  connect(comboProxyType, SIGNAL(currentIndexChanged(int)), this, SLOT(enableApplyButton()));
  connect(textProxyIP, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spinProxyPort, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(checkProxyPeerConnecs, SIGNAL(toggled(bool)), SLOT(enableApplyButton()));
  connect(checkProxyAuth, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(textProxyUsername, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(textProxyPassword, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  // Misc tab
  connect(checkIPFilter, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(textFilterPath, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(checkEnableQueueing, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(spinMaxActiveDownloads, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spinMaxActiveUploads, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spinMaxActiveTorrents, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(checkIgnoreSlowTorrentsForQueueing, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  // Web UI tab
  connect(checkWebUi, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(spinWebUiPort, SIGNAL(valueChanged(int)), this, SLOT(enableApplyButton()));
  connect(checkWebUIUPnP, SIGNAL(toggled(bool)), SLOT(enableApplyButton()));
  connect(checkWebUiHttps, SIGNAL(toggled(bool)), SLOT(enableApplyButton()));
  connect(btnWebUiKey, SIGNAL(clicked()), SLOT(enableApplyButton()));
  connect(btnWebUiCrt, SIGNAL(clicked()), SLOT(enableApplyButton()));
  connect(textWebUiUsername, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(textWebUiPassword, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(checkBypassLocalAuth, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkDynDNS, SIGNAL(toggled(bool)), SLOT(enableApplyButton()));
  connect(comboDNSService, SIGNAL(currentIndexChanged(int)), SLOT(enableApplyButton()));
  connect(domainNameTxt, SIGNAL(textChanged(QString)), SLOT(enableApplyButton()));
  connect(DNSUsernameTxt, SIGNAL(textChanged(QString)), SLOT(enableApplyButton()));
  connect(DNSPasswordTxt, SIGNAL(textChanged(QString)), SLOT(enableApplyButton()));
  // Disable apply Button
  applyButton->setEnabled(false);
  // Tab selection mecanism
  connect(tabSelection, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), this, SLOT(changePage(QListWidgetItem *, QListWidgetItem*)));
#if LIBTORRENT_VERSION_MINOR < 16
  checkuTP->setVisible(false);
  checkLimituTPConnections->setVisible(false);
#endif
  // Load Advanced settings
  QVBoxLayout *adv_layout = new QVBoxLayout();
  advancedSettings = new AdvancedSettings();
  adv_layout->addWidget(advancedSettings);
  scrollArea_advanced->setLayout(adv_layout);
  connect(advancedSettings, SIGNAL(settingsChanged()), this, SLOT(enableApplyButton()));

  // Adapt size
  show();
  loadWindowState();
}

void options_imp::initializeLanguageCombo()
{
  // List language files
  const QDir lang_dir(":/lang");
  const QStringList lang_files = lang_dir.entryList(QStringList() << "qbittorrent_*.qm", QDir::Files);
  foreach (QString lang_file, lang_files) {
    QString localeStr = lang_file.mid(12); // remove "qbittorrent_"
    localeStr.chop(3); // Remove ".qm"
    QLocale locale(localeStr);
    const QString country = locale.name().split("_").last().toLower();
    QString language_name = languageToLocalizedString(locale.language(), country);
    comboI18n->addItem(/*QIcon(":/Icons/flags/"+country+".png"), */language_name, locale.name());
    qDebug() << "Supported locale:" << locale.name();
  }
}

// Main destructor
options_imp::~options_imp() {
  qDebug("-> destructing Options");
  foreach (const QString &path, addedScanDirs)
    ScanFoldersModel::instance()->removePath(path);
  delete scrollArea_advanced->layout();
  delete advancedSettings;
}

void options_imp::changePage(QListWidgetItem *current, QListWidgetItem *previous) {
  if (!current)
    current = previous;
  tabOption->setCurrentIndex(tabSelection->row(current));
}

void options_imp::loadWindowState() {
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  resize(settings.value(QString::fromUtf8("Preferences/State/size"), sizeFittingScreen()).toSize());
  QPoint p = settings.value(QString::fromUtf8("Preferences/State/pos"), QPoint()).toPoint();
  if (!p.isNull())
    move(p);
  // Load slider size
  const QStringList sizes_str = settings.value("Preferences/State/hSplitterSizes", QStringList()).toStringList();
  // Splitter size
  QList<int> sizes;
  if (sizes_str.size() == 2) {
    sizes << sizes_str.first().toInt();
    sizes << sizes_str.last().toInt();
  } else {
    sizes << 130;
    sizes << hsplitter->width()-130;
  }
  hsplitter->setSizes(sizes);
}

void options_imp::saveWindowState() const {
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  settings.setValue(QString::fromUtf8("Preferences/State/size"), size());
  settings.setValue(QString::fromUtf8("Preferences/State/pos"), pos());
  // Splitter size
  QStringList sizes_str;
  sizes_str << QString::number(hsplitter->sizes().first());
  sizes_str << QString::number(hsplitter->sizes().last());
  settings.setValue(QString::fromUtf8("Preferences/State/hSplitterSizes"), sizes_str);
}

QSize options_imp::sizeFittingScreen() const {
  int scrn = 0;
  QWidget *w = this->topLevelWidget();

  if (w)
    scrn = QApplication::desktop()->screenNumber(w);
  else if (QApplication::desktop()->isVirtualDesktop())
    scrn = QApplication::desktop()->screenNumber(QCursor::pos());
  else
    scrn = QApplication::desktop()->screenNumber(this);

  QRect desk(QApplication::desktop()->availableGeometry(scrn));
  if (width() > desk.width() || height() > desk.height()) {
    if (desk.width() > 0 && desk.height() > 0)
      return QSize(desk.width(), desk.height());
  }
  return size();
}

void options_imp::saveOptions() {
  applyButton->setEnabled(false);
  Preferences pref;
  // Load the translation
  QString locale = getLocale();
  if (pref.getLocale() != locale) {
    QTranslator *translator = new QTranslator;
    if (translator->load(QString::fromUtf8(":/lang/qbittorrent_") + locale)) {
      qDebug("%s locale recognized, using translation.", qPrintable(locale));
    }else{
      qDebug("%s locale unrecognized, using default (en_GB).", qPrintable(locale));
    }
    qApp->installTranslator(translator);
  }

  // General preferences
  pref.setLocale(locale);
  pref.setAlternatingRowColors(checkAltRowColors->isChecked());
  pref.setSystrayIntegration(systrayIntegration());
  pref.setTrayIconStyle(TrayIcon::Style(comboTrayIcon->currentIndex()));
  pref.setCloseToTray(closeToTray());
  pref.setMinimizeToTray(minimizeToTray());
  pref.setStartMinimized(startMinimized());
  pref.setSplashScreenDisabled(isSlashScreenDisabled());
  pref.setConfirmOnExit(checkProgramExitConfirm->isChecked());
  pref.setPreventFromSuspend(preventFromSuspend());
#ifdef Q_WS_WIN
  pref.setStartup(Startup());
  // Windows: file association settings
  Preferences::setTorrentFileAssoc(checkAssociateTorrents->isChecked());
  Preferences::setMagnetLinkAssoc(checkAssociateMagnetLinks->isChecked());
#endif
  // End General preferences

  // Downloads preferences
  QString save_path = getSavePath();
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
  save_path.replace("\\", "/");
#endif
  pref.setSavePath(save_path);
  pref.setTempPathEnabled(isTempPathEnabled());
  QString temp_path = getTempPath();
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
  temp_path.replace("\\", "/");
#endif
  pref.setTempPath(temp_path);
  pref.setAppendTorrentLabel(checkAppendLabel->isChecked());
  pref.useIncompleteFilesExtension(checkAppendqB->isChecked());
  pref.preAllocateAllFiles(preAllocateAllFiles());
  pref.useAdditionDialog(useAdditionDialog());
  pref.addTorrentsInPause(addTorrentsInPause());
  ScanFoldersModel::instance()->makePersistent();
  addedScanDirs.clear();
  QString export_dir = getTorrentExportDir();
  QString export_dir_fin = getFinishedTorrentExportDir();
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
  export_dir_fin.replace("\\", "/");
  export_dir.replace("\\", "/");
#endif
  pref.setTorrentExportDir(export_dir);
  pref.setFinishedTorrentExportDir(export_dir_fin);
  pref.setMailNotificationEnabled(groupMailNotification->isChecked());
  pref.setMailNotificationEmail(dest_email_txt->text());
  pref.setMailNotificationSMTP(smtp_server_txt->text());
  pref.setMailNotificationSMTPSSL(checkSmtpSSL->isChecked());
  pref.setMailNotificationSMTPAuth(groupMailNotifAuth->isChecked());
  pref.setMailNotificationSMTPUsername(mailNotifUsername->text());
  pref.setMailNotificationSMTPPassword(mailNotifPassword->text());
  pref.setAutoRunEnabled(autoRunBox->isChecked());
  pref.setAutoRunProgram(autoRun_txt->text());
  pref.setActionOnDblClOnTorrentDl(getActionOnDblClOnTorrentDl());
  pref.setActionOnDblClOnTorrentFn(getActionOnDblClOnTorrentFn());
  // End Downloads preferences
  // Connection preferences
  pref.setSessionPort(getPort());
  pref.setUPnPEnabled(isUPnPEnabled());
  const QPair<int, int> down_up_limit = getGlobalBandwidthLimits();
  pref.setGlobalDownloadLimit(down_up_limit.first);
  pref.setGlobalUploadLimit(down_up_limit.second);
  pref.setuTPEnabled(checkuTP->isChecked());
  pref.setuTPRateLimited(checkLimituTPConnections->isChecked());
  pref.includeOverheadInLimits(checkLimitTransportOverhead->isChecked());
  pref.setAltGlobalDownloadLimit(spinDownloadLimitAlt->value());
  pref.setAltGlobalUploadLimit(spinUploadLimitAlt->value());
  pref.setSchedulerEnabled(check_schedule->isChecked());
  pref.setSchedulerStartTime(schedule_from->time());
  pref.setSchedulerEndTime(schedule_to->time());
  pref.setSchedulerDays((scheduler_days)schedule_days->currentIndex());
  pref.setProxyType(getProxyType());
  pref.setProxyIp(getProxyIp());
  pref.setProxyPort(getProxyPort());
  pref.setProxyPeerConnections(checkProxyPeerConnecs->isChecked());
  pref.setProxyAuthEnabled(isProxyAuthEnabled());
  pref.setProxyUsername(getProxyUsername());
  pref.setProxyPassword(getProxyPassword());
  // End Connection preferences
  // Bittorrent preferences
  pref.setMaxConnecs(getMaxConnecs());
  pref.setMaxConnecsPerTorrent(getMaxConnecsPerTorrent());
  pref.setMaxUploadsPerTorrent(getMaxUploadsPerTorrent());
  pref.setDHTEnabled(isDHTEnabled());
  pref.setPeXEnabled(checkPeX->isChecked());
  pref.setDHTPortSameAsBT(isDHTPortSameAsBT());
  pref.setDHTPort(getDHTPort());
  pref.setLSDEnabled(isLSDEnabled());
  pref.setEncryptionSetting(getEncryptionSetting());
#if LIBTORRENT_VERSION_MINOR > 15
  pref.enableAnonymousMode(checkAnonymousMode->isChecked());
#endif
  pref.setGlobalMaxRatio(getMaxRatio());
  pref.setMaxRatioAction(comboRatioLimitAct->currentIndex());
  // End Bittorrent preferences
  // Misc preferences
  // * IPFilter
  pref.setFilteringEnabled(isFilteringEnabled());
  if (isFilteringEnabled()) {
    QString filter_path = textFilterPath->text();
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
    filter_path.replace("\\", "/");
#endif
    pref.setFilter(filter_path);
  }
  // End IPFilter preferences
  // Queueing system
  pref.setQueueingSystemEnabled(isQueueingSystemEnabled());
  pref.setMaxActiveDownloads(spinMaxActiveDownloads->value());
  pref.setMaxActiveUploads(spinMaxActiveUploads->value());
  pref.setMaxActiveTorrents(spinMaxActiveTorrents->value());
  pref.setIgnoreSlowTorrentsForQueueing(checkIgnoreSlowTorrentsForQueueing->isChecked());
  // End Queueing system preferences
  // Web UI
  pref.setWebUiEnabled(isWebUiEnabled());
  if (isWebUiEnabled())
  {
    pref.setWebUiPort(webUiPort());
    pref.setUPnPForWebUIPort(checkWebUIUPnP->isChecked());
    pref.setWebUiHttpsEnabled(checkWebUiHttps->isChecked());
    if (checkWebUiHttps->isChecked())
    {
      pref.setWebUiHttpsCertificate(m_sslCert);
      pref.setWebUiHttpsKey(m_sslKey);
    }
    pref.setWebUiUsername(webUiUsername());
    // FIXME: Check that the password is valid (not empty at least)
    pref.setWebUiPassword(webUiPassword());
    pref.setWebUiLocalAuthEnabled(!checkBypassLocalAuth->isChecked());
    // DynDNS
    pref.setDynDNSEnabled(checkDynDNS->isChecked());
    pref.setDynDNSService(comboDNSService->currentIndex());
    pref.setDynDomainName(domainNameTxt->text());
    pref.setDynDNSUsername(DNSUsernameTxt->text());
    pref.setDynDNSPassword(DNSPasswordTxt->text());
  }
  // End Web UI
  // End preferences
  // Save advanced settings
  advancedSettings->saveAdvancedSettings();
}

bool options_imp::isFilteringEnabled() const {
  return checkIPFilter->isChecked();
}

int options_imp::getProxyType() const {
  switch(comboProxyType->currentIndex()) {
  case 1:
    return Proxy::SOCKS4;
    break;
  case 2:
    if (isProxyAuthEnabled()) {
      return Proxy::SOCKS5_PW;
    }
    return Proxy::SOCKS5;
  case 3:
    if (isProxyAuthEnabled()) {
      return Proxy::HTTP_PW;
    }
    return Proxy::HTTP;
  default:
    return -1;
  }
}

void options_imp::loadOptions() {
  int intValue;
  qreal floatValue;
  QString strValue;
  // General preferences
  const Preferences pref;
  setLocale(pref.getLocale());
  checkAltRowColors->setChecked(pref.useAlternatingRowColors());
  checkShowSystray->setChecked(pref.systrayIntegration());
  checkShowSplash->setChecked(!pref.isSlashScreenDisabled());
  if (checkShowSystray->isChecked()) {
    checkCloseToSystray->setChecked(pref.closeToTray());
    checkMinimizeToSysTray->setChecked(pref.minimizeToTray());
    checkStartMinimized->setChecked(pref.startMinimized());
  }
  comboTrayIcon->setCurrentIndex(pref.trayIconStyle());
  checkProgramExitConfirm->setChecked(pref.confirmOnExit());
  checkPreventFromSuspend->setChecked(pref.preventFromSuspend());
#ifdef Q_WS_WIN
  checkStartup->setChecked(pref.Startup());
  // Windows: file association settings
  checkAssociateTorrents->setChecked(Preferences::isTorrentFileAssocSet());
  checkAssociateMagnetLinks->setChecked(Preferences::isMagnetLinkAssocSet());
#endif
  // End General preferences
  // Downloads preferences
  QString save_path = pref.getSavePath();
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
  save_path.replace("/", "\\");
#endif
  textSavePath->setText(save_path);
  if (pref.isTempPathEnabled()) {
    // enable
    checkTempFolder->setChecked(true);
  } else {
    checkTempFolder->setChecked(false);
  }
  QString temp_path = pref.getTempPath();
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
  temp_path.replace("/", "\\");
#endif
  textTempPath->setText(temp_path);
  checkAppendLabel->setChecked(pref.appendTorrentLabel());
  checkAppendqB->setChecked(pref.useIncompleteFilesExtension());
  checkPreallocateAll->setChecked(pref.preAllocateAllFiles());
  checkAdditionDialog->setChecked(pref.useAdditionDialog());
  checkStartPaused->setChecked(pref.addTorrentsInPause());

  strValue = pref.getTorrentExportDir();
  if (strValue.isEmpty()) {
    // Disable
    checkExportDir->setChecked(false);
  } else {
    // enable
    checkExportDir->setChecked(true);
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
    strValue.replace("/", "\\");
#endif
    textExportDir->setText(strValue);
  }

  strValue = pref.getFinishedTorrentExportDir();
  if (strValue.isEmpty()) {
    // Disable
    checkExportDirFin->setChecked(false);
  } else {
    // enable
    checkExportDirFin->setChecked(true);
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
    strValue.replace("/", "\\");
#endif
    textExportDirFin->setText(strValue);
  }
  groupMailNotification->setChecked(pref.isMailNotificationEnabled());
  dest_email_txt->setText(pref.getMailNotificationEmail());
  smtp_server_txt->setText(pref.getMailNotificationSMTP());
  checkSmtpSSL->setChecked(pref.getMailNotificationSMTPSSL());
  groupMailNotifAuth->setChecked(pref.getMailNotificationSMTPAuth());
  mailNotifUsername->setText(pref.getMailNotificationSMTPUsername());
  mailNotifPassword->setText(pref.getMailNotificationSMTPPassword());
  autoRunBox->setChecked(pref.isAutoRunEnabled());
  autoRun_txt->setText(pref.getAutoRunProgram());
  intValue = pref.getActionOnDblClOnTorrentDl();
  if (intValue >= actionTorrentDlOnDblClBox->count())
    intValue = 0;
  actionTorrentDlOnDblClBox->setCurrentIndex(intValue);
  intValue = pref.getActionOnDblClOnTorrentFn();
  if (intValue >= actionTorrentFnOnDblClBox->count())
    intValue = 1;
  actionTorrentFnOnDblClBox->setCurrentIndex(intValue);
  // End Downloads preferences
  // Connection preferences
  spinPort->setValue(pref.getSessionPort());
  checkUPnP->setChecked(pref.isUPnPEnabled());
  intValue = pref.getGlobalDownloadLimit();
  if (intValue > 0) {
    // Enabled
    checkDownloadLimit->setChecked(true);
    spinDownloadLimit->setEnabled(true);
    spinDownloadLimit->setValue(intValue);
  } else {
    // Disabled
    checkDownloadLimit->setChecked(false);
    spinDownloadLimit->setEnabled(false);
  }
  intValue = pref.getGlobalUploadLimit();
  if (intValue != -1) {
    // Enabled
    checkUploadLimit->setChecked(true);
    spinUploadLimit->setEnabled(true);
    spinUploadLimit->setValue(intValue);
  } else {
    // Disabled
    checkUploadLimit->setChecked(false);
    spinUploadLimit->setEnabled(false);
  }
  spinUploadLimitAlt->setValue(pref.getAltGlobalUploadLimit());
  spinDownloadLimitAlt->setValue(pref.getAltGlobalDownloadLimit());
  // Options
  checkuTP->setChecked(pref.isuTPEnabled());
  checkLimituTPConnections->setChecked(pref.isuTPRateLimited());
  checkLimitTransportOverhead->setChecked(pref.includeOverheadInLimits());
  // Scheduler
  check_schedule->setChecked(pref.isSchedulerEnabled());
  schedule_from->setTime(pref.getSchedulerStartTime());
  schedule_to->setTime(pref.getSchedulerEndTime());
  schedule_days->setCurrentIndex((int)pref.getSchedulerDays());

  intValue = pref.getProxyType();
  switch(intValue) {
  case Proxy::SOCKS4:
    comboProxyType->setCurrentIndex(1);
    break;
  case Proxy::SOCKS5:
  case Proxy::SOCKS5_PW:
    comboProxyType->setCurrentIndex(2);
    break;
  case Proxy::HTTP:
  case Proxy::HTTP_PW:
    comboProxyType->setCurrentIndex(3);
    break;
  default:
    comboProxyType->setCurrentIndex(0);
  }
  enableProxy(comboProxyType->currentIndex());
  //if (isProxyEnabled()) {
  // Proxy is enabled, save settings
  textProxyIP->setText(pref.getProxyIp());
  spinProxyPort->setValue(pref.getProxyPort());
  checkProxyPeerConnecs->setChecked(pref.proxyPeerConnections());
  checkProxyAuth->setChecked(pref.isProxyAuthEnabled());
  textProxyUsername->setText(pref.getProxyUsername());
  textProxyPassword->setText(pref.getProxyPassword());
  //}
  // End Connection preferences
  // Bittorrent preferences
  intValue = pref.getMaxConnecs();
  if (intValue > 0) {
    // enable
    checkMaxConnecs->setChecked(true);
    spinMaxConnec->setEnabled(true);
    spinMaxConnec->setValue(intValue);
  } else {
    // disable
    checkMaxConnecs->setChecked(false);
    spinMaxConnec->setEnabled(false);
  }
  intValue = pref.getMaxConnecsPerTorrent();
  if (intValue > 0) {
    // enable
    checkMaxConnecsPerTorrent->setChecked(true);
    spinMaxConnecPerTorrent->setEnabled(true);
    spinMaxConnecPerTorrent->setValue(intValue);
  } else {
    // disable
    checkMaxConnecsPerTorrent->setChecked(false);
    spinMaxConnecPerTorrent->setEnabled(false);
  }
  intValue = pref.getMaxUploadsPerTorrent();
  if (intValue > 0) {
    // enable
    checkMaxUploadsPerTorrent->setChecked(true);
    spinMaxUploadsPerTorrent->setEnabled(true);
    spinMaxUploadsPerTorrent->setValue(intValue);
  } else {
    // disable
    checkMaxUploadsPerTorrent->setChecked(false);
    spinMaxUploadsPerTorrent->setEnabled(false);
  }
  checkDHT->setChecked(pref.isDHTEnabled());
  checkDifferentDHTPort->setChecked(!pref.isDHTPortSameAsBT());
  spinDHTPort->setValue(pref.getDHTPort());
  checkPeX->setChecked(pref.isPeXEnabled());
  checkLSD->setChecked(pref.isLSDEnabled());
  comboEncryption->setCurrentIndex(pref.getEncryptionSetting());
#if LIBTORRENT_VERSION_MINOR > 15
  checkAnonymousMode->setChecked(pref.isAnonymousModeEnabled());
  /* make sure ui matches options */
  toggleAnonymousMode(checkAnonymousMode->isChecked());
#endif
  // Ratio limit
  floatValue = pref.getGlobalMaxRatio();
  if (floatValue >= 0.) {
    // Enable
    checkMaxRatio->setChecked(true);
    spinMaxRatio->setEnabled(true);
    comboRatioLimitAct->setEnabled(true);
    spinMaxRatio->setValue(floatValue);
  } else {
    // Disable
    checkMaxRatio->setChecked(false);
    spinMaxRatio->setEnabled(false);
    comboRatioLimitAct->setEnabled(false);
  }
  comboRatioLimitAct->setCurrentIndex(pref.getMaxRatioAction());
  // End Bittorrent preferences
  // Misc preferences
  // * IP Filter
  checkIPFilter->setChecked(pref.isFilteringEnabled());
  textFilterPath->setText(pref.getFilter());
  // End IP Filter
  // Queueing system preferences
  checkEnableQueueing->setChecked(pref.isQueueingSystemEnabled());
  spinMaxActiveDownloads->setValue(pref.getMaxActiveDownloads());
  spinMaxActiveUploads->setValue(pref.getMaxActiveUploads());
  spinMaxActiveTorrents->setValue(pref.getMaxActiveTorrents());
  checkIgnoreSlowTorrentsForQueueing->setChecked(pref.ignoreSlowTorrentsForQueueing());
  // End Queueing system preferences
  // Web UI
  checkWebUi->setChecked(pref.isWebUiEnabled());
  spinWebUiPort->setValue(pref.getWebUiPort());
  checkWebUIUPnP->setChecked(pref.useUPnPForWebUIPort());
  checkWebUiHttps->setChecked(pref.isWebUiHttpsEnabled());
  setSslCertificate(pref.getWebUiHttpsCertificate(), false);
  setSslKey(pref.getWebUiHttpsKey(), false);
  textWebUiUsername->setText(pref.getWebUiUsername());
  textWebUiPassword->setText(pref.getWebUiPassword());
  checkBypassLocalAuth->setChecked(!pref.isWebUiLocalAuthEnabled());
  // Dynamic DNS
  checkDynDNS->setChecked(pref.isDynDNSEnabled());
  comboDNSService->setCurrentIndex((int)pref.getDynDNSService());
  domainNameTxt->setText(pref.getDynDomainName());
  DNSUsernameTxt->setText(pref.getDynDNSUsername());
  DNSPasswordTxt->setText(pref.getDynDNSPassword());
  // End Web UI
  // Random stuff
  srand(time(0));
}

// return min & max ports
// [min, max]
int options_imp::getPort() const {
  return spinPort->value();
}

void options_imp::on_randomButton_clicked() {
  // Range [1024: 65535]
  spinPort->setValue(rand() % 64512 + 1024);
}

int options_imp::getEncryptionSetting() const {
  return comboEncryption->currentIndex();
}

int options_imp::getMaxActiveDownloads() const {
  return spinMaxActiveDownloads->value();
}

int options_imp::getMaxActiveUploads() const {
  return spinMaxActiveUploads->value();
}

int options_imp::getMaxActiveTorrents() const {
  return spinMaxActiveTorrents->value();
}

bool options_imp::minimizeToTray() const {
  if (!checkShowSystray->isChecked()) return false;
  return checkMinimizeToSysTray->isChecked();
}

bool options_imp::closeToTray() const {
  if (!checkShowSystray->isChecked()) return false;
  return checkCloseToSystray->isChecked();
}

bool options_imp::isQueueingSystemEnabled() const {
  return checkEnableQueueing->isChecked();
}

bool options_imp::isDHTEnabled() const {
  return checkDHT->isChecked();
}

bool options_imp::isLSDEnabled() const {
  return checkLSD->isChecked();
}

bool options_imp::isUPnPEnabled() const {
  return checkUPnP->isChecked();
}

// Return Download & Upload limits in kbps
// [download,upload]
QPair<int,int> options_imp::getGlobalBandwidthLimits() const {
  int DL = -1, UP = -1;
  if (checkDownloadLimit->isChecked()) {
    DL = spinDownloadLimit->value();
  }
  if (checkUploadLimit->isChecked()) {
    UP = spinUploadLimit->value();
  }
  return qMakePair(DL, UP);
}

bool options_imp::startMinimized() const {
  if (checkStartMinimized->isChecked()) return true;
  return checkStartMinimized->isChecked();
}

bool options_imp::systrayIntegration() const {
  if (!QSystemTrayIcon::isSystemTrayAvailable()) return false;
  return checkShowSystray->isChecked();
}

int options_imp::getDHTPort() const {
  return spinDHTPort->value();
}

// Return Share ratio
qreal options_imp::getMaxRatio() const {
  if (checkMaxRatio->isChecked()) {
    return spinMaxRatio->value();
  }
  return -1;
}

// Return Save Path
QString options_imp::getSavePath() const {
  if (textSavePath->text().trimmed().isEmpty()) {
    QString save_path = Preferences().getSavePath();
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
    save_path.replace("/", "\\");
#endif
    textSavePath->setText(save_path);
  }
  return fsutils::expandPath(textSavePath->text());
}

QString options_imp::getTempPath() const {
  return fsutils::expandPath(textTempPath->text());
}

bool options_imp::isTempPathEnabled() const {
  return checkTempFolder->isChecked();
}

// Return max connections number
int options_imp::getMaxConnecs() const {
  if (!checkMaxConnecs->isChecked()) {
    return -1;
  }else{
    return spinMaxConnec->value();
  }
}

int options_imp::getMaxConnecsPerTorrent() const {
  if (!checkMaxConnecsPerTorrent->isChecked()) {
    return -1;
  }else{
    return spinMaxConnecPerTorrent->value();
  }
}

int options_imp::getMaxUploadsPerTorrent() const {
  if (!checkMaxUploadsPerTorrent->isChecked()) {
    return -1;
  }else{
    return spinMaxUploadsPerTorrent->value();
  }
}

void options_imp::on_buttonBox_accepted() {
  if (applyButton->isEnabled()) {
    saveOptions();
    applyButton->setEnabled(false);
    this->hide();
    emit status_changed();
  }
  saveWindowState();
  accept();
}

void options_imp::applySettings(QAbstractButton* button) {
  if (button == applyButton) {
    saveOptions();
    emit status_changed();
  }
}

void options_imp::closeEvent(QCloseEvent *e) {
  setAttribute(Qt::WA_DeleteOnClose);
  e->accept();
}

void options_imp::on_buttonBox_rejected() {
  setAttribute(Qt::WA_DeleteOnClose);
  reject();
}

bool options_imp::useAdditionDialog() const {
  return checkAdditionDialog->isChecked();
}

void options_imp::enableApplyButton() {
  applyButton->setEnabled(true);
}

void options_imp::enableProxy(int index) {
  if (index) {
    //enable
    lblProxyIP->setEnabled(true);
    textProxyIP->setEnabled(true);
    lblProxyPort->setEnabled(true);
    spinProxyPort->setEnabled(true);
    checkProxyPeerConnecs->setEnabled(true);
    if (index > 1) {
      checkProxyAuth->setEnabled(true);
    } else {
      checkProxyAuth->setEnabled(false);
      checkProxyAuth->setChecked(false);
    }
  }else{
    //disable
    lblProxyIP->setEnabled(false);
    textProxyIP->setEnabled(false);
    lblProxyPort->setEnabled(false);
    spinProxyPort->setEnabled(false);
    checkProxyPeerConnecs->setEnabled(false);
    checkProxyAuth->setEnabled(false);
    checkProxyAuth->setChecked(false);
  }
}

bool options_imp::isSlashScreenDisabled() const {
  return !checkShowSplash->isChecked();
}

#ifdef Q_WS_WIN
bool options_imp::Startup() const {
  return checkStartup->isChecked();
}
#endif

bool options_imp::preventFromSuspend() const {
  return checkPreventFromSuspend->isChecked();
}

bool options_imp::preAllocateAllFiles() const {
  return checkPreallocateAll->isChecked();
}

bool options_imp::addTorrentsInPause() const {
  return checkStartPaused->isChecked();
}

bool options_imp::isDHTPortSameAsBT() const {
  return !checkDifferentDHTPort->isChecked();
}

// Proxy settings
bool options_imp::isProxyEnabled() const {
  return comboProxyType->currentIndex();
}

bool options_imp::isProxyAuthEnabled() const {
  return checkProxyAuth->isChecked();
}

QString options_imp::getProxyIp() const {
  return textProxyIP->text().trimmed();
}

unsigned short options_imp::getProxyPort() const {
  return spinProxyPort->value();
}

QString options_imp::getProxyUsername() const {
  QString username = textProxyUsername->text();
  username = username.trimmed();
  return username;
}

QString options_imp::getProxyPassword() const {
  QString password = textProxyPassword->text();
  password = password.trimmed();
  return password;
}

// Locale Settings
QString options_imp::getLocale() const {
  return comboI18n->itemData(comboI18n->currentIndex(), Qt::UserRole).toString();
}

void options_imp::setLocale(const QString &localeStr) {
  QLocale locale(localeStr);
  // Attempt to find exact match
  int index = comboI18n->findData(locale.name(), Qt::UserRole);
  if (index < 0) {
    // Unreconized, use US English
    index = comboI18n->findData(QLocale("en").name(), Qt::UserRole);
    Q_ASSERT(index >= 0);
  }
  comboI18n->setCurrentIndex(index);
}

QString options_imp::getTorrentExportDir() const {
  if (checkExportDir->isChecked())
    return fsutils::expandPath(textExportDir->text());
  return QString();
}

QString options_imp::getFinishedTorrentExportDir() const {
  if (checkExportDirFin->isChecked())
    return fsutils::expandPath(textExportDirFin->text());
  return QString();
}

// Return action on double-click on a downloading torrent set in options
int options_imp::getActionOnDblClOnTorrentDl() const {
  if (actionTorrentDlOnDblClBox->currentIndex() < 1)
    return 0;
  return actionTorrentDlOnDblClBox->currentIndex();
}

// Return action on double-click on a finished torrent set in options
int options_imp::getActionOnDblClOnTorrentFn() const {
  if (actionTorrentFnOnDblClBox->currentIndex() < 1)
    return 0;
  return actionTorrentFnOnDblClBox->currentIndex();
}

void options_imp::on_addScanFolderButton_clicked() {
  const QString dir = QFileDialog::getExistingDirectory(this, tr("Add directory to scan"));
  if (!dir.isEmpty()) {
    const ScanFoldersModel::PathStatus status = ScanFoldersModel::instance()->addPath(dir, false);
    QString error;
    switch (status) {
    case ScanFoldersModel::AlreadyInList:
      error = tr("Folder is already being watched.").arg(dir);
      break;
    case ScanFoldersModel::DoesNotExist:
      error = tr("Folder does not exist.");
      break;
    case ScanFoldersModel::CannotRead:
      error = tr("Folder is not readable.");
      break;
    default:
      addedScanDirs << dir;
      enableApplyButton();
    }

    if (!error.isEmpty()) {
      QMessageBox::warning(this, tr("Failure"), tr("Failed to add Scan Folder '%1': %2").arg(dir).arg(error));
    }
  }
}

void options_imp::on_removeScanFolderButton_clicked() {
  const QModelIndexList selected
      = scanFoldersView->selectionModel()->selectedIndexes();
  if (selected.isEmpty())
    return;
  Q_ASSERT(selected.count() == ScanFoldersModel::instance()->columnCount());
  ScanFoldersModel::instance()->removePath(selected.first().row());
}

void options_imp::handleScanFolderViewSelectionChanged() {
  removeScanFolderButton->setEnabled(!scanFoldersView->selectionModel()->selectedIndexes().isEmpty());
}

QString options_imp::askForExportDir(const QString& currentExportPath)
{
  QDir currentExportDir(fsutils::expandPath(currentExportPath));
  QString dir;
  if (!currentExportPath.isEmpty() && currentExportDir.exists()) {
    dir = QFileDialog::getExistingDirectory(this, tr("Choose export directory"), currentExportDir.absolutePath());
  } else {
    dir = QFileDialog::getExistingDirectory(this, tr("Choose export directory"), QDir::homePath());
  }
  return dir;
}

void options_imp::on_browseExportDirButton_clicked() {
  const QString newExportDir = askForExportDir(textExportDir->text());
  if (!newExportDir.isNull())
    textExportDir->setText(fsutils::toDisplayPath(newExportDir));
}

void options_imp::on_browseExportDirFinButton_clicked() {
  const QString newExportDir = askForExportDir(textExportDirFin->text());
  if (!newExportDir.isNull())
    textExportDirFin->setText(fsutils::toDisplayPath(newExportDir));
}

void options_imp::on_browseFilterButton_clicked() {
  const QString filter_path = fsutils::expandPath(textFilterPath->text());
  QDir filterDir(filter_path);
  QString ipfilter;
  if (!filter_path.isEmpty() && filterDir.exists()) {
    ipfilter = QFileDialog::getOpenFileName(this, tr("Choose an ip filter file"), filterDir.absolutePath(), tr("Filters")+QString(" (*.dat *.p2p *.p2b)"));
  } else {
    ipfilter = QFileDialog::getOpenFileName(this, tr("Choose an ip filter file"), QDir::homePath(), tr("Filters")+QString(" (*.dat *.p2p *.p2b)"));
  }
  if (!ipfilter.isNull()) {
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
    ipfilter.replace("/", "\\");
#endif
    textFilterPath->setText(ipfilter);
  }
}

// Display dialog to choose save dir
void options_imp::on_browseSaveDirButton_clicked() {
  const QString save_path = fsutils::expandPath(textSavePath->text());
  QDir saveDir(save_path);
  QString dir;
  if (!save_path.isEmpty() && saveDir.exists()) {
    dir = QFileDialog::getExistingDirectory(this, tr("Choose a save directory"), saveDir.absolutePath());
  } else {
    dir = QFileDialog::getExistingDirectory(this, tr("Choose a save directory"), QDir::homePath());
  }
  if (!dir.isNull()) {
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
    dir.replace("/", "\\");
#endif
    textSavePath->setText(dir);
  }
}

void options_imp::on_browseTempDirButton_clicked() {
  const QString temp_path = fsutils::expandPath(textTempPath->text());
  QDir tempDir(temp_path);
  QString dir;
  if (!temp_path.isEmpty() && tempDir.exists()) {
    dir = QFileDialog::getExistingDirectory(this, tr("Choose a save directory"), tempDir.absolutePath());
  } else {
    dir = QFileDialog::getExistingDirectory(this, tr("Choose a save directory"), QDir::homePath());
  }
  if (!dir.isNull()) {
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
    dir.replace("/", "\\");
#endif
    textTempPath->setText(dir);
  }
}

// Return Filter object to apply to BT session
QString options_imp::getFilter() const {
  return textFilterPath->text();
}

// Web UI

bool options_imp::isWebUiEnabled() const
{
  return checkWebUi->isChecked();
}

quint16 options_imp::webUiPort() const
{
  return spinWebUiPort->value();
}

QString options_imp::webUiUsername() const
{
  return textWebUiUsername->text();
}

QString options_imp::webUiPassword() const
{
  return textWebUiPassword->text();
}

void options_imp::showConnectionTab()
{
  tabSelection->setCurrentRow(2);
}

void options_imp::on_btnWebUiCrt_clicked() {
  QString filename = QFileDialog::getOpenFileName(this, QString(), QString(), tr("SSL Certificate (*.crt *.pem)"));
  if (filename.isNull())
    return;
  QFile file(filename);
  if (file.open(QIODevice::ReadOnly)) {
    setSslCertificate(file.readAll());
    file.close();
  }
}

void options_imp::on_btnWebUiKey_clicked() {
  QString filename = QFileDialog::getOpenFileName(this, QString(), QString(), tr("SSL Key (*.key *.pem)"));
  if (filename.isNull())
    return;
  QFile file(filename);
  if (file.open(QIODevice::ReadOnly)) {
    setSslKey(file.readAll());
    file.close();
  }
}

void options_imp::on_registerDNSBtn_clicked() {
  QDesktopServices::openUrl(DNSUpdater::getRegistrationUrl(comboDNSService->currentIndex()));
}

void options_imp::on_IpFilterRefreshBtn_clicked() {
  if (m_refreshingIpFilter) return;
  m_refreshingIpFilter = true;
  // Updating program preferences
  Preferences pref;
  pref.setFilteringEnabled(true);
  pref.setFilter(getFilter());
  // Force refresh
  connect(QBtSession::instance(), SIGNAL(ipFilterParsed(bool, int)), SLOT(handleIPFilterParsed(bool, int)));
  setCursor(QCursor(Qt::WaitCursor));
  QBtSession::instance()->enableIPFilter(getFilter(), true);
}

void options_imp::handleIPFilterParsed(bool error, int ruleCount)
{
  setCursor(QCursor(Qt::ArrowCursor));
  if (error) {
    QMessageBox::warning(this, tr("Parsing error"), tr("Failed to parse the provided IP filter"));
  } else {
    QMessageBox::information(this, tr("Successfully refreshed"), tr("Successfully parsed the provided IP filter: %1 rules were applied.", "%1 is a number").arg(ruleCount));
  }
  m_refreshingIpFilter = false;
  disconnect(QBtSession::instance(), SIGNAL(ipFilterParsed(bool, int)), this, SLOT(handleIPFilterParsed(bool, int)));
}

QString options_imp::languageToLocalizedString(QLocale::Language language, const QString& country)
{
  switch(language) {
  case QLocale::English: return "English";
  case QLocale::French: return QString::fromUtf8("Français");
  case QLocale::German: return QString::fromUtf8("Deutsch");
  case QLocale::Hungarian: return QString::fromUtf8("Magyar");
  case QLocale::Italian: return QString::fromUtf8("Italiano");
  case QLocale::Dutch: return QString::fromUtf8("Nederlands");
  case QLocale::Spanish: return QString::fromUtf8("Español");
  case QLocale::Catalan: return QString::fromUtf8("Català");
  case QLocale::Galician: return QString::fromUtf8("Galego");
  case QLocale::Portuguese: {
    if (country == "br")
      return QString::fromUtf8("Português brasileiro");
    return QString::fromUtf8("Português");
  }
  case QLocale::Polish: return QString::fromUtf8("Polski");
  case QLocale::Lithuanian: return QString::fromUtf8("Lietuvių");
  case QLocale::Czech: return QString::fromUtf8("Čeština");
  case QLocale::Slovak: return QString::fromUtf8("Slovenčina");
  case QLocale::Serbian: return QString::fromUtf8("Српски");
  case QLocale::Croatian: return QString::fromUtf8("Hrvatski");
  case QLocale::Armenian: return QString::fromUtf8("Հայերեն");
  case QLocale::Romanian: return QString::fromUtf8("Română");
  case QLocale::Turkish: return QString::fromUtf8("Türkçe");
  case QLocale::Greek: return QString::fromUtf8("Ελληνικά");
  case QLocale::Swedish: return QString::fromUtf8("Svenska");
  case QLocale::Finnish: return QString::fromUtf8("Suomi");
  case QLocale::Norwegian: return QString::fromUtf8("Norsk");
  case QLocale::Danish: return QString::fromUtf8("Dansk");
  case QLocale::Bulgarian: return QString::fromUtf8("Български");
  case QLocale::Ukrainian: return QString::fromUtf8("Українська");
  case QLocale::Russian: return QString::fromUtf8("Русский");
  case QLocale::Japanese: return QString::fromUtf8("日本語");
  case QLocale::Hebrew: return QString::fromUtf8("עברית");
  case QLocale::Arabic: return QString::fromUtf8("عربي");
  case QLocale::Georgian: return QString::fromUtf8("ქართული");
  case QLocale::Byelorussian: return QString::fromUtf8("Беларуская");
  case QLocale::Basque: return QString::fromUtf8("Euskara");
  case QLocale::Chinese: {
    if (country == "cn")
      return QString::fromUtf8("中文 (简体)");
    return QString::fromUtf8("中文 (繁體)");
  }
  case QLocale::Korean: return QString::fromUtf8("한글");
  default: {
    // Fallback to English
    const QString eng_lang = QLocale::languageToString(language);
    qWarning() << "Unrecognized language name: " << eng_lang;
    return eng_lang;
  }
  }
}

void options_imp::setSslKey(const QByteArray &key, bool interactive)
{
#ifndef QT_NO_OPENSSL
  if (!key.isEmpty() && !QSslKey(key, QSsl::Rsa).isNull()) {
    lblSslKeyStatus->setPixmap(QPixmap(":/Icons/oxygen/security-high.png").scaledToHeight(20, Qt::SmoothTransformation));
    m_sslKey = key;
  } else {
    lblSslKeyStatus->setPixmap(QPixmap(":/Icons/oxygen/security-low.png").scaledToHeight(20, Qt::SmoothTransformation));
    m_sslKey.clear();
    if (interactive)
      QMessageBox::warning(this, tr("Invalid key"), tr("This is not a valid SSL key."));
  }
#endif
}

void options_imp::setSslCertificate(const QByteArray &cert, bool interactive)
{
#ifndef QT_NO_OPENSSL
  if (!cert.isEmpty() && !QSslCertificate(cert).isNull()) {
    lblSslCertStatus->setPixmap(QPixmap(":/Icons/oxygen/security-high.png").scaledToHeight(20, Qt::SmoothTransformation));
    m_sslCert = cert;
  } else {
    lblSslCertStatus->setPixmap(QPixmap(":/Icons/oxygen/security-low.png").scaledToHeight(20, Qt::SmoothTransformation));
    m_sslCert.clear();
    if (interactive)
      QMessageBox::warning(this, tr("Invalid certificate"), tr("This is not a valid SSL certificate."));
  }
#endif
}

void options_imp::toggleAnonymousMode(bool enabled)
{
  if (enabled) {
    // Disable DHT, LSD, UPnP / NAT-PMP
    checkDHT->setEnabled(false);
    checkDifferentDHTPort->setEnabled(false);
    checkDHT->setChecked(false);
    checkLSD->setEnabled(false);
    checkLSD->setChecked(false);
    checkUPnP->setEnabled(false);
    checkUPnP->setChecked(false);
  } else {
    checkDHT->setEnabled(true);
    checkDifferentDHTPort->setEnabled(true);
    checkLSD->setEnabled(true);
    checkUPnP->setEnabled(true);
  }
}
