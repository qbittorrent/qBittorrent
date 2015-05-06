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

#include "geoipmanager.h"

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

#include <QDir>
#include <QFile>
#include <QChar>

#include "core/utils/fs.h"
#include <libtorrent/session.hpp>

using namespace libtorrent;

QString GeoIPManager::geoipFolder(bool embedded) {
#ifdef WITH_GEOIP_EMBEDDED
  if (embedded)
    return ":/geoip/";
  return Utils::Fs::QDesktopServicesDataLocation()+"geoip"+"/";
#else
  Q_UNUSED(embedded);
  if (QFile::exists("/usr/local/share/GeoIP/GeoIP.dat"))
    return "/usr/local/share/GeoIP/";
  if (QFile::exists("/var/lib/GeoIP/GeoIP.dat"))
    return "/var/lib/GeoIP/";
  return "/usr/share/GeoIP/";
#endif
}

QString GeoIPManager::geoipDBpath(bool embedded) {
  return geoipFolder(embedded)+"GeoIP.dat";
}

#ifdef WITH_GEOIP_EMBEDDED
void GeoIPManager::exportEmbeddedDb() {
  if (!QFile::exists(geoipDBpath(false)) || QFile(geoipDBpath(false)).size() != QFile(geoipDBpath(true)).size()) { // Export is required
    qDebug("A local Geoip database update is required, proceeding...");
    // Create geoip folder is necessary
    QDir gfolder(geoipFolder(false));
    if (!gfolder.exists()) {
      if (!gfolder.mkpath(geoipFolder(false))) {
        std::cerr << "Failed to create geoip folder at " << qPrintable(geoipFolder(false)) << std::endl;
        return;
      }
    }
    // Remove destination files
    if (QFile::exists(geoipDBpath(false)))
      Utils::Fs::forceRemove(geoipDBpath(false));
    // Copy from executable to hard disk
    qDebug("%s -> %s", qPrintable(geoipDBpath(true)), qPrintable(geoipDBpath(false)));
    if (!QFile::copy(geoipDBpath(true), geoipDBpath(false))) {
      std::cerr << "ERROR: Failed to copy geoip.dat from executable to hard disk" << std::endl;
    }
    qDebug("Local Geoip database was updated");
  }
}
#endif

void GeoIPManager::loadDatabase(session *s) {
#ifdef WITH_GEOIP_EMBEDDED
  exportEmbeddedDb();
#endif
  if (QFile::exists(geoipDBpath(false))) {
    qDebug("Loading GeoIP database from %s...", qPrintable(geoipDBpath(false)));
    s->load_country_db(geoipDBpath(false).toLocal8Bit().constData());
  } else {
    qDebug("ERROR: Impossible to find local Geoip Database");
  }
}

const char country_code[253][3] =
{ "--","AP","EU","AD","AE","AF","AG","AI","AL","AM","AN",
  "AO","AQ","AR","AS","AT","AU","AW","AZ","BA","BB",
  "BD","BE","BF","BG","BH","BI","BJ","BM","BN","BO",
  "BR","BS","BT","BV","BW","BY","BZ","CA","CC","CD",
  "CF","CG","CH","CI","CK","CL","CM","CN","CO","CR",
  "CU","CV","CX","CY","CZ","DE","DJ","DK","DM","DO",
  "DZ","EC","EE","EG","EH","ER","ES","ET","FI","FJ",
  "FK","FM","FO","FR","FX","GA","GB","GD","GE","GF",
  "GH","GI","GL","GM","GN","GP","GQ","GR","GS","GT",
  "GU","GW","GY","HK","HM","HN","HR","HT","HU","ID",
  "IE","IL","IN","IO","IQ","IR","IS","IT","JM","JO",
  "JP","KE","KG","KH","KI","KM","KN","KP","KR","KW",
  "KY","KZ","LA","LB","LC","LI","LK","LR","LS","LT",
  "LU","LV","LY","MA","MC","MD","MG","MH","MK","ML",
  "MM","MN","MO","MP","MQ","MR","MS","MT","MU","MV",
  "MW","MX","MY","MZ","NA","NC","NE","NF","NG","NI",
  "NL","NO","NP","NR","NU","NZ","OM","PA","PE","PF",
  "PG","PH","PK","PL","PM","PN","PR","PS","PT","PW",
  "PY","QA","RE","RO","RU","RW","SA","SB","SC","SD",
  "SE","SG","SH","SI","SJ","SK","SL","SM","SN","SO",
  "SR","ST","SV","SY","SZ","TC","TD","TF","TG","TH",
  "TJ","TK","TM","TN","TO","TL","TR","TT","TV","TW",
  "TZ","UA","UG","UM","US","UY","UZ","VA","VC","VE",
  "VG","VI","VN","VU","WF","WS","YE","YT","RS","ZA",
  "ZM","ME","ZW","A1","A2","O1","AX","GG","IM","JE",
  "BL","MF"};

static const uint num_countries = (unsigned)(sizeof(country_code)/sizeof(country_code[0]));

