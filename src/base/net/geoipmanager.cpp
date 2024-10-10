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

#include "geoipmanager.h"

#include <QDateTime>
#include <QHostAddress>
#include <QLocale>

#include "base/global.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/utils/fs.h"
#include "base/utils/gzip.h"
#include "base/utils/io.h"
#include "downloadmanager.h"
#include "geoipdatabase.h"

const QString DATABASE_URL = u"https://download.db-ip.com/free/dbip-country-lite-%1.mmdb.gz"_s;
const QString GEODB_FOLDER = u"GeoDB"_s;
const QString GEODB_FILENAME = u"dbip-country-lite.mmdb"_s;

using namespace Net;

// GeoIPManager

GeoIPManager *GeoIPManager::m_instance = nullptr;

GeoIPManager::GeoIPManager()
{
    configure();
    connect(Preferences::instance(), &Preferences::changed, this, &GeoIPManager::configure);
}

GeoIPManager::~GeoIPManager()
{
    delete m_geoIPDatabase;
}

void GeoIPManager::initInstance()
{
    if (!m_instance)
        m_instance = new GeoIPManager;
}

void GeoIPManager::freeInstance()
{
    delete m_instance;
    m_instance = nullptr;
}

GeoIPManager *GeoIPManager::instance()
{
    return m_instance;
}

void GeoIPManager::loadDatabase()
{
    delete m_geoIPDatabase;
    m_geoIPDatabase = nullptr;

    const Path filepath = specialFolderLocation(SpecialFolder::Data)
            / Path(GEODB_FOLDER) / Path(GEODB_FILENAME);

    QString error;
    m_geoIPDatabase = GeoIPDatabase::load(filepath, error);
    if (m_geoIPDatabase)
    {
        LogMsg(tr("IP geolocation database loaded. Type: %1. Build time: %2.")
                                       .arg(m_geoIPDatabase->type(), m_geoIPDatabase->buildEpoch().toString()),
                                       Log::INFO);
    }
    else
    {
        LogMsg(tr("Couldn't load IP geolocation database. Reason: %1").arg(error), Log::WARNING);
    }

    manageDatabaseUpdate();
}

void GeoIPManager::manageDatabaseUpdate()
{
    const auto expired = [](const QDateTime &testDateTime)
    {
        const QDate testDate = testDateTime.date();
        const QDate curDate = QDateTime::currentDateTimeUtc().date();

        if ((testDate.year() < curDate.year()) && (curDate.day() > 1))
            return true;

        if ((testDate.month() < curDate.month()) && (curDate.day() > 1))
            return true;

        return false;
    };

    if (!m_geoIPDatabase || expired(m_geoIPDatabase->buildEpoch()))
        downloadDatabaseFile();
}

void GeoIPManager::downloadDatabaseFile()
{
    const QDateTime curDatetime = QDateTime::currentDateTimeUtc();
    const QString curUrl = DATABASE_URL.arg(QLocale::c().toString(curDatetime, u"yyyy-MM"));
    DownloadManager::instance()->download(
            {curUrl}, Preferences::instance()->useProxyForGeneralPurposes()
            , this, &GeoIPManager::downloadFinished);
}

QString GeoIPManager::lookup(const QHostAddress &hostAddr) const
{
    if (m_enabled && m_geoIPDatabase)
        return m_geoIPDatabase->lookup(hostAddr);

    return {};
}

