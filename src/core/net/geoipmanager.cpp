/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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
 */

#include <QDebug>
#include <QFile>
#include <QDir>
#include <QHostAddress>
#include <QDateTime>

#include "core/logger.h"
#include "core/preferences.h"
#include "core/utils/fs.h"
#include "core/utils/gzip.h"
#include "downloadmanager.h"
#include "downloadhandler.h"
#include "private/geoipdatabase.h"
#include "geoipmanager.h"

static const char DATABASE_URL[] = "http://geolite.maxmind.com/download/geoip/database/GeoLite2-Country.mmdb.gz";
static const char GEOIP_FOLDER[] = "GeoIP";
static const char GEOIP_FILENAME[] = "GeoLite2-Country.mmdb";
static const int CACHE_SIZE = 1000;
static const int UPDATE_INTERVAL = 30; // Days between database updates

using namespace Net;

// GeoIPManager

GeoIPManager *GeoIPManager::m_instance = 0;

GeoIPManager::GeoIPManager()
    : m_enabled(false)
    , m_geoIPDatabase(0)
{
    configure();
    connect(Preferences::instance(), SIGNAL(changed()), SLOT(configure()));
}

GeoIPManager::~GeoIPManager()
{
    if (m_geoIPDatabase)
        delete m_geoIPDatabase;
}

void GeoIPManager::initInstance()
{
    if (!m_instance)
        m_instance = new GeoIPManager;
}

void GeoIPManager::freeInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = 0;
    }
}

GeoIPManager *GeoIPManager::instance()
{
    return m_instance;
}

void GeoIPManager::loadDatabase()
{
    if (m_geoIPDatabase) {
        delete m_geoIPDatabase;
        m_geoIPDatabase = 0;
    }

    QString filepath = Utils::Fs::expandPathAbs(
                QString("%1%2/%3").arg(Utils::Fs::QDesktopServicesDataLocation())
                .arg(GEOIP_FOLDER).arg(GEOIP_FILENAME));

    QString error;
    m_geoIPDatabase = GeoIPDatabase::load(filepath, error);
    if (m_geoIPDatabase)
        Logger::instance()->addMessage(tr("GeoIP database loaded. Type: %1. Build time: %2.")
                                       .arg(m_geoIPDatabase->type()).arg(m_geoIPDatabase->buildEpoch().toString()),
                                       Log::INFO);
    else
        Logger::instance()->addMessage(tr("Couldn't load GeoIP database. Reason: %1").arg(error), Log::WARNING);

    manageDatabaseUpdate();
}

void GeoIPManager::manageDatabaseUpdate()
{
    if (!m_geoIPDatabase || (m_geoIPDatabase->buildEpoch().daysTo(QDateTime::currentDateTimeUtc()) >= UPDATE_INTERVAL))
        downloadDatabaseFile();
}

void GeoIPManager::downloadDatabaseFile()
{
    DownloadHandler *handler = DownloadManager::instance()->downloadUrl(DATABASE_URL);
    connect(handler, SIGNAL(downloadFinished(QString, QByteArray)), SLOT(downloadFinished(QString, QByteArray)));
    connect(handler, SIGNAL(downloadFailed(QString, QString)), SLOT(downloadFailed(QString, QString)));
}

QString GeoIPManager::lookup(const QHostAddress &hostAddr) const
{
    if (m_enabled && m_geoIPDatabase)
        return m_geoIPDatabase->lookup(hostAddr);

    return QString();
}

