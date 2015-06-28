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
#include <QSystemTrayIcon>
#include <QApplication>
#include <QDialogButtonBox>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QTranslator>
#include <QDesktopServices>
#include <QDebug>

#include <libtorrent/version.hpp>

#include <cstdlib>
#include "options_imp.h"
#include "core/preferences.h"
#include "core/utils/fs.h"
#include "advancedsettings.h"
#include "core/scanfoldersmodel.h"
#include "core/bittorrent/session.h"
#include "guiiconprovider.h"
#include "core/net/dnsupdater.h"
#include "core/unicodestrings.h"

#ifndef QT_NO_OPENSSL
#include <QSslKey>
#include <QSslCertificate>
#endif

// Constructor
options_imp::options_imp(QWidget *parent):
  QDialog(parent), m_refreshingIpFilter(false) {
  qDebug("-> Constructing Options");
  setupUi(this);
  setAttribute(Qt::WA_DeleteOnClose);
  setModal(true);
  // Icons
  tabSelection->item(TAB_UI)->setIcon(GuiIconProvider::instance()->getIcon("preferences-desktop"));
  tabSelection->item(TAB_BITTORRENT)->setIcon(GuiIconProvider::instance()->getIcon("preferences-system-network"));
  tabSelection->item(TAB_CONNECTION)->setIcon(GuiIconProvider::instance()->getIcon("network-wired"));
  tabSelection->item(TAB_DOWNLOADS)->setIcon(GuiIconProvider::instance()->getIcon("download"));
  tabSelection->item(TAB_SPEED)->setIcon(GuiIconProvider::instance()->getIcon("chronometer"));
#ifndef DISABLE_WEBUI
  tabSelection->item(TAB_WEBUI)->setIcon(GuiIconProvider::instance()->getIcon("network-server"));
#else
  tabSelection->item(TAB_WEBUI)->setHidden(true);
#endif
  tabSelection->item(TAB_ADVANCED)->setIcon(GuiIconProvider::instance()->getIcon("preferences-other"));
  IpFilterRefreshBtn->setIcon(GuiIconProvider::instance()->getIcon("view-refresh"));

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

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
  scanFoldersView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#else
  scanFoldersView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#endif
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
  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    checkShowSystray->setChecked(false);
    checkShowSystray->setEnabled(false);
    label_trayIconStyle->setVisible(false);
    comboTrayIcon->setVisible(false);
  }

#if defined(QT_NO_OPENSSL)
  checkWebUiHttps->setVisible(false);
#endif

#ifndef Q_OS_WIN
  checkStartup->setVisible(false);
  groupFileAssociation->setVisible(false);