const char * country_name[253] =
{"N/A","Asia/Pacific Region","Europe","Andorra","United Arab Emirates","Afghanistan","Antigua and Barbuda","Anguilla","Albania","Armenia","Netherlands Antilles",
 "Angola","Antarctica","Argentina","American Samoa","Austria","Australia","Aruba","Azerbaijan","Bosnia and Herzegovina","Barbados",
 "Bangladesh","Belgium","Burkina Faso","Bulgaria","Bahrain","Burundi","Benin","Bermuda","Brunei Darussalam","Bolivia",
 "Brazil","Bahamas","Bhutan","Bouvet Island","Botswana","Belarus","Belize","Canada","Cocos (Keeling) Islands","Congo, The Democratic Republic of the",
 "Central African Republic","Congo","Switzerland","Cote D'Ivoire","Cook Islands","Chile","Cameroon","China","Colombia","Costa Rica",
 "Cuba","Cape Verde","Christmas Island","Cyprus","Czech Republic","Germany","Djibouti","Denmark","Dominica","Dominican Republic",
 "Algeria","Ecuador","Estonia","Egypt","Western Sahara","Eritrea","Spain","Ethiopia","Finland","Fiji",
 "Falkland Islands (Malvinas)","Micronesia, Federated States of","Faroe Islands","France","France, Metropolitan","Gabon","United Kingdom","Grenada","Georgia","French Guiana",
 "Ghana","Gibraltar","Greenland","Gambia","Guinea","Guadeloupe","Equatorial Guinea","Greece","South Georgia and the South Sandwich Islands","Guatemala",
 "Guam","Guinea-Bissau","Guyana","Hong Kong","Heard Island and McDonald Islands","Honduras","Croatia","Haiti","Hungary","Indonesia",
 "Ireland","Israel","India","British Indian Ocean Territory","Iraq","Iran, Islamic Republic of","Iceland","Italy","Jamaica","Jordan",
 "Japan","Kenya","Kyrgyzstan","Cambodia","Kiribati","Comoros","Saint Kitts and Nevis","Korea, Democratic People's Republic of","Korea, Republic of","Kuwait",
 "Cayman Islands","Kazakhstan","Lao People's Democratic Republic","Lebanon","Saint Lucia","Liechtenstein","Sri Lanka","Liberia","Lesotho","Lithuania",
 "Luxembourg","Latvia","Libyan Arab Jamahiriya","Morocco","Monaco","Moldova, Republic of","Madagascar","Marshall Islands","Macedonia","Mali",
 "Myanmar","Mongolia","Macau","Northern Mariana Islands","Martinique","Mauritania","Montserrat","Malta","Mauritius","Maldives",
 "Malawi","Mexico","Malaysia","Mozambique","Namibia","New Caledonia","Niger","Norfolk Island","Nigeria","Nicaragua",
 "Netherlands","Norway","Nepal","Nauru","Niue","New Zealand","Oman","Panama","Peru","French Polynesia",
 "Papua New Guinea","Philippines","Pakistan","Poland","Saint Pierre and Miquelon","Pitcairn Islands","Puerto Rico","Palestinian Territory","Portugal","Palau",
 "Paraguay","Qatar","Reunion","Romania","Russian Federation","Rwanda","Saudi Arabia","Solomon Islands","Seychelles","Sudan",
 "Sweden","Singapore","Saint Helena","Slovenia","Svalbard and Jan Mayen","Slovakia","Sierra Leone","San Marino","Senegal","Somalia","Suriname",
 "Sao Tome and Principe","El Salvador","Syrian Arab Republic","Swaziland","Turks and Caicos Islands","Chad","French Southern Territories","Togo","Thailand",
 "Tajikistan","Tokelau","Turkmenistan","Tunisia","Tonga","Timor-Leste","Turkey","Trinidad and Tobago","Tuvalu","Taiwan",
 "Tanzania, United Republic of","Ukraine","Uganda","United States Minor Outlying Islands","United States","Uruguay","Uzbekistan","Holy See (Vatican City State)","Saint Vincent and the Grenadines","Venezuela",
 "Virgin Islands, British","Virgin Islands, U.S.","Vietnam","Vanuatu","Wallis and Futuna","Samoa","Yemen","Mayotte","Serbia","South Africa",
 "Zambia","Montenegro","Zimbabwe","Anonymous Proxy","Satellite Provider","Other","Aland Islands","Guernsey","Isle of Man","Jersey",
 "Saint Barthelemy","Saint Martin"};

QString GeoIPManager::CountryISOCodeToName(const QString &iso) {
  if (iso.isEmpty()) return "N/A";

  for (uint i = 0; i < num_countries; ++i) {
    if (iso == country_code[i]) {
      return QLatin1String(country_name[i]);
    }
  }
  qDebug("GeoIPManager: Country name resolution failed for: %s", qPrintable(iso));
  return "N/A";
}

// http://www.iso.org/iso/country_codes/iso_3166_code_lists/english_country_names_and_code_elements.htm
QIcon GeoIPManager::CountryISOCodeToIcon(const QString &iso) {
  if (iso.isEmpty() || (iso[0] == '!')) return QIcon();
  return QIcon(":/icons/flags/" + iso.toLower() + ".png");
}