QString GeoIPManager::CountryName(const QString &countryISOCode)
{
    static const QHash<QString, QString> countries =
    {
        // ISO 3166-1 alpha-2 codes
        // http://www.iso.org/iso/home/standards/country_codes/country_names_and_code_elements_txt-temp.htm

        // Officially assigned
        {u"AD"_s, tr("Andorra")},
        {u"AE"_s, tr("United Arab Emirates")},
        {u"AF"_s, tr("Afghanistan")},
        {u"AG"_s, tr("Antigua and Barbuda")},
        {u"AI"_s, tr("Anguilla")},
        {u"AL"_s, tr("Albania")},
        {u"AM"_s, tr("Armenia")},
        {u"AO"_s, tr("Angola")},
        {u"AQ"_s, tr("Antarctica")},
        {u"AR"_s, tr("Argentina")},
        {u"AS"_s, tr("American Samoa")},
        {u"AT"_s, tr("Austria")},
        {u"AU"_s, tr("Australia")},
        {u"AW"_s, tr("Aruba")},
        {u"AX"_s, tr("Aland Islands")},
        {u"AZ"_s, tr("Azerbaijan")},
        {u"BA"_s, tr("Bosnia and Herzegovina")},
        {u"BB"_s, tr("Barbados")},
        {u"BD"_s, tr("Bangladesh")},
        {u"BE"_s, tr("Belgium")},
        {u"BF"_s, tr("Burkina Faso")},
        {u"BG"_s, tr("Bulgaria")},
        {u"BH"_s, tr("Bahrain")},
        {u"BI"_s, tr("Burundi")},
        {u"BJ"_s, tr("Benin")},
        {u"BL"_s, tr("Saint Barthelemy")},
        {u"BM"_s, tr("Bermuda")},
        {u"BN"_s, tr("Brunei Darussalam")},
        {u"BO"_s, tr("Bolivia, Plurinational State of")},
        {u"BQ"_s, tr("Bonaire, Sint Eustatius and Saba")},
        {u"BR"_s, tr("Brazil")},
        {u"BS"_s, tr("Bahamas")},
        {u"BT"_s, tr("Bhutan")},
        {u"BV"_s, tr("Bouvet Island")},
        {u"BW"_s, tr("Botswana")},
        {u"BY"_s, tr("Belarus")},
        {u"BZ"_s, tr("Belize")},
        {u"CA"_s, tr("Canada")},
        {u"CC"_s, tr("Cocos (Keeling) Islands")},
        {u"CD"_s, tr("Congo, The Democratic Republic of the")},
        {u"CF"_s, tr("Central African Republic")},
        {u"CG"_s, tr("Congo")},
        {u"CH"_s, tr("Switzerland")},
        {u"CI"_s, tr("Cote d'Ivoire")},
        {u"CK"_s, tr("Cook Islands")},
        {u"CL"_s, tr("Chile")},
        {u"CM"_s, tr("Cameroon")},
        {u"CN"_s, tr("China")},
        {u"CO"_s, tr("Colombia")},
        {u"CR"_s, tr("Costa Rica")},
        {u"CU"_s, tr("Cuba")},
        {u"CV"_s, tr("Cape Verde")},
        {u"CW"_s, tr("Curacao")},
        {u"CX"_s, tr("Christmas Island")},
        {u"CY"_s, tr("Cyprus")},
        {u"CZ"_s, tr("Czech Republic")},
        {u"DE"_s, tr("Germany")},
        {u"DJ"_s, tr("Djibouti")},
        {u"DK"_s, tr("Denmark")},
        {u"DM"_s, tr("Dominica")},
        {u"DO"_s, tr("Dominican Republic")},
        {u"DZ"_s, tr("Algeria")},
        {u"EC"_s, tr("Ecuador")},
        {u"EE"_s, tr("Estonia")},
        {u"EG"_s, tr("Egypt")},
        {u"EH"_s, tr("Western Sahara")},
        {u"ER"_s, tr("Eritrea")},
        {u"ES"_s, tr("Spain")},
        {u"ET"_s, tr("Ethiopia")},
        {u"FI"_s, tr("Finland")},
        {u"FJ"_s, tr("Fiji")},
        {u"FK"_s, tr("Falkland Islands (Malvinas)")},
        {u"FM"_s, tr("Micronesia, Federated States of")},
        {u"FO"_s, tr("Faroe Islands")},
        {u"FR"_s, tr("France")},
        {u"GA"_s, tr("Gabon")},
        {u"GB"_s, tr("United Kingdom")},
        {u"GD"_s, tr("Grenada")},
        {u"GE"_s, tr("Georgia")},
        {u"GF"_s, tr("French Guiana")},
        {u"GG"_s, tr("Guernsey")},
        {u"GH"_s, tr("Ghana")},
        {u"GI"_s, tr("Gibraltar")},
        {u"GL"_s, tr("Greenland")},
        {u"GM"_s, tr("Gambia")},
        {u"GN"_s, tr("Guinea")},
        {u"GP"_s, tr("Guadeloupe")},
        {u"GQ"_s, tr("Equatorial Guinea")},
        {u"GR"_s, tr("Greece")},
        {u"GS"_s, tr("South Georgia and the South Sandwich Islands")},
        {u"GT"_s, tr("Guatemala")},
        {u"GU"_s, tr("Guam")},
        {u"GW"_s, tr("Guinea-Bissau")},
        {u"GY"_s, tr("Guyana")},
        {u"HK"_s, tr("Hong Kong")},
        {u"HM"_s, tr("Heard Island and McDonald Islands")},
        {u"HN"_s, tr("Honduras")},
        {u"HR"_s, tr("Croatia")},
        {u"HT"_s, tr("Haiti")},
        {u"HU"_s, tr("Hungary")},
        {u"ID"_s, tr("Indonesia")},
        {u"IE"_s, tr("Ireland")},
        {u"IL"_s, tr("Israel")},
        {u"IM"_s, tr("Isle of Man")},
        {u"IN"_s, tr("India")},
        {u"IO"_s, tr("British Indian Ocean Territory")},
        {u"IQ"_s, tr("Iraq")},
        {u"IR"_s, tr("Iran, Islamic Republic of")},
        {u"IS"_s, tr("Iceland")},
        {u"IT"_s, tr("Italy")},
        {u"JE"_s, tr("Jersey")},
        {u"JM"_s, tr("Jamaica")},
        {u"JO"_s, tr("Jordan")},
        {u"JP"_s, tr("Japan")},
        {u"KE"_s, tr("Kenya")},
        {u"KG"_s, tr("Kyrgyzstan")},
        {u"KH"_s, tr("Cambodia")},
        {u"KI"_s, tr("Kiribati")},
        {u"KM"_s, tr("Comoros")},
        {u"KN"_s, tr("Saint Kitts and Nevis")},
        {u"KP"_s, tr("Korea, Democratic People's Republic of")},
        {u"KR"_s, tr("Korea, Republic of")},
        {u"KW"_s, tr("Kuwait")},
        {u"KY"_s, tr("Cayman Islands")},
        {u"KZ"_s, tr("Kazakhstan")},
        {u"LA"_s, tr("Lao People's Democratic Republic")},
        {u"LB"_s, tr("Lebanon")},
        {u"LC"_s, tr("Saint Lucia")},
        {u"LI"_s, tr("Liechtenstein")},
        {u"LK"_s, tr("Sri Lanka")},
        {u"LR"_s, tr("Liberia")},
        {u"LS"_s, tr("Lesotho")},
        {u"LT"_s, tr("Lithuania")},
        {u"LU"_s, tr("Luxembourg")},
        {u"LV"_s, tr("Latvia")},
        {u"LY"_s, tr("Libya")},
        {u"MA"_s, tr("Morocco")},
        {u"MC"_s, tr("Monaco")},
        {u"MD"_s, tr("Moldova, Republic of")},
        {u"ME"_s, tr("Montenegro")},
        {u"MF"_s, tr("Saint Martin (French part)")},
        {u"MG"_s, tr("Madagascar")},
        {u"MH"_s, tr("Marshall Islands")},
        {u"MK"_s, tr("Macedonia, The Former Yugoslav Republic of")},
        {u"ML"_s, tr("Mali")},
        {u"MM"_s, tr("Myanmar")},
        {u"MN"_s, tr("Mongolia")},
        {u"MO"_s, tr("Macao")},
        {u"MP"_s, tr("Northern Mariana Islands")},
        {u"MQ"_s, tr("Martinique")},
        {u"MR"_s, tr("Mauritania")},
        {u"MS"_s, tr("Montserrat")},
        {u"MT"_s, tr("Malta")},
        {u"MU"_s, tr("Mauritius")},
        {u"MV"_s, tr("Maldives")},
        {u"MW"_s, tr("Malawi")},
        {u"MX"_s, tr("Mexico")},
        {u"MY"_s, tr("Malaysia")},
        {u"MZ"_s, tr("Mozambique")},
        {u"NA"_s, tr("Namibia")},
        {u"NC"_s, tr("New Caledonia")},
        {u"NE"_s, tr("Niger")},
        {u"NF"_s, tr("Norfolk Island")},
        {u"NG"_s, tr("Nigeria")},
        {u"NI"_s, tr("Nicaragua")},
        {u"NL"_s, tr("Netherlands")},
        {u"NO"_s, tr("Norway")},
        {u"NP"_s, tr("Nepal")},
        {u"NR"_s, tr("Nauru")},
        {u"NU"_s, tr("Niue")},
        {u"NZ"_s, tr("New Zealand")},
        {u"OM"_s, tr("Oman")},
        {u"PA"_s, tr("Panama")},
        {u"PE"_s, tr("Peru")},
        {u"PF"_s, tr("French Polynesia")},
        {u"PG"_s, tr("Papua New Guinea")},
        {u"PH"_s, tr("Philippines")},
        {u"PK"_s, tr("Pakistan")},
        {u"PL"_s, tr("Poland")},
        {u"PM"_s, tr("Saint Pierre and Miquelon")},
        {u"PN"_s, tr("Pitcairn")},
        {u"PR"_s, tr("Puerto Rico")},
        {u"PS"_s, tr("Palestine, State of")},
        {u"PT"_s, tr("Portugal")},
        {u"PW"_s, tr("Palau")},
        {u"PY"_s, tr("Paraguay")},
        {u"QA"_s, tr("Qatar")},
        {u"RE"_s, tr("Reunion")},
        {u"RO"_s, tr("Romania")},
        {u"RS"_s, tr("Serbia")},
        {u"RU"_s, tr("Russian Federation")},
        {u"RW"_s, tr("Rwanda")},
        {u"SA"_s, tr("Saudi Arabia")},
        {u"SB"_s, tr("Solomon Islands")},
        {u"SC"_s, tr("Seychelles")},
        {u"SD"_s, tr("Sudan")},
        {u"SE"_s, tr("Sweden")},
        {u"SG"_s, tr("Singapore")},
        {u"SH"_s, tr("Saint Helena, Ascension and Tristan da Cunha")},
        {u"SI"_s, tr("Slovenia")},
        {u"SJ"_s, tr("Svalbard and Jan Mayen")},
        {u"SK"_s, tr("Slovakia")},
        {u"SL"_s, tr("Sierra Leone")},
        {u"SM"_s, tr("San Marino")},
        {u"SN"_s, tr("Senegal")},
        {u"SO"_s, tr("Somalia")},
        {u"SR"_s, tr("Suriname")},
        {u"SS"_s, tr("South Sudan")},
        {u"ST"_s, tr("Sao Tome and Principe")},
        {u"SV"_s, tr("El Salvador")},
        {u"SX"_s, tr("Sint Maarten (Dutch part)")},
        {u"SY"_s, tr("Syrian Arab Republic")},
        {u"SZ"_s, tr("Swaziland")},
        {u"TC"_s, tr("Turks and Caicos Islands")},
        {u"TD"_s, tr("Chad")},
        {u"TF"_s, tr("French Southern Territories")},
        {u"TG"_s, tr("Togo")},
        {u"TH"_s, tr("Thailand")},
        {u"TJ"_s, tr("Tajikistan")},
        {u"TK"_s, tr("Tokelau")},
        {u"TL"_s, tr("Timor-Leste")},
        {u"TM"_s, tr("Turkmenistan")},
        {u"TN"_s, tr("Tunisia")},
        {u"TO"_s, tr("Tonga")},
        {u"TR"_s, tr("Turkey")},
        {u"TT"_s, tr("Trinidad and Tobago")},
        {u"TV"_s, tr("Tuvalu")},
        {u"TW"_s, tr("Taiwan")},
        {u"TZ"_s, tr("Tanzania, United Republic of")},
        {u"UA"_s, tr("Ukraine")},
        {u"UG"_s, tr("Uganda")},
        {u"UM"_s, tr("United States Minor Outlying Islands")},
        {u"US"_s, tr("United States")},
        {u"UY"_s, tr("Uruguay")},
        {u"UZ"_s, tr("Uzbekistan")},
        {u"VA"_s, tr("Holy See (Vatican City State)")},
        {u"VC"_s, tr("Saint Vincent and the Grenadines")},
        {u"VE"_s, tr("Venezuela, Bolivarian Republic of")},
        {u"VG"_s, tr("Virgin Islands, British")},
        {u"VI"_s, tr("Virgin Islands, U.S.")},
        {u"VN"_s, tr("Vietnam")},
        {u"VU"_s, tr("Vanuatu")},
        {u"WF"_s, tr("Wallis and Futuna")},
        {u"WS"_s, tr("Samoa")},
        {u"YE"_s, tr("Yemen")},
        {u"YT"_s, tr("Mayotte")},
        {u"ZA"_s, tr("South Africa")},
        {u"ZM"_s, tr("Zambia")},
        {u"ZW"_s, tr("Zimbabwe")},

        {{}, tr("N/A")}
    };

    return countries.value(countryISOCode, tr("N/A"));
}