#endif

  // Connect signals / slots
  connect(comboProxyType, SIGNAL(currentIndexChanged(int)),this, SLOT(enableProxy(int)));
  connect(checkRandomPort, SIGNAL(toggled(bool)), spinPort, SLOT(setDisabled(bool)));

  // Apply button is activated when a value is changed
  // General tab
  connect(comboI18n, SIGNAL(currentIndexChanged(int)), this, SLOT(enableApplyButton()));
  connect(checkAltRowColors, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkShowSystray, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkCloseToSystray, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkMinimizeToSysTray, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkStartMinimized, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
#ifdef Q_OS_WIN
  connect(checkStartup, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
#endif
  connect(checkShowSplash, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkProgramExitConfirm, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkPreventFromSuspend, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(comboTrayIcon, SIGNAL(currentIndexChanged(int)), this, SLOT(enableApplyButton()));
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC)) && !defined(QT_DBUS_LIB)
  checkPreventFromSuspend->setDisabled(true);
#endif
#ifdef Q_OS_WIN
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
  connect(checkAdditionDialogFront, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
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
  connect(checkRandomPort, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkUPnP, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkUploadLimit, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkDownloadLimit, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkUploadLimitAlt, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkDownloadLimitAlt, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
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
  connect(checkMaxUploads, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkMaxUploadsPerTorrent, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(spinMaxConnec, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spinMaxConnecPerTorrent, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spinMaxUploads, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spinMaxUploadsPerTorrent, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(checkDHT, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkAnonymousMode, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkPeX, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
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
#if LIBTORRENT_VERSION_NUM >= 10000
  connect(checkForceProxy, SIGNAL(toggled(bool)), SLOT(enableApplyButton()));
#endif
  connect(checkProxyAuth, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(textProxyUsername, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(textProxyPassword, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  // Misc tab
  connect(checkIPFilter, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(checkIpFilterTrackers, SIGNAL(toggled(bool)), SLOT(enableApplyButton()));
  connect(textFilterPath, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(checkEnableQueueing, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(spinMaxActiveDownloads, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spinMaxActiveUploads, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spinMaxActiveTorrents, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(checkIgnoreSlowTorrentsForQueueing, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
#ifndef DISABLE_WEBUI
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
#endif
  // Disable apply Button
  applyButton->setEnabled(false);
  // Tab selection mechanism
  connect(tabSelection, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), this, SLOT(changePage(QListWidgetItem *, QListWidgetItem*)));
  // Load Advanced settings
  QVBoxLayout *adv_layout = new QVBoxLayout();
  advancedSettings = new AdvancedSettings();
  adv_layout->addWidget(advancedSettings);
  scrollArea_advanced->setLayout(adv_layout);
  connect(advancedSettings, SIGNAL(settingsChanged()), this, SLOT(enableApplyButton()));

  //Hide incompatible options
#if LIBTORRENT_VERSION_NUM < 10000
  checkForceProxy->setVisible(false);
#endif

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
    QString language_name = languageToLocalizedString(locale);
    comboI18n->addItem(/*QIcon(":/icons/flags/"+country+".png"), */language_name, localeStr);
    qDebug() << "Supported locale:" << localeStr;
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
  const Preferences* const pref = Preferences::instance();
  resize(pref->getPrefSize(sizeFittingScreen()));
  QPoint p = pref->getPrefPos();
  QRect scr_rect = qApp->desktop()->screenGeometry();
  if (!p.isNull() && scr_rect.contains(p))
    move(p);
  // Load slider size
  const QStringList sizes_str = pref->getPrefHSplitterSizes();
  // Splitter size
  QList<int> sizes;
  if (sizes_str.size() == 2) {
    sizes << sizes_str.first().toInt();
    sizes << sizes_str.last().toInt();
  } else {
    sizes << 116;
    sizes << hsplitter->width()-116;
  }
  hsplitter->setSizes(sizes);
}

void options_imp::saveWindowState() const {
  Preferences* const pref = Preferences::instance();
  pref->setPrefSize(size());
  pref->setPrefPos(pos());
  // Splitter size
  QStringList sizes_str;
  sizes_str << QString::number(hsplitter->sizes().first());
  sizes_str << QString::number(hsplitter->sizes().last());
  pref->setPrefHSplitterSizes(sizes_str);
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
  Preferences* const pref = Preferences::instance();
  // Load the translation
  QString locale = getLocale();
  if (pref->getLocale() != locale) {
    QTranslator *translator = new QTranslator;
    if (translator->load(QString::fromUtf8(":/lang/qbittorrent_") + locale)) {
      qDebug("%s locale recognized, using translation.", qPrintable(locale));
    }else{
      qDebug("%s locale unrecognized, using default (en).", qPrintable(locale));
    }
    qApp->installTranslator(translator);
  }

  // General preferences
  pref->setLocale(locale);
  pref->setAlternatingRowColors(checkAltRowColors->isChecked());
  pref->setSystrayIntegration(systrayIntegration());
  pref->setTrayIconStyle(TrayIcon::Style(comboTrayIcon->currentIndex()));
  pref->setCloseToTray(closeToTray());
  pref->setMinimizeToTray(minimizeToTray());
  pref->setStartMinimized(startMinimized());
  pref->setSplashScreenDisabled(isSlashScreenDisabled());
  pref->setConfirmOnExit(checkProgramExitConfirm->isChecked());
  pref->setPreventFromSuspend(preventFromSuspend());
#ifdef Q_OS_WIN
  pref->setWinStartup(WinStartup());
  // Windows: file association settings
  Preferences::setTorrentFileAssoc(checkAssociateTorrents->isChecked());
  Preferences::setMagnetLinkAssoc(checkAssociateMagnetLinks->isChecked());
#endif
  // End General preferences

  // Downloads preferences
  pref->setSavePath(getSavePath());
  pref->setTempPathEnabled(isTempPathEnabled());
  pref->setTempPath(getTempPath());
  pref->setAppendTorrentLabel(checkAppendLabel->isChecked());
  pref->useIncompleteFilesExtension(checkAppendqB->isChecked());
  pref->preAllocateAllFiles(preAllocateAllFiles());
  pref->useAdditionDialog(useAdditionDialog());
  pref->additionDialogFront(checkAdditionDialogFront->isChecked());
  pref->addTorrentsInPause(addTorrentsInPause());
  ScanFoldersModel::instance()->makePersistent();
  addedScanDirs.clear();
  pref->setTorrentExportDir(getTorrentExportDir());
  pref->setFinishedTorrentExportDir(getFinishedTorrentExportDir());
  pref->setMailNotificationEnabled(groupMailNotification->isChecked());
  pref->setMailNotificationEmail(dest_email_txt->text());
  pref->setMailNotificationSMTP(smtp_server_txt->text());
  pref->setMailNotificationSMTPSSL(checkSmtpSSL->isChecked());
  pref->setMailNotificationSMTPAuth(groupMailNotifAuth->isChecked());
  pref->setMailNotificationSMTPUsername(mailNotifUsername->text());
  pref->setMailNotificationSMTPPassword(mailNotifPassword->text());
  pref->setAutoRunEnabled(autoRunBox->isChecked());
  pref->setAutoRunProgram(autoRun_txt->text());
  pref->setActionOnDblClOnTorrentDl(getActionOnDblClOnTorrentDl());
  pref->setActionOnDblClOnTorrentFn(getActionOnDblClOnTorrentFn());
  // End Downloads preferences
  // Connection preferences
  pref->setSessionPort(getPort());
  pref->setRandomPort(checkRandomPort->isChecked());
  pref->setUPnPEnabled(isUPnPEnabled());
  const QPair<int, int> down_up_limit = getGlobalBandwidthLimits();
  pref->setGlobalDownloadLimit(down_up_limit.first);
  pref->setGlobalUploadLimit(down_up_limit.second);
  pref->setuTPEnabled(checkuTP->isChecked());
  pref->setuTPRateLimited(checkLimituTPConnections->isChecked());
  pref->includeOverheadInLimits(checkLimitTransportOverhead->isChecked());
  const QPair<int, int> alt_down_up_limit = getAltGlobalBandwidthLimits();
  pref->setAltGlobalDownloadLimit(alt_down_up_limit.first);
  pref->setAltGlobalUploadLimit(alt_down_up_limit.second);
  pref->setSchedulerEnabled(check_schedule->isChecked());
  pref->setSchedulerStartTime(schedule_from->time());
  pref->setSchedulerEndTime(schedule_to->time());
  pref->setSchedulerDays((scheduler_days)schedule_days->currentIndex());
  pref->setProxyType(getProxyType());
  pref->setProxyIp(getProxyIp());
  pref->setProxyPort(getProxyPort());
  pref->setProxyPeerConnections(checkProxyPeerConnecs->isChecked());
#if LIBTORRENT_VERSION_NUM >= 10000
  pref->setForceProxy(checkForceProxy->isChecked());
#endif
  pref->setProxyAuthEnabled(isProxyAuthEnabled());
  pref->setProxyUsername(getProxyUsername());
  pref->setProxyPassword(getProxyPassword());
  // End Connection preferences
  // Bittorrent preferences
  pref->setMaxConnecs(getMaxConnecs());
  pref->setMaxConnecsPerTorrent(getMaxConnecsPerTorrent());
  pref->setMaxUploads(getMaxUploads());
  pref->setMaxUploadsPerTorrent(getMaxUploadsPerTorrent());
  pref->setDHTEnabled(isDHTEnabled());
  pref->setPeXEnabled(checkPeX->isChecked());
  pref->setLSDEnabled(isLSDEnabled());
  pref->setEncryptionSetting(getEncryptionSetting());
  pref->enableAnonymousMode(checkAnonymousMode->isChecked());
  pref->setGlobalMaxRatio(getMaxRatio());
  pref->setMaxRatioAction(comboRatioLimitAct->currentIndex());
  // End Bittorrent preferences
  // Misc preferences
  // * IPFilter
  pref->setFilteringEnabled(isFilteringEnabled());
  if (isFilteringEnabled()) {
    pref->setFilteringTrackerEnabled(checkIpFilterTrackers->isChecked());
    pref->setFilter(textFilterPath->text());
  }
  // End IPFilter preferences
  // Queueing system
  pref->setQueueingSystemEnabled(isQueueingSystemEnabled());
  pref->setMaxActiveDownloads(spinMaxActiveDownloads->value());
  pref->setMaxActiveUploads(spinMaxActiveUploads->value());
  pref->setMaxActiveTorrents(spinMaxActiveTorrents->value());
  pref->setIgnoreSlowTorrentsForQueueing(checkIgnoreSlowTorrentsForQueueing->isChecked());
  // End Queueing system preferences
  // Web UI
  pref->setWebUiEnabled(isWebUiEnabled());
  if (isWebUiEnabled())
  {
    pref->setWebUiPort(webUiPort());
    pref->setUPnPForWebUIPort(checkWebUIUPnP->isChecked());
    pref->setWebUiHttpsEnabled(checkWebUiHttps->isChecked());
    if (checkWebUiHttps->isChecked())
    {
      pref->setWebUiHttpsCertificate(m_sslCert);
      pref->setWebUiHttpsKey(m_sslKey);
    }
    pref->setWebUiUsername(webUiUsername());
    // FIXME: Check that the password is valid (not empty at least)
    pref->setWebUiPassword(webUiPassword());
    pref->setWebUiLocalAuthEnabled(!checkBypassLocalAuth->isChecked());
    // DynDNS
    pref->setDynDNSEnabled(checkDynDNS->isChecked());
    pref->setDynDNSService(comboDNSService->currentIndex());
    pref->setDynDomainName(domainNameTxt->text());
    pref->setDynDNSUsername(DNSUsernameTxt->text());
    pref->setDynDNSPassword(DNSPasswordTxt->text());
  }
  // End Web UI
  // End preferences
  // Save advanced settings
  advancedSettings->saveAdvancedSettings();
  // Assume that user changed multiple settings
  // so it's best to save immediately
  pref->apply();
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
  const Preferences* const pref = Preferences::instance();
  setLocale(pref->getLocale());
  checkAltRowColors->setChecked(pref->useAlternatingRowColors());

  checkShowSplash->setChecked(!pref->isSplashScreenDisabled());
  checkStartMinimized->setChecked(pref->startMinimized());
  checkProgramExitConfirm->setChecked(pref->confirmOnExit());

  checkShowSystray->setChecked(pref->systrayIntegration());
  if (checkShowSystray->isChecked()) {
    checkMinimizeToSysTray->setChecked(pref->minimizeToTray());
    checkCloseToSystray->setChecked(pref->closeToTray());
    comboTrayIcon->setCurrentIndex(pref->trayIconStyle());
  }

  checkPreventFromSuspend->setChecked(pref->preventFromSuspend());

#ifdef Q_OS_WIN
  checkStartup->setChecked(pref->WinStartup());
  // Windows: file association settings
  checkAssociateTorrents->setChecked(Preferences::isTorrentFileAssocSet());
  checkAssociateMagnetLinks->setChecked(Preferences::isMagnetLinkAssocSet());
#endif
  // End General preferences
  // Downloads preferences
  textSavePath->setText(Utils::Fs::toNativePath(pref->getSavePath()));
  if (pref->isTempPathEnabled()) {
    // enable
    checkTempFolder->setChecked(true);
  } else {
    checkTempFolder->setChecked(false);
  }
  textTempPath->setText(Utils::Fs::toNativePath(pref->getTempPath()));
  checkAppendLabel->setChecked(pref->appendTorrentLabel());
  checkAppendqB->setChecked(pref->useIncompleteFilesExtension());
  checkPreallocateAll->setChecked(pref->preAllocateAllFiles());
  checkAdditionDialog->setChecked(pref->useAdditionDialog());
  checkAdditionDialogFront->setChecked(pref->additionDialogFront());
  checkStartPaused->setChecked(pref->addTorrentsInPause());

  strValue = Utils::Fs::toNativePath(pref->getTorrentExportDir());
  if (strValue.isEmpty()) {
    // Disable
    checkExportDir->setChecked(false);
  } else {
    // enable
    checkExportDir->setChecked(true);
    textExportDir->setText(strValue);
  }

  strValue = Utils::Fs::toNativePath(pref->getFinishedTorrentExportDir());
  if (strValue.isEmpty()) {
    // Disable
    checkExportDirFin->setChecked(false);
  } else {
    // enable
    checkExportDirFin->setChecked(true);
    textExportDirFin->setText(strValue);
  }
  groupMailNotification->setChecked(pref->isMailNotificationEnabled());
  dest_email_txt->setText(pref->getMailNotificationEmail());
  smtp_server_txt->setText(pref->getMailNotificationSMTP());
  checkSmtpSSL->setChecked(pref->getMailNotificationSMTPSSL());
  groupMailNotifAuth->setChecked(pref->getMailNotificationSMTPAuth());
  mailNotifUsername->setText(pref->getMailNotificationSMTPUsername());
  mailNotifPassword->setText(pref->getMailNotificationSMTPPassword());
  autoRunBox->setChecked(pref->isAutoRunEnabled());
  autoRun_txt->setText(pref->getAutoRunProgram());
  intValue = pref->getActionOnDblClOnTorrentDl();
  if (intValue >= actionTorrentDlOnDblClBox->count())
    intValue = 0;
  actionTorrentDlOnDblClBox->setCurrentIndex(intValue);
  intValue = pref->getActionOnDblClOnTorrentFn();
  if (intValue >= actionTorrentFnOnDblClBox->count())
    intValue = 1;
  actionTorrentFnOnDblClBox->setCurrentIndex(intValue);
  // End Downloads preferences
  // Connection preferences
  spinPort->setValue(pref->getSessionPort());
  checkUPnP->setChecked(pref->isUPnPEnabled());
  checkRandomPort->setChecked(pref->useRandomPort());
  spinPort->setDisabled(checkRandomPort->isChecked());
  intValue = pref->getGlobalDownloadLimit();
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
  intValue = pref->getGlobalUploadLimit();
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

  intValue = pref->getAltGlobalDownloadLimit();
  if (intValue > 0) {
    // Enabled
    checkDownloadLimitAlt->setChecked(true);
    spinDownloadLimitAlt->setEnabled(true);
    spinDownloadLimitAlt->setValue(intValue);
  } else {
    // Disabled
    checkDownloadLimitAlt->setChecked(false);
    spinDownloadLimitAlt->setEnabled(false);
  }
  intValue = pref->getAltGlobalUploadLimit();
  if (intValue != -1) {
    // Enabled
    checkUploadLimitAlt->setChecked(true);
    spinUploadLimitAlt->setEnabled(true);
    spinUploadLimitAlt->setValue(intValue);
  } else {
    // Disabled
    checkUploadLimitAlt->setChecked(false);
    spinUploadLimitAlt->setEnabled(false);
  }
  // Options
  checkuTP->setChecked(pref->isuTPEnabled());
  checkLimituTPConnections->setChecked(pref->isuTPRateLimited());
  checkLimitTransportOverhead->setChecked(pref->includeOverheadInLimits());
  // Scheduler
  check_schedule->setChecked(pref->isSchedulerEnabled());
  schedule_from->setTime(pref->getSchedulerStartTime());
  schedule_to->setTime(pref->getSchedulerEndTime());
  schedule_days->setCurrentIndex((int)pref->getSchedulerDays());

  intValue = pref->getProxyType();
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
  textProxyIP->setText(pref->getProxyIp());
  spinProxyPort->setValue(pref->getProxyPort());
  checkProxyPeerConnecs->setChecked(pref->proxyPeerConnections());
#if LIBTORRENT_VERSION_NUM >= 10000
  checkForceProxy->setChecked(pref->getForceProxy());
#endif
  checkProxyAuth->setChecked(pref->isProxyAuthEnabled());
  textProxyUsername->setText(pref->getProxyUsername());
  textProxyPassword->setText(pref->getProxyPassword());
  //}
  // End Connection preferences
  // Bittorrent preferences
  intValue = pref->getMaxConnecs();
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
  intValue = pref->getMaxConnecsPerTorrent();
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
  intValue = pref->getMaxUploads();
  if (intValue > 0) {
    // enable
    checkMaxUploads->setChecked(true);
    spinMaxUploads->setEnabled(true);
    spinMaxUploads->setValue(intValue);
  } else {
    // disable
    checkMaxUploads->setChecked(false);
    spinMaxUploads->setEnabled(false);
  }
  intValue = pref->getMaxUploadsPerTorrent();
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
  checkDHT->setChecked(pref->isDHTEnabled());
  checkPeX->setChecked(pref->isPeXEnabled());
  checkLSD->setChecked(pref->isLSDEnabled());
  comboEncryption->setCurrentIndex(pref->getEncryptionSetting());
  checkAnonymousMode->setChecked(pref->isAnonymousModeEnabled());
  // Ratio limit
  floatValue = pref->getGlobalMaxRatio();
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
  comboRatioLimitAct->setCurrentIndex(pref->getMaxRatioAction());
  // End Bittorrent preferences
  // Misc preferences
  // * IP Filter
  checkIPFilter->setChecked(pref->isFilteringEnabled());
  checkIpFilterTrackers->setChecked(pref->isFilteringTrackerEnabled());
  textFilterPath->setText(Utils::Fs::toNativePath(pref->getFilter()));
  // End IP Filter
  // Queueing system preferences
  checkEnableQueueing->setChecked(pref->isQueueingSystemEnabled());
  spinMaxActiveDownloads->setValue(pref->getMaxActiveDownloads());
  spinMaxActiveUploads->setValue(pref->getMaxActiveUploads());
  spinMaxActiveTorrents->setValue(pref->getMaxActiveTorrents());
  checkIgnoreSlowTorrentsForQueueing->setChecked(pref->ignoreSlowTorrentsForQueueing());
  // End Queueing system preferences
  // Web UI
  checkWebUi->setChecked(pref->isWebUiEnabled());
  spinWebUiPort->setValue(pref->getWebUiPort());
  checkWebUIUPnP->setChecked(pref->useUPnPForWebUIPort());
  checkWebUiHttps->setChecked(pref->isWebUiHttpsEnabled());
  setSslCertificate(pref->getWebUiHttpsCertificate(), false);
  setSslKey(pref->getWebUiHttpsKey(), false);
  textWebUiUsername->setText(pref->getWebUiUsername());
  textWebUiPassword->setText(pref->getWebUiPassword());
  checkBypassLocalAuth->setChecked(!pref->isWebUiLocalAuthEnabled());
  // Dynamic DNS
  checkDynDNS->setChecked(pref->isDynDNSEnabled());
  comboDNSService->setCurrentIndex((int)pref->getDynDNSService());
  domainNameTxt->setText(pref->getDynDomainName());
  DNSUsernameTxt->setText(pref->getDynDNSUsername());
  DNSPasswordTxt->setText(pref->getDynDNSPassword());
  // End Web UI
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

// Return alternate Download & Upload limits in kbps
// [download,upload]
QPair<int,int> options_imp::getAltGlobalBandwidthLimits() const {
  int DL = -1, UP = -1;
  if (checkDownloadLimitAlt->isChecked()) {
    DL = spinDownloadLimitAlt->value();
  }
  if (checkUploadLimitAlt->isChecked()) {
    UP = spinUploadLimitAlt->value();
  }
  return qMakePair(DL, UP);
}

bool options_imp::startMinimized() const {
  return checkStartMinimized->isChecked();
}

bool options_imp::systrayIntegration() const {
  if (!QSystemTrayIcon::isSystemTrayAvailable()) return false;
  return checkShowSystray->isChecked();
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
    QString save_path = Preferences::instance()->getSavePath();
    textSavePath->setText(Utils::Fs::toNativePath(save_path));
  }
  return Utils::Fs::expandPathAbs(textSavePath->text());
}

QString options_imp::getTempPath() const {
  return Utils::Fs::expandPathAbs(textTempPath->text());
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

int options_imp::getMaxUploads() const {
  if (!checkMaxUploads->isChecked()) {
    return -1;
  }else{
    return spinMaxUploads->value();
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
    if (!schedTimesOk()) {
      tabSelection->setCurrentRow(TAB_SPEED);
      return;
    }
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
    if (!schedTimesOk()) {
      tabSelection->setCurrentRow(TAB_SPEED);
      return;
    }
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
#if LIBTORRENT_VERSION_NUM >= 10000
    checkForceProxy->setEnabled(true);
#endif
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
#if LIBTORRENT_VERSION_NUM >= 10000
    checkForceProxy->setEnabled(false);
#endif
    checkProxyAuth->setEnabled(false);
    checkProxyAuth->setChecked(false);
  }
}

bool options_imp::isSlashScreenDisabled() const {
  return !checkShowSplash->isChecked();
}

#ifdef Q_OS_WIN
bool options_imp::WinStartup() const {
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
  QString name = locale.name();
  // Attempt to find exact match
  int index = comboI18n->findData(name, Qt::UserRole);
  if (index < 0 ) {
      //Attempt to find a language match without a country
      int pos = name.indexOf('_');
      if (pos > -1) {
          QString lang = name.left(pos);
          index = comboI18n->findData(lang, Qt::UserRole);
      }
  }
  if (index < 0) {
    // Unrecognized, use US English
    index = comboI18n->findData(QLocale("en").name(), Qt::UserRole);
    Q_ASSERT(index >= 0);
  }
  comboI18n->setCurrentIndex(index);
}

QString options_imp::getTorrentExportDir() const {
  if (checkExportDir->isChecked())
    return Utils::Fs::expandPathAbs(textExportDir->text());
  return QString();
}

QString options_imp::getFinishedTorrentExportDir() const {
  if (checkExportDirFin->isChecked())
    return Utils::Fs::expandPathAbs(textExportDirFin->text());
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
  Preferences* const pref = Preferences::instance();
  const QString dir = QFileDialog::getExistingDirectory(this, tr("Add directory to scan"),
                                                        Utils::Fs::toNativePath(Utils::Fs::folderName(pref->getScanDirsLastPath())));
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
      pref->setScanDirsLastPath(dir);
      addedScanDirs << dir;
      scanFoldersView->resizeColumnsToContents();
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
  QDir currentExportDir(Utils::Fs::expandPathAbs(currentExportPath));
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
    textExportDir->setText(Utils::Fs::toNativePath(newExportDir));
}

void options_imp::on_browseExportDirFinButton_clicked() {
  const QString newExportDir = askForExportDir(textExportDirFin->text());
  if (!newExportDir.isNull())
    textExportDirFin->setText(Utils::Fs::toNativePath(newExportDir));
}

void options_imp::on_browseFilterButton_clicked() {
  const QString filter_path = Utils::Fs::expandPathAbs(textFilterPath->text());
  QDir filterDir(filter_path);
  QString ipfilter;
  if (!filter_path.isEmpty() && filterDir.exists()) {
    ipfilter = QFileDialog::getOpenFileName(this, tr("Choose an IP filter file"), filterDir.absolutePath(), tr("Filters")+QString(" (*.dat *.p2p *.p2b)"));
  } else {
    ipfilter = QFileDialog::getOpenFileName(this, tr("Choose an IP filter file"), QDir::homePath(), tr("Filters")+QString(" (*.dat *.p2p *.p2b)"));
  }
  if (!ipfilter.isNull())
    textFilterPath->setText(Utils::Fs::toNativePath(ipfilter));
}

// Display dialog to choose save dir
void options_imp::on_browseSaveDirButton_clicked() {
  const QString save_path = Utils::Fs::expandPathAbs(textSavePath->text());
  QDir saveDir(save_path);
  QString dir;
  if (!save_path.isEmpty() && saveDir.exists()) {
    dir = QFileDialog::getExistingDirectory(this, tr("Choose a save directory"), saveDir.absolutePath());
  } else {
    dir = QFileDialog::getExistingDirectory(this, tr("Choose a save directory"), QDir::homePath());
  }
  if (!dir.isNull())
    textSavePath->setText(Utils::Fs::toNativePath(dir));
}

void options_imp::on_browseTempDirButton_clicked() {
  const QString temp_path = Utils::Fs::expandPathAbs(textTempPath->text());
  QDir tempDir(temp_path);
  QString dir;
  if (!temp_path.isEmpty() && tempDir.exists()) {
    dir = QFileDialog::getExistingDirectory(this, tr("Choose a save directory"), tempDir.absolutePath());
  } else {
    dir = QFileDialog::getExistingDirectory(this, tr("Choose a save directory"), QDir::homePath());
  }
  if (!dir.isNull())
    textTempPath->setText(Utils::Fs::toNativePath(dir));
}

// Return Filter object to apply to BT session
QString options_imp::getFilter() const {
  return Utils::Fs::fromNativePath(textFilterPath->text());
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
  tabSelection->setCurrentRow(TAB_CONNECTION);
}

void options_imp::on_btnWebUiCrt_clicked() {
  QString filename = QFileDialog::getOpenFileName(this, QString(), QString(), tr("SSL Certificate")+QString(" (*.crt *.pem)"));
  if (filename.isNull())
    return;
  QFile file(filename);
  if (file.open(QIODevice::ReadOnly)) {
    setSslCertificate(file.readAll());
    file.close();
  }
}

void options_imp::on_btnWebUiKey_clicked() {
  QString filename = QFileDialog::getOpenFileName(this, QString(), QString(), tr("SSL Key")+QString(" (*.key *.pem)"));
  if (filename.isNull())
    return;
  QFile file(filename);
  if (file.open(QIODevice::ReadOnly)) {
    setSslKey(file.readAll());
    file.close();
  }
}

void options_imp::on_registerDNSBtn_clicked() {
  QDesktopServices::openUrl(Net::DNSUpdater::getRegistrationUrl(comboDNSService->currentIndex()));
}

void options_imp::on_IpFilterRefreshBtn_clicked() {
  if (m_refreshingIpFilter) return;
  m_refreshingIpFilter = true;
  // Updating program preferences
  Preferences* const pref = Preferences::instance();
  pref->setFilteringEnabled(true);
  pref->setFilter(getFilter());
  // Force refresh
  connect(BitTorrent::Session::instance(), SIGNAL(ipFilterParsed(bool, int)), SLOT(handleIPFilterParsed(bool, int)));
  setCursor(QCursor(Qt::WaitCursor));
  BitTorrent::Session::instance()->enableIPFilter(getFilter(), true);
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
  disconnect(BitTorrent::Session::instance(), SIGNAL(ipFilterParsed(bool, int)), this, SLOT(handleIPFilterParsed(bool, int)));
}

QString options_imp::languageToLocalizedString(const QLocale &locale)
{
  switch(locale.language()) {
  case QLocale::English: {
    if (locale.country() == QLocale::Australia)
      return QString::fromUtf8(C_LOCALE_ENGLISH_AUSTRALIA);
    else if (locale.country() == QLocale::UnitedKingdom)
      return QString::fromUtf8(C_LOCALE_ENGLISH_UNITEDKINGDOM);
    return QString::fromUtf8(C_LOCALE_ENGLISH);
  }
  case QLocale::French: return QString::fromUtf8(C_LOCALE_FRENCH);
  case QLocale::German: return QString::fromUtf8(C_LOCALE_GERMAN);
  case QLocale::Hungarian: return QString::fromUtf8(C_LOCALE_HUNGARIAN);
  case QLocale::Indonesian: return QString::fromUtf8(C_LOCALE_INDONESIAN);
  case QLocale::Italian: return QString::fromUtf8(C_LOCALE_ITALIAN);
  case QLocale::Dutch: return QString::fromUtf8(C_LOCALE_DUTCH);
  case QLocale::Spanish: return QString::fromUtf8(C_LOCALE_SPANISH);
  case QLocale::Catalan: return QString::fromUtf8(C_LOCALE_CATALAN);
  case QLocale::Galician: return QString::fromUtf8(C_LOCALE_GALICIAN);
  case QLocale::Portuguese: {
    if (locale.country() == QLocale::Brazil)
      return QString::fromUtf8(C_LOCALE_PORTUGUESE_BRAZIL);
    return QString::fromUtf8(C_LOCALE_PORTUGUESE);
  }
  case QLocale::Polish: return QString::fromUtf8(C_LOCALE_POLISH);
  case QLocale::Lithuanian: return QString::fromUtf8(C_LOCALE_LITHUANIAN);
  case QLocale::Czech: return QString::fromUtf8(C_LOCALE_CZECH);
  case QLocale::Slovak: return QString::fromUtf8(C_LOCALE_SLOVAK);
  case QLocale::Serbian: return QString::fromUtf8(C_LOCALE_SERBIAN);
  case QLocale::Croatian: return QString::fromUtf8(C_LOCALE_CROATIAN);
  case QLocale::Armenian: return QString::fromUtf8(C_LOCALE_ARMENIAN);
  case QLocale::Romanian: return QString::fromUtf8(C_LOCALE_ROMANIAN);
  case QLocale::Turkish: return QString::fromUtf8(C_LOCALE_TURKISH);
  case QLocale::Greek: return QString::fromUtf8(C_LOCALE_GREEK);
  case QLocale::Swedish: return QString::fromUtf8(C_LOCALE_SWEDISH);
  case QLocale::Finnish: return QString::fromUtf8(C_LOCALE_FINNISH);
  case QLocale::Norwegian: return QString::fromUtf8(C_LOCALE_NORWEGIAN);
  case QLocale::Danish: return QString::fromUtf8(C_LOCALE_DANISH);
  case QLocale::Bulgarian: return QString::fromUtf8(C_LOCALE_BULGARIAN);
  case QLocale::Ukrainian: return QString::fromUtf8(C_LOCALE_UKRAINIAN);
  case QLocale::Russian: return QString::fromUtf8(C_LOCALE_RUSSIAN);
  case QLocale::Japanese: return QString::fromUtf8(C_LOCALE_JAPANESE);
  case QLocale::Hebrew: return QString::fromUtf8(C_LOCALE_HEBREW);
  case QLocale::Hindi: return QString::fromUtf8(C_LOCALE_HINDI);
  case QLocale::Arabic: return QString::fromUtf8(C_LOCALE_ARABIC);
  case QLocale::Georgian: return QString::fromUtf8(C_LOCALE_GEORGIAN);
  case QLocale::Byelorussian: return QString::fromUtf8(C_LOCALE_BYELORUSSIAN);
  case QLocale::Basque: return QString::fromUtf8(C_LOCALE_BASQUE);
  case QLocale::Vietnamese: return QString::fromUtf8(C_LOCALE_VIETNAMESE);
  case QLocale::Chinese: {
      switch(locale.country()) {
      case QLocale::China:
          return QString::fromUtf8(C_LOCALE_CHINESE_SIMPLIFIED);
      case QLocale::HongKong:
          return QString::fromUtf8(C_LOCALE_CHINESE_TRADITIONAL_HK);
      default:
          return QString::fromUtf8(C_LOCALE_CHINESE_TRADITIONAL_TW);

      }
  }
  case QLocale::Korean: return QString::fromUtf8(C_LOCALE_KOREAN);
  default: {
    // Fallback to English
    const QString eng_lang = QLocale::languageToString(locale.language());
    qWarning() << "Unrecognized language name: " << eng_lang;
    return eng_lang;
  }
  }
}

void options_imp::setSslKey(const QByteArray &key, bool interactive)
{
#ifndef QT_NO_OPENSSL
  if (!key.isEmpty() && !QSslKey(key, QSsl::Rsa).isNull()) {
    lblSslKeyStatus->setPixmap(QPixmap(":/icons/oxygen/security-high.png").scaledToHeight(20, Qt::SmoothTransformation));
    m_sslKey = key;
  } else {
    lblSslKeyStatus->setPixmap(QPixmap(":/icons/oxygen/security-low.png").scaledToHeight(20, Qt::SmoothTransformation));
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
    lblSslCertStatus->setPixmap(QPixmap(":/icons/oxygen/security-high.png").scaledToHeight(20, Qt::SmoothTransformation));
    m_sslCert = cert;
  } else {
    lblSslCertStatus->setPixmap(QPixmap(":/icons/oxygen/security-low.png").scaledToHeight(20, Qt::SmoothTransformation));
    m_sslCert.clear();
    if (interactive)
      QMessageBox::warning(this, tr("Invalid certificate"), tr("This is not a valid SSL certificate."));
  }
#endif
}

bool options_imp::schedTimesOk() {
  QString msg;

  if (schedule_from->time() == schedule_to->time())
    msg = tr("The start time and the end time can't be the same.");

  if (!msg.isEmpty()) {
    QMessageBox::critical(this, tr("Time Error"), msg);
    return false;
  }

  return true;
}