// http://www.iso.org/iso/country_codes/iso_3166_code_lists/english_country_names_and_code_elements.htm
QString GeoIPManager::CountryName(const QString &countryISOCode)
{
    static QHash<QString, QString> countries;
    static bool initialized = false;

    if (!initialized) {
        countries[QString()] = "N/A";
        countries["AP"] = "Asia/Pacific Region";
        countries["EU"] = "Europe";
        countries["AD"] = "Andorra";
        countries["AE"] = "United Arab Emirates";
        countries["AF"] = "Afghanistan";
        countries["AG"] = "Antigua and Barbuda";
        countries["AI"] = "Anguilla";
        countries["AL"] = "Albania";
        countries["AM"] = "Armenia";
        countries["AN"] = "Netherlands Antilles";
        countries["AO"] = "Angola";
        countries["AQ"] = "Antarctica";
        countries["AR"] = "Argentina";
        countries["AS"] = "American Samoa";
        countries["AT"] = "Austria";
        countries["AU"] = "Australia";
        countries["AW"] = "Aruba";
        countries["AZ"] = "Azerbaijan";
        countries["BA"] = "Bosnia and Herzegovina";
        countries["BB"] = "Barbados";
        countries["BD"] = "Bangladesh";
        countries["BE"] = "Belgium";
        countries["BF"] = "Burkina Faso";
        countries["BG"] = "Bulgaria";
        countries["BH"] = "Bahrain";
        countries["BI"] = "Burundi";
        countries["BJ"] = "Benin";
        countries["BM"] = "Bermuda";
        countries["BN"] = "Brunei Darussalam";
        countries["BO"] = "Bolivia";
        countries["BR"] = "Brazil";
        countries["BS"] = "Bahamas";
        countries["BT"] = "Bhutan";
        countries["BV"] = "Bouvet Island";
        countries["BW"] = "Botswana";
        countries["BY"] = "Belarus";
        countries["BZ"] = "Belize";
        countries["CA"] = "Canada";
        countries["CC"] = "Cocos (Keeling) Islands";
        countries["CD"] = "Congo, The Democratic Republic of the";
        countries["CF"] = "Central African Republic";
        countries["CG"] = "Congo";
        countries["CH"] = "Switzerland";
        countries["CI"] = "Cote D'Ivoire";
        countries["CK"] = "Cook Islands";
        countries["CL"] = "Chile";
        countries["CM"] = "Cameroon";
        countries["CN"] = "China";
        countries["CO"] = "Colombia";
        countries["CR"] = "Costa Rica";
        countries["CU"] = "Cuba";
        countries["CV"] = "Cape Verde";
        countries["CX"] = "Christmas Island";
        countries["CY"] = "Cyprus";
        countries["CZ"] = "Czech Republic";
        countries["DE"] = "Germany";
        countries["DJ"] = "Djibouti";
        countries["DK"] = "Denmark";
        countries["DM"] = "Dominica";
        countries["DO"] = "Dominican Republic";
        countries["DZ"] = "Algeria";
        countries["EC"] = "Ecuador";
        countries["EE"] = "Estonia";
        countries["EG"] = "Egypt";
        countries["EH"] = "Western Sahara";
        countries["ER"] = "Eritrea";
        countries["ES"] = "Spain";
        countries["ET"] = "Ethiopia";
        countries["FI"] = "Finland";
        countries["FJ"] = "Fiji";
        countries["FK"] = "Falkland Islands (Malvinas)";
        countries["FM"] = "Micronesia, Federated States of";
        countries["FO"] = "Faroe Islands";
        countries["FR"] = "France";
        countries["FX"] = "France, Metropolitan";
        countries["GA"] = "Gabon";
        countries["GB"] = "United Kingdom";
        countries["GD"] = "Grenada";
        countries["GE"] = "Georgia";
        countries["GF"] = "French Guiana";
        countries["GH"] = "Ghana";
        countries["GI"] = "Gibraltar";
        countries["GL"] = "Greenland";
        countries["GM"] = "Gambia";
        countries["GN"] = "Guinea";
        countries["GP"] = "Guadeloupe";
        countries["GQ"] = "Equatorial Guinea";
        countries["GR"] = "Greece";
        countries["GS"] = "South Georgia and the South Sandwich Islands";
        countries["GT"] = "Guatemala";
        countries["GU"] = "Guam";
        countries["GW"] = "Guinea-Bissau";
        countries["GY"] = "Guyana";
        countries["HK"] = "Hong Kong";
        countries["HM"] = "Heard Island and McDonald Islands";
        countries["HN"] = "Honduras";
        countries["HR"] = "Croatia";
        countries["HT"] = "Haiti";
        countries["HU"] = "Hungary";
        countries["ID"] = "Indonesia";
        countries["IE"] = "Ireland";
        countries["IL"] = "Israel";
        countries["IN"] = "India";
        countries["IO"] = "British Indian Ocean Territory";
        countries["IQ"] = "Iraq";
        countries["IR"] = "Iran, Islamic Republic of";
        countries["IS"] = "Iceland";
        countries["IT"] = "Italy";
        countries["JM"] = "Jamaica";
        countries["JO"] = "Jordan";
        countries["JP"] = "Japan";
        countries["KE"] = "Kenya";
        countries["KG"] = "Kyrgyzstan";
        countries["KH"] = "Cambodia";
        countries["KI"] = "Kiribati";
        countries["KM"] = "Comoros";
        countries["KN"] = "Saint Kitts and Nevis";
        countries["KP"] = "Korea, Democratic People's Republic of";
        countries["KR"] = "Korea, Republic of";
        countries["KW"] = "Kuwait";
        countries["KY"] = "Cayman Islands";
        countries["KZ"] = "Kazakhstan";
        countries["LA"] = "Lao People's Democratic Republic";
        countries["LB"] = "Lebanon";
        countries["LC"] = "Saint Lucia";
        countries["LI"] = "Liechtenstein";
        countries["LK"] = "Sri Lanka";
        countries["LR"] = "Liberia";
        countries["LS"] = "Lesotho";
        countries["LT"] = "Lithuania";
        countries["LU"] = "Luxembourg";
        countries["LV"] = "Latvia";
        countries["LY"] = "Libyan Arab Jamahiriya";
        countries["MA"] = "Morocco";
        countries["MC"] = "Monaco";
        countries["MD"] = "Moldova, Republic of";
        countries["MG"] = "Madagascar";
        countries["MH"] = "Marshall Islands";
        countries["MK"] = "Macedonia";
        countries["ML"] = "Mali";
        countries["MM"] = "Myanmar";
        countries["MN"] = "Mongolia";
        countries["MO"] = "Macau";
        countries["MP"] = "Northern Mariana Islands";
        countries["MQ"] = "Martinique";
        countries["MR"] = "Mauritania";
        countries["MS"] = "Montserrat";
        countries["MT"] = "Malta";
        countries["MU"] = "Mauritius";
        countries["MV"] = "Maldives";
        countries["MW"] = "Malawi";
        countries["MX"] = "Mexico";
        countries["MY"] = "Malaysia";
        countries["MZ"] = "Mozambique";
        countries["NA"] = "Namibia";
        countries["NC"] = "New Caledonia";
        countries["NE"] = "Niger";
        countries["NF"] = "Norfolk Island";
        countries["NG"] = "Nigeria";
        countries["NI"] = "Nicaragua";
        countries["NL"] = "Netherlands";
        countries["NO"] = "Norway";
        countries["NP"] = "Nepal";
        countries["NR"] = "Nauru";
        countries["NU"] = "Niue";
        countries["NZ"] = "New Zealand";
        countries["OM"] = "Oman";
        countries["PA"] = "Panama";
        countries["PE"] = "Peru";
        countries["PF"] = "French Polynesia";
        countries["PG"] = "Papua New Guinea";
        countries["PH"] = "Philippines";
        countries["PK"] = "Pakistan";
        countries["PL"] = "Poland";
        countries["PM"] = "Saint Pierre and Miquelon";
        countries["PN"] = "Pitcairn Islands";
        countries["PR"] = "Puerto Rico";
        countries["PS"] = "Palestinian Territory";
        countries["PT"] = "Portugal";
        countries["PW"] = "Palau";
        countries["PY"] = "Paraguay";
        countries["QA"] = "Qatar";
        countries["RE"] = "Reunion";
        countries["RO"] = "Romania";
        countries["RU"] = "Russian Federation";
        countries["RW"] = "Rwanda";
        countries["SA"] = "Saudi Arabia";
        countries["SB"] = "Solomon Islands";
        countries["SC"] = "Seychelles";
        countries["SD"] = "Sudan";
        countries["SE"] = "Sweden";
        countries["SG"] = "Singapore";
        countries["SH"] = "Saint Helena";
        countries["SI"] = "Slovenia";
        countries["SJ"] = "Svalbard and Jan Mayen";
        countries["SK"] = "Slovakia";
        countries["SL"] = "Sierra Leone";
        countries["SM"] = "San Marino";
        countries["SN"] = "Senegal";
        countries["SO"] = "Somalia";
        countries["SR"] = "Suriname";
        countries["ST"] = "Sao Tome and Principe";
        countries["SV"] = "El Salvador";
        countries["SY"] = "Syrian Arab Republic";
        countries["SZ"] = "Swaziland";
        countries["TC"] = "Turks and Caicos Islands";
        countries["TD"] = "Chad";
        countries["TF"] = "French Southern Territories";
        countries["TG"] = "Togo";
        countries["TH"] = "Thailand";
        countries["TJ"] = "Tajikistan";
        countries["TK"] = "Tokelau";
        countries["TM"] = "Turkmenistan";
        countries["TN"] = "Tunisia";
        countries["TO"] = "Tonga";
        countries["TL"] = "Timor-Leste";
        countries["TR"] = "Turkey";
        countries["TT"] = "Trinidad and Tobago";
        countries["TV"] = "Tuvalu";
        countries["TW"] = "Taiwan";
        countries["TZ"] = "Tanzania, United Republic of";
        countries["UA"] = "Ukraine";
        countries["UG"] = "Uganda";
        countries["UM"] = "United States Minor Outlying Islands";
        countries["US"] = "United States";
        countries["UY"] = "Uruguay";
        countries["UZ"] = "Uzbekistan";
        countries["VA"] = "Holy See (Vatican City State)";
        countries["VC"] = "Saint Vincent and the Grenadines";
        countries["VE"] = "Venezuela";
        countries["VG"] = "Virgin Islands, British";
        countries["VI"] = "Virgin Islands, U.S.";
        countries["VN"] = "Vietnam";
        countries["VU"] = "Vanuatu";
        countries["WF"] = "Wallis and Futuna";
        countries["WS"] = "Samoa";
        countries["YE"] = "Yemen";
        countries["YT"] = "Mayotte";
        countries["RS"] = "Serbia";
        countries["ZA"] = "South Africa";
        countries["ZM"] = "Zambia";
        countries["ME"] = "Montenegro";
        countries["ZW"] = "Zimbabwe";
        countries["A1"] = "Anonymous Proxy";
        countries["A2"] = "Satellite Provider";
        countries["O1"] = "Other";
        countries["AX"] = "Aland Islands";
        countries["GG"] = "Guernsey";
        countries["IM"] = "Isle of Man";
        countries["JE"] = "Jersey";
        countries["BL"] = "Saint Barthelemy";
        countries["MF"] = "Saint Martin";

        initialized = true;
    }

    return countries.value(countryISOCode, "N/A");
}

