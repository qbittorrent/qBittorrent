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
        countries[QString()] = tr("N/A");
        countries["AP"] = tr("Asia/Pacific Region");
        countries["EU"] = tr("Europe");
        countries["AD"] = tr("Andorra");
        countries["AE"] = tr("United Arab Emirates");
        countries["AF"] = tr("Afghanistan");
        countries["AG"] = tr("Antigua and Barbuda");
        countries["AI"] = tr("Anguilla");
        countries["AL"] = tr("Albania");
        countries["AM"] = tr("Armenia");
        countries["AN"] = tr("Netherlands Antilles");
        countries["AO"] = tr("Angola");
        countries["AQ"] = tr("Antarctica");
        countries["AR"] = tr("Argentina");
        countries["AS"] = tr("American Samoa");
        countries["AT"] = tr("Austria");
        countries["AU"] = tr("Australia");
        countries["AW"] = tr("Aruba");
        countries["AZ"] = tr("Azerbaijan");
        countries["BA"] = tr("Bosnia and Herzegovina");
        countries["BB"] = tr("Barbados");
        countries["BD"] = tr("Bangladesh");
        countries["BE"] = tr("Belgium");
        countries["BF"] = tr("Burkina Faso");
        countries["BG"] = tr("Bulgaria");
        countries["BH"] = tr("Bahrain");
        countries["BI"] = tr("Burundi");
        countries["BJ"] = tr("Benin");
        countries["BM"] = tr("Bermuda");
        countries["BN"] = tr("Brunei Darussalam");
        countries["BO"] = tr("Bolivia");
        countries["BR"] = tr("Brazil");
        countries["BS"] = tr("Bahamas");
        countries["BT"] = tr("Bhutan");
        countries["BV"] = tr("Bouvet Island");
        countries["BW"] = tr("Botswana");
        countries["BY"] = tr("Belarus");
        countries["BZ"] = tr("Belize");
        countries["CA"] = tr("Canada");
        countries["CC"] = tr("Cocos (Keeling) Islands");
        countries["CD"] = tr("Congo, The Democratic Republic of the");
        countries["CF"] = tr("Central African Republic");
        countries["CG"] = tr("Congo");
        countries["CH"] = tr("Switzerland");
        countries["CI"] = tr("Cote D'Ivoire");
        countries["CK"] = tr("Cook Islands");
        countries["CL"] = tr("Chile");
        countries["CM"] = tr("Cameroon");
        countries["CN"] = tr("China");
        countries["CO"] = tr("Colombia");
        countries["CR"] = tr("Costa Rica");
        countries["CU"] = tr("Cuba");
        countries["CV"] = tr("Cape Verde");
        countries["CX"] = tr("Christmas Island");
        countries["CY"] = tr("Cyprus");
        countries["CZ"] = tr("Czech Republic");
        countries["DE"] = tr("Germany");
        countries["DJ"] = tr("Djibouti");
        countries["DK"] = tr("Denmark");
        countries["DM"] = tr("Dominica");
        countries["DO"] = tr("Dominican Republic");
        countries["DZ"] = tr("Algeria");
        countries["EC"] = tr("Ecuador");
        countries["EE"] = tr("Estonia");
        countries["EG"] = tr("Egypt");
        countries["EH"] = tr("Western Sahara");
        countries["ER"] = tr("Eritrea");
        countries["ES"] = tr("Spain");
        countries["ET"] = tr("Ethiopia");
        countries["FI"] = tr("Finland");
        countries["FJ"] = tr("Fiji");
        countries["FK"] = tr("Falkland Islands (Malvinas)");
        countries["FM"] = tr("Micronesia, Federated States of");
        countries["FO"] = tr("Faroe Islands");
        countries["FR"] = tr("France");
        countries["FX"] = tr("France, Metropolitan");
        countries["GA"] = tr("Gabon");
        countries["GB"] = tr("United Kingdom");
        countries["GD"] = tr("Grenada");
        countries["GE"] = tr("Georgia");
        countries["GF"] = tr("French Guiana");
        countries["GH"] = tr("Ghana");
        countries["GI"] = tr("Gibraltar");
        countries["GL"] = tr("Greenland");
        countries["GM"] = tr("Gambia");
        countries["GN"] = tr("Guinea");
        countries["GP"] = tr("Guadeloupe");
        countries["GQ"] = tr("Equatorial Guinea");
        countries["GR"] = tr("Greece");
        countries["GS"] = tr("South Georgia and the South Sandwich Islands");
        countries["GT"] = tr("Guatemala");
        countries["GU"] = tr("Guam");
        countries["GW"] = tr("Guinea-Bissau");
        countries["GY"] = tr("Guyana");
        countries["HK"] = tr("Hong Kong");
        countries["HM"] = tr("Heard Island and McDonald Islands");
        countries["HN"] = tr("Honduras");
        countries["HR"] = tr("Croatia");
        countries["HT"] = tr("Haiti");
        countries["HU"] = tr("Hungary");
        countries["ID"] = tr("Indonesia");
        countries["IE"] = tr("Ireland");
        countries["IL"] = tr("Israel");
        countries["IN"] = tr("India");
        countries["IO"] = tr("British Indian Ocean Territory");
        countries["IQ"] = tr("Iraq");
        countries["IR"] = tr("Iran, Islamic Republic of");
        countries["IS"] = tr("Iceland");
        countries["IT"] = tr("Italy");
        countries["JM"] = tr("Jamaica");
        countries["JO"] = tr("Jordan");
        countries["JP"] = tr("Japan");
        countries["KE"] = tr("Kenya");
        countries["KG"] = tr("Kyrgyzstan");
        countries["KH"] = tr("Cambodia");
        countries["KI"] = tr("Kiribati");
        countries["KM"] = tr("Comoros");
        countries["KN"] = tr("Saint Kitts and Nevis");
        countries["KP"] = tr("Korea, Democratic People's Republic of");
        countries["KR"] = tr("Korea, Republic of");
        countries["KW"] = tr("Kuwait");
        countries["KY"] = tr("Cayman Islands");
        countries["KZ"] = tr("Kazakhstan");
        countries["LA"] = tr("Lao People's Democratic Republic");
        countries["LB"] = tr("Lebanon");
        countries["LC"] = tr("Saint Lucia");
        countries["LI"] = tr("Liechtenstein");
        countries["LK"] = tr("Sri Lanka");
        countries["LR"] = tr("Liberia");
        countries["LS"] = tr("Lesotho");
        countries["LT"] = tr("Lithuania");
        countries["LU"] = tr("Luxembourg");
        countries["LV"] = tr("Latvia");
        countries["LY"] = tr("Libyan Arab Jamahiriya");
        countries["MA"] = tr("Morocco");
        countries["MC"] = tr("Monaco");
        countries["MD"] = tr("Moldova, Republic of");
        countries["MG"] = tr("Madagascar");
        countries["MH"] = tr("Marshall Islands");
        countries["MK"] = tr("Macedonia");
        countries["ML"] = tr("Mali");
        countries["MM"] = tr("Myanmar");
        countries["MN"] = tr("Mongolia");
        countries["MO"] = tr("Macau");
        countries["MP"] = tr("Northern Mariana Islands");
        countries["MQ"] = tr("Martinique");
        countries["MR"] = tr("Mauritania");
        countries["MS"] = tr("Montserrat");
        countries["MT"] = tr("Malta");
        countries["MU"] = tr("Mauritius");
        countries["MV"] = tr("Maldives");
        countries["MW"] = tr("Malawi");
        countries["MX"] = tr("Mexico");
        countries["MY"] = tr("Malaysia");
        countries["MZ"] = tr("Mozambique");
        countries["NA"] = tr("Namibia");
        countries["NC"] = tr("New Caledonia");
        countries["NE"] = tr("Niger");
        countries["NF"] = tr("Norfolk Island");
        countries["NG"] = tr("Nigeria");
        countries["NI"] = tr("Nicaragua");
        countries["NL"] = tr("Netherlands");
        countries["NO"] = tr("Norway");
        countries["NP"] = tr("Nepal");
        countries["NR"] = tr("Nauru");
        countries["NU"] = tr("Niue");
        countries["NZ"] = tr("New Zealand");
        countries["OM"] = tr("Oman");
        countries["PA"] = tr("Panama");
        countries["PE"] = tr("Peru");
        countries["PF"] = tr("French Polynesia");
        countries["PG"] = tr("Papua New Guinea");
        countries["PH"] = tr("Philippines");
        countries["PK"] = tr("Pakistan");
        countries["PL"] = tr("Poland");
        countries["PM"] = tr("Saint Pierre and Miquelon");
        countries["PN"] = tr("Pitcairn Islands");
        countries["PR"] = tr("Puerto Rico");
        countries["PS"] = tr("Palestinian Territory");
        countries["PT"] = tr("Portugal");
        countries["PW"] = tr("Palau");
        countries["PY"] = tr("Paraguay");
        countries["QA"] = tr("Qatar");
        countries["RE"] = tr("Reunion");
        countries["RO"] = tr("Romania");
        countries["RU"] = tr("Russian Federation");
        countries["RW"] = tr("Rwanda");
        countries["SA"] = tr("Saudi Arabia");
        countries["SB"] = tr("Solomon Islands");
        countries["SC"] = tr("Seychelles");
        countries["SD"] = tr("Sudan");
        countries["SE"] = tr("Sweden");
        countries["SG"] = tr("Singapore");
        countries["SH"] = tr("Saint Helena");
        countries["SI"] = tr("Slovenia");
        countries["SJ"] = tr("Svalbard and Jan Mayen");
        countries["SK"] = tr("Slovakia");
        countries["SL"] = tr("Sierra Leone");
        countries["SM"] = tr("San Marino");
        countries["SN"] = tr("Senegal");
        countries["SO"] = tr("Somalia");
        countries["SR"] = tr("Suriname");
        countries["ST"] = tr("Sao Tome and Principe");
        countries["SV"] = tr("El Salvador");
        countries["SY"] = tr("Syrian Arab Republic");
        countries["SZ"] = tr("Swaziland");
        countries["TC"] = tr("Turks and Caicos Islands");
        countries["TD"] = tr("Chad");
        countries["TF"] = tr("French Southern Territories");
        countries["TG"] = tr("Togo");
        countries["TH"] = tr("Thailand");
        countries["TJ"] = tr("Tajikistan");
        countries["TK"] = tr("Tokelau");
        countries["TM"] = tr("Turkmenistan");
        countries["TN"] = tr("Tunisia");
        countries["TO"] = tr("Tonga");
        countries["TL"] = tr("Timor-Leste");
        countries["TR"] = tr("Turkey");
        countries["TT"] = tr("Trinidad and Tobago");
        countries["TV"] = tr("Tuvalu");
        countries["TW"] = tr("Taiwan");
        countries["TZ"] = tr("Tanzania, United Republic of");
        countries["UA"] = tr("Ukraine");
        countries["UG"] = tr("Uganda");
        countries["UM"] = tr("United States Minor Outlying Islands");
        countries["US"] = tr("United States");
        countries["UY"] = tr("Uruguay");
        countries["UZ"] = tr("Uzbekistan");
        countries["VA"] = tr("Holy See (Vatican City State)");
        countries["VC"] = tr("Saint Vincent and the Grenadines");
        countries["VE"] = tr("Venezuela");
        countries["VG"] = tr("Virgin Islands, British");
        countries["VI"] = tr("Virgin Islands, U.S.");
        countries["VN"] = tr("Vietnam");
        countries["VU"] = tr("Vanuatu");
        countries["WF"] = tr("Wallis and Futuna");
        countries["WS"] = tr("Samoa");
        countries["YE"] = tr("Yemen");
        countries["YT"] = tr("Mayotte");
        countries["RS"] = tr("Serbia");
        countries["ZA"] = tr("South Africa");
        countries["ZM"] = tr("Zambia");
        countries["ME"] = tr("Montenegro");
        countries["ZW"] = tr("Zimbabwe");
        countries["A1"] = tr("Anonymous Proxy");
        countries["A2"] = tr("Satellite Provider");
        countries["O1"] = tr("Other");
        countries["AX"] = tr("Aland Islands");
        countries["GG"] = tr("Guernsey");
        countries["IM"] = tr("Isle of Man");
        countries["JE"] = tr("Jersey");
        countries["BL"] = tr("Saint Barthelemy");
        countries["MF"] = tr("Saint Martin");

        initialized = true;
    }

    return countries.value(countryISOCode, tr("N/A"));
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