void GeoIPManager::configure()
{
    const bool enabled = Preferences::instance()->resolvePeerCountries();
    if (m_enabled != enabled)
    {
        m_enabled = enabled;
        if (m_enabled && !m_geoIPDatabase)
        {
            loadDatabase();
        }
        else if (!m_enabled)
        {
            delete m_geoIPDatabase;
            m_geoIPDatabase = nullptr;
        }
    }
}

void GeoIPManager::downloadFinished(const DownloadResult &result)
{
    if (result.status != DownloadStatus::Success)
    {
        LogMsg(tr("Couldn't download IP geolocation database file. Reason: %1").arg(result.errorString), Log::WARNING);
        return;
    }

    bool ok = false;
    const QByteArray data = Utils::Gzip::decompress(result.data, &ok);
    if (!ok)
    {
        LogMsg(tr("Could not decompress IP geolocation database file."), Log::WARNING);
        return;
    }

    QString error;
    GeoIPDatabase *geoIPDatabase = GeoIPDatabase::load(data, error);
    if (geoIPDatabase)
    {
        if (!m_geoIPDatabase || (geoIPDatabase->buildEpoch() > m_geoIPDatabase->buildEpoch()))
        {
            delete m_geoIPDatabase;
            m_geoIPDatabase = geoIPDatabase;
            LogMsg(tr("IP geolocation database loaded. Type: %1. Build time: %2.")
                .arg(m_geoIPDatabase->type(), m_geoIPDatabase->buildEpoch().toString())
                   , Log::INFO);
            const Path targetPath = specialFolderLocation(SpecialFolder::Data) / Path(GEODB_FOLDER);
            if (!targetPath.exists())
                Utils::Fs::mkpath(targetPath);

            const auto path = targetPath / Path(GEODB_FILENAME);
            const nonstd::expected<void, QString> saveResult = Utils::IO::saveToFile(path, data);
            if (saveResult)
            {
                LogMsg(tr("Successfully updated IP geolocation database."), Log::INFO);
            }
            else
            {
                LogMsg(tr("Couldn't save downloaded IP geolocation database file. Reason: %1")
                    .arg(saveResult.error()), Log::WARNING);
            }
        }
        else
        {
            delete geoIPDatabase;
        }
    }
    else
    {
        LogMsg(tr("Couldn't load IP geolocation database. Reason: %1").arg(error), Log::WARNING);
    }
}
