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
#include <QDomDocument>

#include "options_imp.h"
#include "misc.h"

// Constructor
options_imp::options_imp(QWidget *parent):QDialog(parent){
  QString savePath;
  setupUi(this);
  // Setting icons
  tabWidget->setTabIcon(0, QIcon(QString::fromUtf8(":/Icons/systemtray.png")));
  tabWidget->setTabIcon(1, QIcon(QString::fromUtf8(":/Icons/filter.png")));
  tabWidget->setTabIcon(2, QIcon(QString::fromUtf8(":/Icons/proxy.png")));
  tabWidget->setTabIcon(3, QIcon(QString::fromUtf8(":/Icons/style.png")));
  lbl_icon_i18n->setPixmap(QPixmap(QString::fromUtf8(":/Icons/locale.png")));
  addFilterRange->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/add.png")));
  delFilterRange->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/remove.png")));
  enableProxyAuth_checkBox->setIcon(QIcon(QString::fromUtf8(":/Icons/encrypted.png")));
  to_range->setText(tr("to", "<min port> to <max port>"));
  // Languages supported
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/united_kingdom.png"))), QString::fromUtf8("English"));
  locales << "en_GB";
  combo_i18n->setCurrentIndex(0);
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/france.png"))), QString::fromUtf8("Français"));
  locales << "fr_FR";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/germany.png"))), QString::fromUtf8("Deutsch"));
  locales << "de_DE";
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
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/norway.png"))), QString::fromUtf8("Norsk"));
  locales << "nb_NO";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/bulgaria.png"))), QString::fromUtf8("Български"));
  locales << "bg_BG";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/ukraine.png"))), QString::fromUtf8("Українська"));
  locales << "uk_UA";
  combo_i18n->addItem((QIcon(QString::fromUtf8(":/Icons/flags/russia.png"))), QString::fromUtf8("Русский"));
  locales << "ru_RU";
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
  // Save Options if options.xml does not exist yet
  savePath = misc::qBittorrentPath() + "options.xml";
  if(! QFile::exists(savePath)){
    saveOptions();
  }
  if(!loadOptions()){
    std::cout << "Warning: Couldn't load options" << '\n';
  }
  // Connect signals / slots
  connect(disableUPLimit, SIGNAL(stateChanged(int)), this, SLOT(disableUpload(int)));
  connect(disableDLLimit,  SIGNAL(stateChanged(int)), this, SLOT(disableDownload(int)));
  connect(disableRatio,  SIGNAL(stateChanged(int)), this, SLOT(disableShareRatio(int)));
  connect(activateFilter,  SIGNAL(stateChanged(int)), this, SLOT(enableFilter(int)));
  connect(enableProxy_checkBox,  SIGNAL(stateChanged(int)), this, SLOT(enableProxy(int)));
  connect(enableProxyAuth_checkBox,  SIGNAL(stateChanged(int)), this, SLOT(enableProxyAuth(int)));
  connect(enableScan_checkBox, SIGNAL(stateChanged(int)), this, SLOT(enableDirScan(int)));
  connect(disableMaxConnec, SIGNAL(stateChanged(int)), this, SLOT(disableMaxConnecLimit(int)));
  // Apply button is activated when a value is changed
  // Main
  connect(txt_savePath, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
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
  // Misc Settings
  connect(combo_i18n, SIGNAL(currentIndexChanged(int)), this, SLOT(enableApplyButton()));
  connect(check_goToSysTray, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(clearFinished_checkBox, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(confirmExit_checkBox, SIGNAL(stateChanged(int)), this, SLOT(enableApplyButton()));
  connect(preview_program, SIGNAL(textChanged(QString)), this, SLOT(enableApplyButton()));
  connect(alwaysOSD, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(someOSD, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  connect(neverOSD, SIGNAL(toggled(bool)), this, SLOT(enableApplyButton()));
  // Disable apply Button
  applyButton->setEnabled(false);
}

// save options to options.xml file
bool options_imp::saveOptions(){
  QString savePath;
  QDomDocument doc("options");
  QDomElement root;
  QDomElement tag, tag2;
  QDomText optionValue;
  QString xml;
  FILE *f;
  QFile file;
  // Getting HOME evironment variable
  savePath = misc::qBittorrentPath() + "options.xml";
  // Check if min port < max port
  checkPortsLogic();
  // Generate XML
  root = doc.createElement("options");
  doc.appendChild(root);
  // Main Options
  tag = doc.createElement("save_path");
  root.appendChild(tag);
  optionValue = doc.createTextNode(txt_savePath->text());
  tag.appendChild(optionValue);
  tag = doc.createElement("dl_limit");
  root.appendChild(tag);
  if(spin_download->isEnabled()){
    optionValue = doc.createTextNode(misc::toString(spin_download->value()).c_str());
  }else{
    optionValue = doc.createTextNode(misc::toString(-1).c_str());
  }
  tag.appendChild(optionValue);
  tag = doc.createElement("up_limit");
  root.appendChild(tag);
  if(spin_upload->isEnabled()){
    optionValue = doc.createTextNode(misc::toString(spin_upload->value()).c_str());
  }else{
    optionValue = doc.createTextNode(misc::toString(-1).c_str());
  }
  tag.appendChild(optionValue);
  tag = doc.createElement("max_connec");
  root.appendChild(tag);
  if(spin_max_connec->isEnabled()){
    optionValue = doc.createTextNode(misc::toString(spin_max_connec->value()).c_str());
  }else{
    optionValue = doc.createTextNode(misc::toString(-1).c_str());
  }
  tag.appendChild(optionValue);
  tag = doc.createElement("port_range");
  root.appendChild(tag);
  tag2 = doc.createElement("port_min");
  tag.appendChild(tag2);
  optionValue = doc.createTextNode(misc::toString(spin_port_min->value()).c_str());
  tag2.appendChild(optionValue);
  tag2 = doc.createElement("port_max");
  tag.appendChild(tag2);
  optionValue = doc.createTextNode(misc::toString(spin_port_max->value()).c_str());
  tag2.appendChild(optionValue);
  tag = doc.createElement("ratio");
  root.appendChild(tag);
  if(spin_ratio->isEnabled()){
    optionValue = doc.createTextNode(misc::toString(spin_ratio->value()).c_str());
  }else{
    optionValue = doc.createTextNode("0");
  }
  tag.appendChild(optionValue);
  tag = doc.createElement("DHT");
  root.appendChild(tag);
  if(disableDHT->isChecked()){
    optionValue = doc.createTextNode("0");
  }else{
    optionValue = doc.createTextNode("1");
  }
  tag.appendChild(optionValue);
  if(scanDir->isEnabled()){
    tag = doc.createElement("scan_dir");
    root.appendChild(tag);
    optionValue = doc.createTextNode(scanDir->text());
    tag.appendChild(optionValue);
  }
  // IPFilter Settings
  if(filterGroup->isEnabled()){
    tag = doc.createElement("ipFilter_File");
    root.appendChild(tag);
    optionValue = doc.createTextNode(filterFile->text());
    tag.appendChild(optionValue);
  }
  // Proxy Settings
  if(groupProxy->isEnabled()){
    tag = doc.createElement("proxy_settings");
    root.appendChild(tag);
    tag2 = doc.createElement("proxy_ip");
    tag.appendChild(tag2);
    optionValue = doc.createTextNode(proxy_ip->text());
    tag2.appendChild(optionValue);
    tag2 = doc.createElement("proxy_port");
    tag.appendChild(tag2);
    optionValue = doc.createTextNode(proxy_port->text());
    tag2.appendChild(optionValue);
    // Proxy Authentication
    if(groupProxyAuth->isEnabled()){
      tag2 = doc.createElement("proxy_username");
      tag.appendChild(tag2);
      optionValue = doc.createTextNode(proxy_username->text());
      tag2.appendChild(optionValue);
      tag2 = doc.createElement("proxy_password");
      tag.appendChild(tag2);
      optionValue = doc.createTextNode(proxy_password->text());
      tag2.appendChild(optionValue);
    }
  }
  // GUI stuff
  tag =  doc.createElement("gui_settings");
  root.appendChild(tag);
  tag2 =  doc.createElement("goToSysTrayOnMinimizing");
  if(check_goToSysTray->isChecked()){
    optionValue = doc.createTextNode("true");
  }else{
    optionValue = doc.createTextNode("false");
  }
  tag2.appendChild(optionValue);
  tag.appendChild(tag2);
  tag2 =  doc.createElement("clearFinishedOnExit");
  if(clearFinished_checkBox->isChecked()){
    optionValue = doc.createTextNode("true");
  }else{
    optionValue = doc.createTextNode("false");
  }
  tag2.appendChild(optionValue);
  tag.appendChild(tag2);
  tag2 =  doc.createElement("confirmOnExit");
  if(confirmExit_checkBox->isChecked()){
    optionValue = doc.createTextNode("true");
  }else{
    optionValue = doc.createTextNode("false");
  }
  tag2.appendChild(optionValue);
  tag.appendChild(tag2);
  tag2 = doc.createElement("previewing");
  optionValue = doc.createTextNode(preview_program->text());
  tag2.appendChild(optionValue);
  tag.appendChild(tag2);
  tag2 =  doc.createElement("displayOSD");
  if(alwaysOSD->isChecked()){
    optionValue = doc.createTextNode("true");
  }else{
    if(someOSD->isChecked()){
      optionValue = doc.createTextNode("partial");
    }else{
      optionValue = doc.createTextNode("false");
    }
  }
  tag2.appendChild(optionValue);
  tag.appendChild(tag2);
  // Language Settings
  tag = doc.createElement("locale");
  root.appendChild(tag);
  optionValue = doc.createTextNode(getLocale());
  tag.appendChild(optionValue);
  xml = doc.toString();
  // Write XML file to HD
  f = fopen(savePath.toStdString().c_str(), "w");
  if (!f){
    std::cout << "Error: Couldn't create file " << savePath.toStdString() << " for saving!" << '\n';
    return false;
  }
  if (!file.open(f, QIODevice::WriteOnly | QIODevice::Text)){
    std::cout << "Error: Couldn't open file " << savePath.toStdString() << " for saving!" << '\n';
    return false;
  }
  file.write(xml.toStdString().c_str(), xml.length());
  file.close();
  if(fclose(f) == EOF){
    std::cout << "Error: Couldn't close file " << savePath.toStdString() << " after saving!" << '\n';
    return false;
  }
  // set infobar text
  emit status_changed(tr("Options saved successfully!"));
  // Disable apply Button
  applyButton->setEnabled(false);
  return true;
}

bool options_imp::isFilteringEnabled() const{
  return activateFilter->isChecked();
}

// Load options from options.xml file
bool options_imp::loadOptions(){
  QString savePath;
  QDomDocument doc("options");
  QDomElement root;
  QDomElement tag, tag2;
  QDomText optionValue;
  QString xml;
  FILE *f;
  QFile file;
  int tmp;
  float tmpFloat;
  // Getting savepath for options.xml
  savePath = misc::qBittorrentPath() + "options.xml";
  // Read XML file on HD
  f = fopen(savePath.toStdString().c_str(), "r");
  if (!f){
    return false;
  }
  if (!file.open(f, QIODevice::ReadOnly | QIODevice::Text)){
    return false;
  }
  if (!doc.setContent(&file)) {
    file.close();
    return false;
  }
  file.close();
  if(fclose(f) == EOF){
    std::cout << "Error: Couldn't close file " << savePath.toStdString() << " after reading!" << '\n';
    return false;
  }
  // Loading option from XML
  root = doc.firstChildElement("options");
  this->txt_savePath->setText(root.firstChildElement("save_path").text());
  tmp = root.firstChildElement("dl_limit").text().toInt();
  if(tmp == -1){
    disableDLLimit->setChecked(true);
    spin_download->setEnabled(false);
  }else{
    disableDLLimit->setChecked(false);
    spin_download->setEnabled(true);
    this->spin_download->setValue(tmp);
  }
  tmp = root.firstChildElement("up_limit").text().toInt();
  if(tmp == -1){
    disableUPLimit->setChecked(true);
    spin_upload->setEnabled(false);
  }else{
    disableUPLimit->setChecked(false);
    spin_upload->setEnabled(true);
    this->spin_upload->setValue(tmp);
  }
  tmpFloat = root.firstChildElement("ratio").text().toFloat();
  if(tmpFloat == 0){
    disableRatio->setChecked(true);
    spin_ratio->setEnabled(false);
  }else{
    disableRatio->setChecked(false);
    spin_ratio->setEnabled(true);
    this->spin_ratio->setValue(tmpFloat);
  }
  tag = root.firstChildElement("DHT");
  if(!tag.isNull()){
    if(tag.text().toInt() == 0){
      disableDHT->setChecked(true);
    }else{
      disableDHT->setChecked(false);
    }
  }
  tmp = root.firstChildElement("max_connec").text().toInt();
  if(tmp == -1){
    disableMaxConnec->setChecked(true);
    spin_max_connec->setEnabled(false);
  }else{
    disableMaxConnec->setChecked(false);
    spin_max_connec->setEnabled(true);
    this->spin_max_connec->setValue(tmp);
  }
  tag = root.firstChildElement("port_range");
  this->spin_port_min->setValue(tag.firstChildElement("port_min").text().toInt());
  this->spin_port_max->setValue(tag.firstChildElement("port_max").text().toInt());
  tag = root.firstChildElement("scan_dir");
  if(!tag.isNull()){
    enableScan_checkBox->setChecked(true);
    scanDir->setEnabled(true);
    scanDir->setText(tag.text());
  }else{
    enableScan_checkBox->setChecked(false);
    scanDir->setEnabled(false);
  }
  // Load IPFilter Settings
  tag = root.firstChildElement("ipFilter_File");
  if(!tag.isNull()){
    activateFilter->setChecked(true);
    filterGroup->setEnabled(true);
    filterFile->setText(tag.text());
    processFilterFile(tag.text());
  }else{
    activateFilter->setChecked(false);
    filterGroup->setEnabled(false);
  }
  // Load proxy settings
  tag = root.firstChildElement("proxy_settings");
  if(!tag.isNull()){
    enableProxy_checkBox->setChecked(true);
    groupProxy->setEnabled(true);
    proxy_ip->setText(tag.firstChildElement("proxy_ip").text());
    proxy_port->setValue(tag.firstChildElement("proxy_port").text().toInt());
    tag2 = tag.firstChildElement("proxy_username");
    if(!tag2.isNull()){
      enableProxyAuth_checkBox->setChecked(true);
      groupProxyAuth->setEnabled(true);
      proxy_username->setText(tag2.text());
      proxy_password->setText(tag.firstChildElement("proxy_password").text());
    }else{
      enableProxyAuth_checkBox->setChecked(false);
      groupProxyAuth->setEnabled(false);
    }
  }else{
    enableProxyAuth_checkBox->setChecked(false);
    enableProxy_checkBox->setChecked(false);
    groupProxyAuth->setEnabled(false);
    groupProxy->setEnabled(false);
  }
  // Locale Settings
  tag = root.firstChildElement("locale");
  if(!tag.isNull()){
      setLocale(tag.text());
  }
  // Gui Settings
  tag = root.firstChildElement("gui_settings");
  if(!tag.isNull()){
    if(tag.firstChildElement("goToSysTrayOnMinimizing").text() == "false"){
      check_goToSysTray->setChecked(false);
    }else{
      check_goToSysTray->setChecked(true);
    }
    if(tag.firstChildElement("clearFinishedOnExit").text() == "false"){
      clearFinished_checkBox->setChecked(false);
    }else{
      clearFinished_checkBox->setChecked(true);
    }
    if(tag.firstChildElement("confirmOnExit").text() == "false"){
      confirmExit_checkBox->setChecked(false);
    }else{
      confirmExit_checkBox->setChecked(true);
    }
    preview_program->setText(tag.firstChildElement("previewing").text());
    QString OSDValue = tag.firstChildElement("displayOSD").text();
    if(OSDValue == "false"){
      neverOSD->setChecked(true);
    }else{
      if(OSDValue == "partial"){
        someOSD->setChecked(true);
      }else{
        alwaysOSD->setChecked(true);
      }
    }
  }
  // Disable apply Button
  applyButton->setEnabled(false);
  return true;
}

// return min & max ports
// [min, max]
std::pair<unsigned short, unsigned short> options_imp::getPorts() const{
  return std::make_pair(this->spin_port_min->value(), this->spin_port_min->value());
}

QString options_imp::getPreviewProgram() const{
  QString preview_txt = preview_program->text();
  preview_txt.trimmed();
  return preview_txt;
}

bool options_imp::getGoToSysTrayOnMinimizingWindow() const{
  return check_goToSysTray->isChecked();
}

bool options_imp::getConfirmOnExit() const{
  return confirmExit_checkBox->isChecked();
}

bool options_imp::isDHTEnabled() const{
  return !disableDHT->isChecked();
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
  return alwaysOSD->isChecked();
}

// Should the program use OSD when the window is hidden only?
bool options_imp::getUseOSDWhenHiddenOnly() const{
  return someOSD->isChecked();
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

void options_imp::showLoad(){
  if(!loadOptions()){
    std::cout << "Warning: Couldn't load options" << '\n';
  }
  this->show();
}

void options_imp::on_okButton_clicked(){
  if(applyButton->isEnabled()){
    saveOptions();
    applyButton->setEnabled(false);
  }
  this->hide();
}

bool options_imp::getClearFinishedOnExit() const{
  return clearFinished_checkBox->isChecked();
}

void options_imp::on_applyButton_clicked(){
  saveOptions();
}

void options_imp::on_cancelButton_clicked(){
  this->hide();
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
  QString dir = QFileDialog::getExistingDirectory(this, tr("Choose Scan Directory"), QDir::homePath());
  if(!dir.isNull()){
    scanDir->setText(dir);
  }
}

void options_imp::on_filterBrowse_clicked(){
  QString ipfilter = QFileDialog::getOpenFileName(this, tr("Choose ipfilter.dat file"), QDir::homePath());
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
  QString dir = QFileDialog::getExistingDirectory(this, tr("Choose save Directory"), QDir::homePath());
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
void options_imp::processFilterFile(const QString& filePath){
  filtersList->clear();
  QString manualFilters= misc::qBittorrentPath() + "ipfilter.dat";
  QStringList filterFiles(manualFilters);
  filterFiles.append(filePath);
  for(int i=0; i<2; ++i){
    QFile file(filterFiles.at(i));
    QStringList IP;
    if (file.exists()){
      if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        QMessageBox::critical(0, tr("I/O Error"), tr("Couldn't open:")+" "+filePath+" "+tr("in read mode."));
        return;
      }
      unsigned int nbLine = 0;
      while (!file.atEnd()) {
        ++nbLine;
        QByteArray line = file.readLine();
        QString strLine = QString(line.data());
        if(!strLine.startsWith('#') && !strLine.startsWith("//")){
          // Line is not commented
          QStringList partsList = strLine.split(',');
          unsigned int nbElem = partsList.size();
          if(nbElem != 2 && nbElem != 3){
            QMessageBox::critical(0, tr("Invalid Line"), tr("Line")+" "+QString(misc::toString(nbLine).c_str())+" "+tr("is malformed."));
            return;
          }
          int nbAccess = partsList.at(1).trimmed().toInt();
          if(nbAccess <= 127){
            QString strComment;
            QString strStartIP = partsList.at(0).split('-').at(0).trimmed();
            QString strEndIP = partsList.at(0).split('-').at(1).trimmed();
            if(partsList.size() == 3){
              strComment = partsList.at(2).trimmed();
            }else{
              strComment = QString();
            }
            // Split IP
            IP = strStartIP.split('.');
            if(IP.size() != 4){
              continue;
            }
            address start(address::from_string(strStartIP.toStdString()));
            IP = strEndIP.split('.');
            if(IP.size() != 4){
              continue;
            }
            address last(address::from_string(strEndIP.toStdString()));
            // add it to list
            QStringList item(strStartIP);
            item.append(strEndIP);
            if(!i){
              item.append("Manual");
            }else{
              item.append("ipfilter.dat");
            }
            item.append(strComment);
            new QTreeWidgetItem(filtersList, item);
            // Apply to bittorrent session
            filter.add_rule(start, last, ip_filter::blocked);
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
  if (!ok || startIP.isEmpty() || IP1.size() != 4){
    QMessageBox::critical(0, tr("Incorrect IP"), tr("This IP is incorrect."));
    return;
  }
  for(int i=0; i<4; ++i){
    QString part = IP1.at(i).trimmed();
    if(part.isEmpty() || part.toInt() < 0 || part.toInt() > 255){
      QMessageBox::critical(0, tr("Incorrect IP"), tr("This IP is incorrect."));
      return;
    }
  }
  // Ask user for last ip
  QString lastIP = QInputDialog::getText(this, tr("Range End IP"),
                                          tr("End IP:"), QLineEdit::Normal,
                                          startIP, &ok);
  QStringList IP2 = lastIP.split('.');
  // check IP
  if (!ok || lastIP.isEmpty() || IP2.size() != 4){
    QMessageBox::critical(0, tr("Incorrect IP"), tr("This IP is incorrect."));
    return;
  }
  for(int i=0; i<4; ++i){
    QString part = IP2.at(i).trimmed();
    if(part.isEmpty() || part.toInt() < 0 || part.toInt() > 255){
      QMessageBox::critical(0, tr("Incorrect IP"), tr("This IP is incorrect."));
      return;
    }
  }
  // Ask user for Comment
  QString comment = QInputDialog::getText(this, tr("IP Range Comment"),
                                         tr("Comment:"), QLineEdit::Normal,
                                         "", &ok);
  if (!ok){
    comment = QString("");
  }
  QFile ipfilter(misc::qBittorrentPath() + "ipfilter.dat");
  if (!ipfilter.open(QIODevice::Append | QIODevice::WriteOnly | QIODevice::Text)){
    std::cout << "Error: Couldn't write in ipfilter.dat";
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
    std::cout << "Error: Couldn't write in ipfilter.dat";
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