void GeoIPManager::configure()
{
    const bool enabled = Preferences::instance()->resolvePeerCountries();
    if (m_enabled != enabled) {
        m_enabled = enabled;
        if (m_enabled && !m_geoIPDatabase) {
            loadDatabase();
        }
        else if (!m_enabled && m_geoIPDatabase) {
            delete m_geoIPDatabase;
            m_geoIPDatabase = 0;
        }
    }
}

void GeoIPManager::downloadFinished(const QString &url, QByteArray data)
{
    Q_UNUSED(url);

    if (!Utils::Gzip::uncompress(data, data)) {
        Logger::instance()->addMessage(tr("Could not uncompress GeoIP database file."), Log::WARNING);
        return;
    }

    QString error;
    GeoIPDatabase *geoIPDatabase = GeoIPDatabase::load(data, error);
    if (geoIPDatabase) {
        if (!m_geoIPDatabase || (geoIPDatabase->buildEpoch() > m_geoIPDatabase->buildEpoch())) {
            if (m_geoIPDatabase)
                delete m_geoIPDatabase;
            m_geoIPDatabase = geoIPDatabase;
            Logger::instance()->addMessage(tr("GeoIP database loaded. Type: %1. Build time: %2.")
                                           .arg(m_geoIPDatabase->type()).arg(m_geoIPDatabase->buildEpoch().toString()),
                                           Log::INFO);
            QString targetPath = Utils::Fs::expandPathAbs(
                        Utils::Fs::QDesktopServicesDataLocation() + GEOIP_FOLDER);
            if (!QDir(targetPath).exists())
                QDir().mkpath(targetPath);
            QFile targetFile(QString("%1/%2").arg(targetPath).arg(GEOIP_FILENAME));
            if (!targetFile.open(QFile::WriteOnly) || (targetFile.write(data) == -1)) {
                Logger::instance()->addMessage(
                            tr("Couldn't save downloaded GeoIP database file."), Log::WARNING);
            }
            else {
                Logger::instance()->addMessage(tr("Successfully updated GeoIP database."), Log::INFO);
            }
        }
        else {
            delete geoIPDatabase;
        }
    }
    else {
        Logger::instance()->addMessage(tr("Couldn't load GeoIP database. Reason: %1").arg(error), Log::WARNING);
    }
}

void GeoIPManager::downloadFailed(const QString &url, const QString &reason)
{
    Q_UNUSED(url);
    Logger::instance()->addMessage(tr("Couldn't download GeoIP database file. Reason: %1").arg(reason), Log::WARNING);
}
