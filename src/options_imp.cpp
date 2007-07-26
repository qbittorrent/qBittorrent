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
 * Contact : chris@qbittorrent.org
 */
#include <QTextStream>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QSystemTrayIcon>
#include <QApplication>
#include <QSettings>
#include <QPlastiqueStyle>
#include <QCleanlooksStyle>
#include <QMotifStyle>
#include <QCDEStyle>
#include <QDialogButtonBox>
#include <QCloseEvent>
#ifdef Q_WS_WIN
  #include <QWindowsXPStyle>
#endif

#ifdef Q_WS_MAC
  #include <QMacStyle>
#endif

#include "options_imp.h"
#include "misc.h"

// Constructor
options_imp::options_imp(QWidget *parent):QDialog(parent){
  qDebug("-> Constructing Options");
  QString savePath;
  setupUi(this);
  // Get apply button in button box
  QList<QAbstractButton *> buttons = buttonBox->buttons();
  QAbstractButton *button;
  foreach(button, buttons){
    if(buttonBox->buttonRole(button) == QDialogButtonBox::ApplyRole){
      applyButton = button;
      break;
    }
  }
  connect(buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(applySettings(QAbstractButton*)));
  // Setting icons
  tabOptions->setTabIcon(0, QIcon(QString::fromUtf8(":/Icons/connection.png")));
  tabOptions->setTabIcon(1, QIcon(QString::fromUtf8(":/Icons/style.png")));
  tabOptions->setTabIcon(2, QIcon(QString::fromUtf8(":/Icons/locale.png")));
  tabOptions->setTabIcon(3, QIcon(QString::fromUtf8(":/Icons/filter.png")));
  tabOptions->setTabIcon(4, QIcon(QString::fromUtf8(":/Icons/proxy.png")));
  tabOptions->setTabIcon(5, QIcon(QString::fromUtf8(":/Icons/wizard.png")));
  lbl_icon_i18n->setPixmap(QPixmap(QString::fromUtf8(":/Icons/locale.png")));
  lblencryption_icon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/password.png")));
  addFilterRange->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/add.png")));
  delFilterRange->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/remove.png")));
  enableProxyAuth_checkBox->setIcon(QIcon(QString::fromUtf8(":/Icons/encrypted.png")));
  to_range->setText(tr("to", "<min port> to <max port>"));
#ifdef Q_WS_WIN
  radioWinXPStyle->setEnabled(true);
#endif
#ifdef Q_WS_MAC
  radioMacOSStyle->setEnabled(true);
#endif
  // Languages supported
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/united_kingdom.png"))), QString::fromUtf8("English"));
  locales << "en_GB";
  combo_i18n->setCurrentIndex(0);
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/france.png"))), QString::fromUtf8("Français"));
  locales << "fr_FR";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/germany.png"))), QString::fromUtf8("Deutsch"));
  locales << "de_DE";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/hungary.png"))), QString::fromUtf8("Magyar"));
  locales << "hu_HU";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/italy.png"))), QString::fromUtf8("Italiano"));
  locales << "it_IT";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/netherlands.png"))), QString::fromUtf8("Nederlands"));
  locales << "nl_NL";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/spain.png"))), QString::fromUtf8("Español"));
  locales << "es_ES";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/spain_catalunya.png"))), QString::fromUtf8("Català"));
  locales << "ca_ES";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/portugal.png"))), QString::fromUtf8("Português"));
  locales << "pt_PT";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/poland.png"))), QString::fromUtf8("Polski"));
  locales << "pl_PL";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/slovakia.png"))), QString::fromUtf8("Slovenčina"));
  locales << "sk_SK";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/romania.png"))), QString::fromUtf8("Română"));
  locales << "ro_RO";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/turkey.png"))), QString::fromUtf8("Türkçe"));
  locales << "tr_TR";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/greece.png"))), QString::fromUtf8("Ελληνικά"));
  locales << "el_GR";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/sweden.png"))), QString::fromUtf8("Svenska"));
  locales << "sv_SE";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/finland.png"))), QString::fromUtf8("Suomi"));
  locales << "fi_FI";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/norway.png"))), QString::fromUtf8("Norsk"));
  locales << "nb_NO";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/denmark.png"))), QString::fromUtf8("Dansk"));
  locales << "da_DK";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/bulgaria.png"))), QString::fromUtf8("Български"));
  locales << "bg_BG";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/ukraine.png"))), QString::fromUtf8("Українська"));
  locales << "uk_UA";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/russia.png"))), QString::fromUtf8("Русский"));
  locales << "ru_RU";
	combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/japan.png"))), QString::fromUtf8("日本語"));
	locales << "ja_JP";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/china.png"))), QString::fromUtf8("中文 (简体)"));
  locales << "zh_CN";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/china_hong_kong.png"))), QString::fromUtf8("中文 (繁體)"));
  locales << "zh_HK";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/south_korea.png"))), QString::fromUtf8("한글"));
  locales << "ko_KR";

  QString home = QDir::homePath();
  if(home[home.length()-1] != QDir::separator()){
    home += QDir::separator();
  }
  txt_savePath->setText(home+"qBT_dir");
  // Load options
  loadOptions();
  // Disable systray integration if it is not supported by the system
  if(!QSystemTrayIcon::isSystemTrayAvailable()){
    systrayDisabled(true);
    check_disableSystray->setEnabled(false);
  }
  // Connect signals / slots
  connect(disableUPLimit, SIGNAL(stateChanged(int)), this, SLOT(disableUpload(int)));
  connect(disableDLLimit,  SIGNAL(stateChanged(int)), this, SLOT(disableDownload(int)));
  connect(disableDHT,  SIGNAL(stateChanged(int)), this, SLOT(disableDHTGroup(int)));
  connect(check_disableSystray, SIGNAL(stateChanged(int)), this, SLOT(systrayDisabled(int)));
  connect(disableRatio,  SIGNAL(stateChanged(int)), this, SLOT(disableShareRatio(int)));
  connect(activateFilter,  SIGNAL(stateChanged(int)), this, SLOT(enableFilter(int)));
  connect(enableProxy_checkBox,  SIGNAL(stateChanged(int)), this, SLOT(enableProxy(int)));
  connect(enableProxyAuth_checkBox,  SIGNAL(stateChanged(int)), this, SLOT(enableProxyAuth(int)));
  connect(enableScan_checkBox, SIGNAL(stateChanged(int)), this, SLOT(enableDirScan(int)));
  connect(disableMaxConnec, SIGNAL(stateChanged(int)), this, SLOT(disableMaxConnecLimit(int)));
  connect(checkAdditionDialog, SIGNAL(stateChanged(int)), this, SLOT(enableSavePath(int)));
  // Apply button is activated when a value is changed
  // Main
  connect(spin_download, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spin_upload, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spin_port_min, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spin_port_max, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spin_max_connec, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(spin_ratio, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(disableUPLimit, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(disableDLLimit, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(disableRatio, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(scanDir, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(enableScan_checkBox, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(disableMaxConnec, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(disableDHT, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(disablePeX, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(comboEncryption, SIGNAL(currentIndexChanged(int)), this, SLOT(enableApplyButton()));
  // Language
  connect(combo_i18n, SIGNAL(currentIndexChanged(int)), this, SLOT(enableApplyButton()));
  // IPFilter
  connect(activateFilter, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(filterFile, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  // Proxy
  connect(enableProxyAuth_checkBox, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(enableProxy_checkBox, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(proxy_port, SIGNAL(valueChanged(QString)), this, SLOT(enableApplyButton()));
  connect(proxy_ip, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(proxy_username, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(proxy_password, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(comboProxyType, SIGNAL(currentIndexChanged(int)), this, SLOT(enableApplyButton()));
  connect(checkProxyTrackers, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(checkProxyPeers, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(checkProxyWebseeds, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(checkProxyDHT, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  // Misc Settings
  connect(checkAdditionDialog, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(txt_savePath, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(check_goToSysTray, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(check_closeToSysTray, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(confirmExit_checkBox, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(preview_program, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(alwaysOSD, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(someOSD, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(neverOSD, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(radioPlastiqueStyle, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(radioCleanlooksStyle, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(radioMotifStyle, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(radioCDEStyle, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(check_disableSystray, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
#ifdef Q_WS_WIN
  connect(radioWinXPStyle, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
#endif
#ifdef Q_WS_MAC
  connect(radioMacOSStyle, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
#endif
  // Disable apply Button
  applyButton->setEnabled(false);
  if(!QSystemTrayIcon::supportsMessages()){
    // Mac OS X doesn't support it yet
    neverOSD->setChecked(true);
    groupOSD->setEnabled(false);
  }
}

// Main destructor
options_imp::~options_imp(){
  qDebug("-> destructing Options");
}

void options_imp::useStyle(){
  QString style = getStyle();
  if(style == "Cleanlooks"){
    QApplication::setStyle(new QCleanlooksStyle());
    return;
  }
  if(style == "Motif"){
    QApplication::setStyle(new QMotifStyle());
    return;
  }
  if(style == "CDE"){
    QApplication::setStyle(new QCDEStyle());
    return;
  }
#ifdef Q_WS_MAC
  if(style == "MacOS"){
    QApplication::setStyle(new QMacStyle());
    return;
  }
#endif
#ifdef Q_WS_WIN
  if(style == "WinXP"){
    QApplication::setStyle(new QWindowsXPStyle());
    return;
  }
#endif
  QApplication::setStyle(new QPlastiqueStyle());
}

void options_imp::saveOptions(){
  applyButton->setEnabled(false);
  QSettings settings("qBittorrent", "qBittorrent");
  // Apply style
  useStyle();
  // Check if min port < max port
  checkPortsLogic();
  settings.beginGroup("Options");
  // Main options
  settings.beginGroup("Main");
  settings.setValue("DLLimit", getLimits().first);
  settings.setValue("UPLimit", getLimits().second);
  settings.setValue("MaxConnecs", getMaxConnec());
  settings.setValue("PortRangeMin", getPorts().first);
  settings.setValue("PortRangeMax", getPorts().second);
  settings.setValue("ShareRatio", getRatio());
  settings.setValue("EncryptionState", getEncryptionSetting());
  settings.setValue("PeXState", !isPeXDisabled());
  settings.setValue("DHTPort", getDHTPort());
  settings.setValue("ScanDir", getScanDir());
  // End Main options
  settings.endGroup();
  // Language options
  settings.beginGroup("Language");
  settings.setValue("Locale", getLocale());
  // End Language options
  settings.endGroup();
  // IPFilter options
  settings.beginGroup("IPFilter");
  bool enabled = isFilteringEnabled();
  settings.setValue("Enabled", enabled);
  if(enabled){
    settings.setValue("File", filterFile->text());
  }
  // End IPFilter options
  settings.endGroup();
  // Proxy options
  settings.beginGroup("Proxy");
  enabled = isProxyEnabled();
  settings.setValue("Enabled", enabled);
  if(enabled){
    settings.setValue("IP", getProxyIp());
    settings.setValue("Port", getProxyPort());
    unsigned short val = getProxyType();
    if(val == HTTP || val == HTTP_PW){
      settings.setValue("ProxyType", HTTP);
    }else{
      settings.setValue("ProxyType", SOCKS5);
    }
    settings.setValue("UseProxyForTrackers", useProxyForTrackers());
    settings.setValue("UseProxyForPeers", useProxyForPeers());
    settings.setValue("UseProxyForWebseeds", useProxyForWebseeds());
    settings.setValue("UseProxyForDHT", useProxyForDHT());
    enabled = isProxyAuthEnabled();
    settings.beginGroup("Authentication");
    settings.setValue("Enabled", enabled);
    if(enabled){
      settings.setValue("Username", getProxyUsername());
      settings.setValue("Password", getProxyPassword());
    }
    settings.endGroup();
  }
  // End Proxy options
  settings.endGroup();
  // Misc options
  settings.beginGroup("Misc");
  settings.beginGroup("TorrentAdditionDialog");
  enabled = useAdditionDialog();
  settings.setValue("Enabled", enabled);
  if(!enabled){
    settings.setValue("SavePath", getSavePath());
  }
  settings.endGroup();
  settings.beginGroup("Behaviour");
  settings.setValue("ConfirmOnExit", getConfirmOnExit());
  settings.setValue("GoToSystray", getGoToSysTrayOnMinimizingWindow());
  settings.setValue("GoToSystrayOnExit", getGoToSysTrayOnExitingWindow());
  settings.setValue("SystrayIntegration", useSystrayIntegration());
  // End Behaviour group
  settings.endGroup();
  settings.setValue("PreviewProgram", getPreviewProgram());
  // End Misc options
  settings.endGroup();
  if(getUseOSDAlways()){
    settings.setValue("OSDEnabled", 1);
  }else{
    if(getUseOSDWhenHiddenOnly()){
      settings.setValue("OSDEnabled", 2);
    }else{
      settings.setValue("OSDEnabled", 0);
    }
  }
  settings.setValue("Style", getStyle());
  // End Options group
  settings.endGroup();
}

bool options_imp::isFilteringEnabled() const{
  return activateFilter->isChecked();
}

unsigned short options_imp::getProxyType() const{
  if(comboProxyType->currentIndex() == HTTP){
    if(isProxyAuthEnabled()){
      return HTTP_PW;
    }else{
      return HTTP;
    }
  }else{
    if(isProxyAuthEnabled()){
      return SOCKS5_PW;
    }else{
      return SOCKS5;
    }
  }
}

bool options_imp::useProxyForTrackers() const{
  return checkProxyTrackers->isChecked();
}

bool options_imp::useProxyForPeers() const{
  return checkProxyPeers->isChecked();
}

bool options_imp::useProxyForWebseeds() const{
  return checkProxyWebseeds->isChecked();
}

bool options_imp::useProxyForDHT() const{
  return checkProxyDHT->isChecked();
}

QString options_imp::getStyle() const{
  if(radioPlastiqueStyle->isChecked()) return "Plastique";
  if(radioCleanlooksStyle->isChecked()) return "Cleanlooks";
  if(radioMotifStyle->isChecked()) return "Motif";
  if(radioCDEStyle->isChecked()) return "CDE";
  if(radioMacOSStyle->isChecked()) return "MacOS";
  if(radioWinXPStyle->isChecked()) return "WinXP";
#ifdef Q_WS_WIN
  return "WinXP";
#endif
#ifdef Q_WS_MAC
  return "MacOS";
#endif
  return "Plastique";
}

void options_imp::setStyle(QString style){
  if(style == "Cleanlooks"){
    radioCleanlooksStyle->setChecked(true);
    return;
  }
  if(style == "Motif"){
    radioMotifStyle->setChecked(true);
    return;
  }
  if(style == "CDE"){
    radioCDEStyle->setChecked(true);
    return;
  }
  if(style == "MacOS"){
    radioMacOSStyle->setChecked(true);
    return;
  }
  if(style == "WinXP"){
    radioWinXPStyle->setChecked(true);
    return;
  }
  radioPlastiqueStyle->setChecked(true);
}

void options_imp::loadOptions(){
  int value;
  float floatValue;
  bool boolValue;
  QString strValue;
  QSettings settings("qBittorrent", "qBittorrent");
  // Check if min port < max port
  checkPortsLogic();
  settings.beginGroup("Options");
  // Main options
  settings.beginGroup("Main");
  value = settings.value("DLLimit", -1).toInt();
  if(value < 0){
    disableDLLimit->setChecked(true);
    spin_download->setEnabled(false);
  }else{
    disableDLLimit->setChecked(false);
    spin_download->setEnabled(true);
    spin_download->setValue(value);
  }
  value = settings.value("UPLimit", -1).toInt();
  if(value < 0){
    disableUPLimit->setChecked(true);
    spin_upload->setEnabled(false);
  }else{
    disableUPLimit->setChecked(false);
    spin_upload->setEnabled(true);
    spin_upload->setValue(value);
  }
  value = settings.value("MaxConnecs", -1).toInt();
  if(value < 0){
    disableMaxConnec->setChecked(true);
    spin_max_connec->setEnabled(false);
  }else{
    disableMaxConnec->setChecked(false);
    spin_max_connec->setEnabled(true);
    spin_max_connec->setValue(value);
  }
  spin_port_min->setValue(settings.value("PortRangeMin", 6881).toInt());
  spin_port_max->setValue(settings.value("PortRangeMax", 6889).toInt());
  floatValue = settings.value("ShareRatio", 0).toDouble();
  if(floatValue == 0){
    disableRatio->setChecked(true);
    spin_ratio->setEnabled(false);
  }else{
    disableRatio->setChecked(false);
    spin_ratio->setEnabled(true);
    spin_ratio->setValue(floatValue);
  }
  value = settings.value("DHTPort", 6881).toInt();
  if(value < 0){
    disableDHT->setChecked(true);
    groupDHT->setEnabled(false);
  }else{
    disableDHT->setChecked(false);
    groupDHT->setEnabled(true);
    if(value < 1000){
      value = 6881;
    }
    spin_dht_port->setValue(value);
  }
  value = settings.value("EncryptionState", 0).toInt();
  comboEncryption->setCurrentIndex(value);
  boolValue = settings.value("PeXState", true).toBool();
  if(boolValue){
    // Pex disabled
    disablePeX->setChecked(false);
  }else{
    // PeX enabled
    disablePeX->setChecked(true);
  }
  strValue = settings.value("ScanDir", QString()).toString();
  if(!strValue.isEmpty()){
    enableScan_checkBox->setChecked(true);
    lbl_scanDir->setEnabled(true);
    scanDir->setEnabled(true);
    browse_button_scan->setEnabled(true);
    scanDir->setText(strValue);
  }else{
    enableScan_checkBox->setChecked(false);
    lbl_scanDir->setEnabled(false);
    browse_button_scan->setEnabled(false);
    scanDir->setEnabled(false);
  }
  // End Main options
  settings.endGroup();
  // Language options
  settings.beginGroup("Language");
  strValue = settings.value("Locale", "en_GB").toString();
  setLocale(strValue);
  // End Language options
  settings.endGroup();
  // IPFilter options
  settings.beginGroup("IPFilter");
  if(settings.value("Enabled", false).toBool()){
    strValue = settings.value("File", QString()).toString();
    activateFilter->setChecked(true);
    filterGroup->setEnabled(true);
    filterFile->setText(strValue);
    processFilterFile(strValue);
  }else{
    activateFilter->setChecked(false);
    filterGroup->setEnabled(false);
  }
  // End IPFilter options
  settings.endGroup();
  // Proxy options
  settings.beginGroup("Proxy");
  if(settings.value("Enabled", false).toBool()){
    strValue = settings.value("IP", QString()).toString();
    if(strValue.isEmpty()){
      enableProxy_checkBox->setChecked(false);
      groupProxy->setEnabled(false);
    }else{
      enableProxy_checkBox->setChecked(true);
      groupProxy->setEnabled(true);
      proxy_ip->setText(strValue);
      proxy_port->setValue(settings.value("Port", 8080).toInt());
      comboProxyType->setCurrentIndex(settings.value("ProxyType", HTTP).toInt());
      checkProxyTrackers->setChecked(settings.value("useProxyForTrackers", true).toBool());
      checkProxyPeers->setChecked(settings.value("useProxyForPeers", true).toBool());
      checkProxyWebseeds->setChecked(settings.value("useProxyForWebseeds", true).toBool());
      checkProxyDHT->setChecked(settings.value("useProxyForDHT", true).toBool());
      settings.beginGroup("Authentication");
      if(settings.value("Enabled", false).toBool()){
        enableProxyAuth_checkBox->setChecked(true);
        groupProxyAuth->setEnabled(true);
        proxy_username->setText(settings.value("Username", QString()).toString());
        proxy_password->setText(settings.value("Password", QString()).toString());
      }else{
        enableProxyAuth_checkBox->setChecked(false);
        groupProxyAuth->setEnabled(false);
      }
      settings.endGroup();
    }
  }else{
    enableProxy_checkBox->setChecked(false);
    groupProxy->setEnabled(false);
  }
  // End Proxy options
  settings.endGroup();
  // Misc options
  settings.beginGroup("Misc");
  settings.beginGroup("TorrentAdditionDialog");
  if(settings.value("Enabled", true).toBool()){
    checkAdditionDialog->setChecked(true);
    groupSavePath->setEnabled(false);
  }else{
    checkAdditionDialog->setChecked(false);
    groupSavePath->setEnabled(true);
    txt_savePath->setText(settings.value("SavePath", QString()).toString());
  }
  settings.endGroup();
  settings.beginGroup("Behaviour");
  confirmExit_checkBox->setChecked(settings.value("ConfirmOnExit", true).toBool());
  check_goToSysTray->setChecked(settings.value("GoToSystray", true).toBool());
  check_closeToSysTray->setChecked(settings.value("GoToSystrayOnExit", false).toBool());
  boolValue = settings.value("SystrayIntegration", true).toBool();
  check_disableSystray->setChecked(!boolValue);
  systrayDisabled(!boolValue);
  // End Behaviour group
  settings.endGroup();
  preview_program->setText(settings.value("PreviewProgram", QString()).toString());
  // End Misc group
  settings.endGroup();
  value = settings.value("OSDEnabled", 1).toInt();
  if(value == 0){
    neverOSD->setChecked(true);
  }else{
    if(value == 2){
      someOSD->setChecked(true);
    }else{
      alwaysOSD->setChecked(true);
    }
  }
  setStyle(settings.value("Style", QString()).toString());
  // End Options group
  settings.endGroup();
}

void options_imp::systrayDisabled(int val){
  if(val){
    // No SystrayIntegration
    check_closeToSysTray->setChecked(false);
    check_closeToSysTray->setEnabled(false);
    check_goToSysTray->setChecked(false);
    check_goToSysTray->setEnabled(false);
    neverOSD->setChecked(true);
    groupOSD->setEnabled(false);
  }else{
    check_closeToSysTray->setEnabled(true);
    check_goToSysTray->setEnabled(true);
    groupOSD->setEnabled(true);
  }
}

// return min & max ports
// [min, max]
std::pair<unsigned short, unsigned short> options_imp::getPorts() const{
  return std::make_pair(this->spin_port_min->value(), this->spin_port_max->value());
}

int options_imp::getDHTPort() const{
  if(isDHTEnabled()){
    return spin_dht_port->value();
  }else{
    return -1;
  }
}

int options_imp::getEncryptionSetting() const{
  return comboEncryption->currentIndex();
}

QString options_imp::getPreviewProgram() const{
  QString preview_txt = preview_program->text();
  preview_txt.trimmed();
  return preview_txt;
}

bool options_imp::getGoToSysTrayOnMinimizingWindow() const{
  return check_goToSysTray->isChecked();
}

bool options_imp::getGoToSysTrayOnExitingWindow() const{
  return check_closeToSysTray->isChecked();
}

bool options_imp::getConfirmOnExit() const{
  return confirmExit_checkBox->isChecked();
}

bool options_imp::isDHTEnabled() const{
  return !disableDHT->isChecked();
}

bool options_imp::isPeXDisabled() const{
  return disablePeX->isChecked();
}
// Return Download & Upload limits
// [download,upload]
QPair<int,int> options_imp::getLimits() const{
  int DL = -1, UP = -1;
  if(!disableDLLimit->isChecked()){
    DL = this->spin_download->value();
  }
  if(!disableUPLimit->isChecked()){
    UP = this->spin_upload->value();
  }
  return qMakePair(DL, UP);
}

// Should the program use OSD?
bool options_imp::getUseOSDAlways() const{
  if(!QSystemTrayIcon::supportsMessages()){
    // Mac OS X doesn't support it yet
    return false;
  }
  return alwaysOSD->isChecked();
}

// Should the program use OSD when the window is hidden only?
bool options_imp::getUseOSDWhenHiddenOnly() const{
  if(!QSystemTrayIcon::supportsMessages()){
    // Mac OS X doesn't support it yet
    return false;
  }
  return someOSD->isChecked();
}

bool options_imp::useSystrayIntegration() const{
  if (!QSystemTrayIcon::isSystemTrayAvailable()) return false;
  return (!check_disableSystray->isChecked());
}

// Return Share ratio
float options_imp::getRatio() const{
  if(!disableRatio->isChecked()){
    return spin_ratio->value();
  }
  return 0;
}

// Return Save Path
QString options_imp::getSavePath() const{
  QString home = QDir::homePath();
  if(home[home.length()-1] != QDir::separator()){
    home += QDir::separator();
  }
  if(txt_savePath->text().trimmed().isEmpty()){
    txt_savePath->setText(home+"qBT_dir");
  }
  return txt_savePath->text();
}

// Return max connections number
int options_imp::getMaxConnec() const{
  if(disableMaxConnec->isChecked()){
    return -1;
  }else{
    return spin_max_connec->value();
  }
}

void options_imp::on_buttonBox_accepted(){
  if(applyButton->isEnabled()){
    saveOptions();
    applyButton->setEnabled(false);
    // set infobar text
    this->hide();
    emit status_changed(tr("Options were saved successfully."), true);
  }else{
    setAttribute(Qt::WA_DeleteOnClose);
    accept();
  }
}

void options_imp::applySettings(QAbstractButton* button) {
  if(button == applyButton){
    saveOptions();
    emit status_changed(tr("Options were saved successfully."), false);
  }
}

void options_imp::closeEvent(QCloseEvent *e){
  setAttribute(Qt::WA_DeleteOnClose);
  e->accept();
}

void options_imp::on_buttonBox_rejected(){
  setAttribute(Qt::WA_DeleteOnClose);
  reject();
}

void options_imp::disableDownload(int checkBoxValue){
  if(checkBoxValue==2){
    //Disable
    spin_download->setEnabled(false);
  }else{
    //enable
    spin_download->setEnabled(true);
  }
}

void options_imp::disableDHTGroup(int checkBoxValue){
  if(checkBoxValue==2){
    //Disable
    groupDHT->setEnabled(false);
  }else{
    //enable
    groupDHT->setEnabled(true);
  }
}

void options_imp::enableSavePath(int checkBoxValue){
 if(checkBoxValue==2){
    //enable
    groupSavePath->setEnabled(false);
  }else{
    //disable
    groupSavePath->setEnabled(true);
  }
}

bool options_imp::useAdditionDialog() const{
  return checkAdditionDialog->isChecked();
}

void options_imp::disableMaxConnecLimit(int checkBoxValue){
  if(checkBoxValue==2){
    //Disable
    spin_max_connec->setEnabled(false);
  }else{
    //enable
    spin_max_connec->setEnabled(true);
  }
}

void options_imp::enableFilter(int checkBoxValue){
  if(checkBoxValue!=2){
    //Disable
    filterGroup->setEnabled(false);
  }else{
    //enable
    filterGroup->setEnabled(true);
  }
}

void options_imp::disableUpload(int checkBoxValue){
  if(checkBoxValue==2){
    //Disable
    spin_upload->setEnabled(false);
  }else{
    //enable
    spin_upload->setEnabled(true);
  }
}

void options_imp::enableApplyButton(){
  if(!applyButton->isEnabled()){
    applyButton->setEnabled(true);
  }
}

void options_imp::disableShareRatio(int checkBoxValue){
  if(checkBoxValue==2){
    //Disable
    spin_ratio->setEnabled(false);
  }else{
    //enable
    spin_ratio->setEnabled(true);
  }
}

void options_imp::enableProxy(int checkBoxValue){
  if(checkBoxValue==2){
    //enable
    groupProxy->setEnabled(true);
  }else{
    //disable
    groupProxy->setEnabled(false);
  }
}

void options_imp::enableProxyAuth(int checkBoxValue){
  if(checkBoxValue==2){
    //enable
    groupProxyAuth->setEnabled(true);
  }else{
    //disable
    groupProxyAuth->setEnabled(false);
  }
}

void options_imp::enableDirScan(int checkBoxValue){
  if(checkBoxValue==2){
    //enable
    lbl_scanDir->setEnabled(true);
    scanDir->setEnabled(true);
    browse_button_scan->setEnabled(true);
  }else{
    //disable
    lbl_scanDir->setEnabled(false);
    scanDir->setEnabled(false);
    browse_button_scan->setEnabled(false);
  }
}

// Proxy settings
bool options_imp::isProxyEnabled() const{
  return groupProxy->isEnabled();
}

bool options_imp::isProxyAuthEnabled() const{
  return groupProxyAuth->isEnabled();
}

QString options_imp::getProxyIp() const{
  return proxy_ip->text();
}

unsigned short options_imp::getProxyPort() const{
  return proxy_port->value();
}

QString options_imp::getProxyUsername() const{
  return proxy_username->text();
}

QString options_imp::getProxyPassword() const{
  return proxy_password->text();
}

// Locale Settings
QString options_imp::getLocale() const{
  return locales.at(combo_i18n->currentIndex());
}

void options_imp::setLocale(QString locale){
  int indexLocales=locales.indexOf(QRegExp(locale));
  if(indexLocales != -1){
    combo_i18n->setCurrentIndex(indexLocales);
  }
}

// Is called before saving to check if minPort < maxPort
void options_imp::checkPortsLogic(){
  int maxValue = spin_port_max->value();
  if(spin_port_min->value() > spin_port_max->value()){
    spin_port_max->setValue(spin_port_min->value());
    spin_port_min->setValue(maxValue);
  }
}

// Return scan dir set in options
QString options_imp::getScanDir() const{
  if(scanDir->isEnabled()){
    return scanDir->text();
  }else{
    return QString();
  }
}

// Display dialog to choose scan dir
void options_imp::on_browse_button_scan_clicked(){
  QString dir = QFileDialog::getExistingDirectory(this, tr("Choose scan directory"), QDir::homePath());
  if(!dir.isNull()){
    scanDir->setText(dir);
  }
}

void options_imp::on_filterBrowse_clicked(){
  QString ipfilter = QFileDialog::getOpenFileName(this, tr("Choose an ipfilter.dat file"), QDir::homePath());
  if(!ipfilter.isNull()){
    filterFile->setText(ipfilter);
    processFilterFile(ipfilter);
  }
}

void options_imp::on_browsePreview_clicked(){
  QString program_txt = QFileDialog::getOpenFileName(this, tr("Choose your favourite preview program"), QDir::homePath());
  if(!program_txt.isNull()){
    preview_program->setText(program_txt);
  }
}

// Display dialog to choose save dir
void options_imp::on_browse_button_clicked(){
  QString dir = QFileDialog::getExistingDirectory(this, tr("Choose a save directory"), QDir::homePath());
  if(!dir.isNull()){
    txt_savePath->setText(dir);
  }
}

// look for ipfilter.dat file
// reads emule ipfilter files.
// with the following format:
//
// <first-ip> - <last-ip> , <access> , <comment>
//
// first-ip is an ip address that defines the first
// address of the range
// last-ip is the last ip address in the range
// access is a number specifying the access control
// for this ip-range. Right now values > 127 = allowed
// and numbers <= 127 = blocked
// the rest of the line is ignored
//
// Lines may be commented using '#' or '//'
void options_imp::processFilterFile(QString filePath){
  qDebug("Processing filter files");
  filtersList->clear();
  QString manualFilters= misc::qBittorrentPath() + "ipfilter.dat";
  QStringList filterFiles(manualFilters);
  filterFiles.append(filePath);
  for(int i=0; i<2; ++i){
    QFile file(filterFiles.at(i));
    QStringList IP;
    if (file.exists()){
      if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        QMessageBox::critical(0, tr("I/O Error", "Input/Output Error"), tr("Couldn't open %1 in read mode.").arg(filePath));
        continue;
      }
      unsigned int nbLine = 0;
      while (!file.atEnd()) {
        ++nbLine;
        QByteArray line = file.readLine();
        if(line.startsWith('#') || line.startsWith("//")) continue;
        // Line is not commented
        QList<QByteArray> partsList = line.split(',');
        unsigned int nbElem = partsList.size();
        if(nbElem < 2){
          qDebug("Ipfilter.dat: line %d is malformed.", nbLine);
          continue;
        }
        bool ok;
        int nbAccess = partsList.at(1).trimmed().toInt(&ok);
        if(!ok){
          qDebug("Ipfilter.dat: line %d is malformed.", nbLine);
          continue;
        }
        if(nbAccess <= 127){
          QString strComment;
          QString strStartIP = partsList.at(0).split('-').at(0).trimmed();
          QString strEndIP = partsList.at(0).split('-').at(1).trimmed();
          if(nbElem > 2){
            strComment = partsList.at(2).trimmed();
          }else{
            strComment = QString();
          }
          // Split IP

          QRegExp is_ipv6("^[0-9a-f]{4}(:[0-9a-f]{4}){7}$", Qt::CaseInsensitive, QRegExp::RegExp);
          QRegExp is_ipv4("^(([0-1]?[0-9]?[0-9])|(2[0-4][0-9])|(25[0-5]))(\\.(([0-1]?[0-9]?[0-9])|(2[0-4][0-9])|(25[0-5]))){3}$", Qt::CaseInsensitive, QRegExp::RegExp);

          if(strStartIP.contains(is_ipv4) && strEndIP.contains(is_ipv4)) {
            // IPv4
            IP = strStartIP.split('.');
            address_v4 start((IP.at(0).toInt() << 24) + (IP.at(1).toInt() << 16) + (IP.at(2).toInt() << 8) + IP.at(3).toInt());
            IP = strEndIP.split('.');
            address_v4 last((IP.at(0).toInt() << 24) + (IP.at(1).toInt() << 16) + (IP.at(2).toInt() << 8) + IP.at(3).toInt());

            // add it to list
            QStringList item(QString(start.to_string().c_str()));
            item.append(QString(last.to_string().c_str()));
            if(!i){
              item.append("Manual");
            }else{
              item.append("ipfilter.dat");
            }
            item.append(strComment);
            new QTreeWidgetItem(filtersList, item);
            // Apply to bittorrent session
            filter.add_rule(start, last, ip_filter::blocked);
          } else if(strStartIP.contains(is_ipv6) && strEndIP.contains(is_ipv6)) {
            // IPv6, ex :   1fff:0000:0a88:85a3:0000:0000:ac1f:8001
            IP = strStartIP.split(':');
            address_v6 start = address_v6::from_string(strStartIP.remove(':', 0).toUtf8().data());
            IP = strEndIP.split(':');
            address_v6 last = address_v6::from_string(strEndIP.remove(':', 0).toUtf8().data());

            // add it to list
            QStringList item(QString(start.to_string().c_str()));
            item.append(QString(last.to_string().c_str()));
            if(!i){
              item.append("Manual");
            }else{
              item.append("ipfilter.dat");
            }
            item.append(strComment);
            new QTreeWidgetItem(filtersList, item);
            // Apply to bittorrent session
            filter.add_rule(start, last, ip_filter::blocked);
          } else {
              qDebug("Ipfilter.dat: line %d is malformed.", nbLine);
              continue;
          }

        }
      }
      file.close();
    }
  }
}

// Return Filter object to apply to BT session
ip_filter options_imp::getFilter() const{
  return filter;
}

// Add an IP Range to ipFilter
void options_imp::on_addFilterRange_clicked(){
  bool ok;
  // Ask user for start ip
  QString startIP = QInputDialog::getText(this, tr("Range Start IP"),
                                       tr("Start IP:"), QLineEdit::Normal,
                                       "0.0.0.0", &ok);
  QStringList IP1 = startIP.split('.');
  // Check IP
  bool ipv4 = true;
  QRegExp is_ipv6("^[0-9a-f]{4}(:[0-9a-f]{4}){7}$", Qt::CaseInsensitive, QRegExp::RegExp);
  QRegExp is_ipv4("^(([0-1]?[0-9]?[0-9])|(2[0-4][0-9])|(25[0-5]))(\\.(([0-1]?[0-9]?[0-9])|(2[0-4][0-9])|(25[0-5]))){3}$", Qt::CaseInsensitive, QRegExp::RegExp);



  if(!ok) {
    return;
  } else if(startIP.isEmpty()
    || (!startIP.contains(is_ipv4) && !startIP.contains(is_ipv6))){
    QMessageBox::critical(0, tr("Invalid IP"), tr("This IP is invalid."));
    return;
  } else if(startIP.contains(is_ipv4)) {
    ipv4 = true;
  } else if(startIP.contains(is_ipv6)) {
    ipv4 = false;
  }

  // Ask user for last ip
  QString lastIP = QInputDialog::getText(this, tr("Range End IP"),
                                          tr("End IP:"), QLineEdit::Normal,
                                          startIP, &ok);
  // check IP
  if (!ok) {
    return;
  } else if(lastIP.isEmpty()
    || (!lastIP.contains(is_ipv4) && !lastIP.contains(is_ipv6))
    || (ipv4 == true && !lastIP.contains(is_ipv4))
    || (ipv4 == false && !lastIP.contains(is_ipv6))){
    QMessageBox::critical(0, tr("Invalid IP"), tr("This IP is invalid."));
    return;
  }

  // Ask user for Comment
  QString comment = QInputDialog::getText(this, tr("IP Range Comment"),
                                         tr("Comment:"), QLineEdit::Normal,
                                         "", &ok);
  if (!ok){
    comment = QString("");
    return;
  }
  QFile ipfilter(misc::qBittorrentPath() + "ipfilter.dat");
  if (!ipfilter.open(QIODevice::Append | QIODevice::WriteOnly | QIODevice::Text)){
    std::cerr << "Error: Couldn't write in ipfilter.dat";
    return;
  }
  QTextStream out(&ipfilter);
  out << startIP << " - " << lastIP << ", 0, " << comment << "\n";
  ipfilter.close();
  processFilterFile(filterFile->text());
  enableApplyButton();
}

// Delete selected IP range in list and ipfilter.dat file
// User can only delete IP added manually
void options_imp::on_delFilterRange_clicked(){
  bool changed = false;
  QList<QTreeWidgetItem *> selectedItems = filtersList->selectedItems();
  // Delete from list
  for(int i=0;i<selectedItems.size();++i){
    QTreeWidgetItem *item = selectedItems.at(i);
    if(item->text(2) == "Manual"){
      delete item;
      changed = true;
    }
    if(changed){
      enableApplyButton();
    }
  }
  // Update ipfilter.dat
  QFile ipfilter(misc::qBittorrentPath() + "ipfilter.dat");
  if (!ipfilter.open(QIODevice::WriteOnly | QIODevice::Text)){
    std::cerr << "Error: Couldn't write in ipfilter.dat";
    return;
  }
  QTextStream out(&ipfilter);
  for(int i=0; i<filtersList->topLevelItemCount();++i){
    QTreeWidgetItem *item = filtersList->topLevelItem(i);
    if(item->text(2) == "Manual"){
      out << item->text(0) << " - " << item->text(1) << ", 0, " << item->text(3) << "\n";
    }
  }
  ipfilter.close();
}
