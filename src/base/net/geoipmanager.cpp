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

const QString DATABASE_URL = u"https://download.db-ip.com/free/dbip-country-lite-%1.mmdb.gz"_qs;
const QString GEODB_FOLDER = u"GeoDB"_qs;
const QString GEODB_FILENAME = u"dbip-country-lite.mmdb"_qs;

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
    DownloadManager::instance()->download({curUrl}, this, &GeoIPManager::downloadFinished);
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
        {u"AD"_qs, tr("Andorra")},
        {u"AE"_qs, tr("United Arab Emirates")},
        {u"AF"_qs, tr("Afghanistan")},
        {u"AG"_qs, tr("Antigua and Barbuda")},
        {u"AI"_qs, tr("Anguilla")},
        {u"AL"_qs, tr("Albania")},
        {u"AM"_qs, tr("Armenia")},
        {u"AO"_qs, tr("Angola")},
        {u"AQ"_qs, tr("Antarctica")},
        {u"AR"_qs, tr("Argentina")},
        {u"AS"_qs, tr("American Samoa")},
        {u"AT"_qs, tr("Austria")},
        {u"AU"_qs, tr("Australia")},
        {u"AW"_qs, tr("Aruba")},
        {u"AX"_qs, tr("Aland Islands")},
        {u"AZ"_qs, tr("Azerbaijan")},
        {u"BA"_qs, tr("Bosnia and Herzegovina")},
        {u"BB"_qs, tr("Barbados")},
        {u"BD"_qs, tr("Bangladesh")},
        {u"BE"_qs, tr("Belgium")},
        {u"BF"_qs, tr("Burkina Faso")},
        {u"BG"_qs, tr("Bulgaria")},
        {u"BH"_qs, tr("Bahrain")},
        {u"BI"_qs, tr("Burundi")},
        {u"BJ"_qs, tr("Benin")},
        {u"BL"_qs, tr("Saint Barthelemy")},
        {u"BM"_qs, tr("Bermuda")},
        {u"BN"_qs, tr("Brunei Darussalam")},
        {u"BO"_qs, tr("Bolivia, Plurinational State of")},
        {u"BQ"_qs, tr("Bonaire, Sint Eustatius and Saba")},
        {u"BR"_qs, tr("Brazil")},
        {u"BS"_qs, tr("Bahamas")},
        {u"BT"_qs, tr("Bhutan")},
        {u"BV"_qs, tr("Bouvet Island")},
        {u"BW"_qs, tr("Botswana")},
        {u"BY"_qs, tr("Belarus")},
        {u"BZ"_qs, tr("Belize")},
        {u"CA"_qs, tr("Canada")},
        {u"CC"_qs, tr("Cocos (Keeling) Islands")},
        {u"CD"_qs, tr("Congo, The Democratic Republic of the")},
        {u"CF"_qs, tr("Central African Republic")},
        {u"CG"_qs, tr("Congo")},
        {u"CH"_qs, tr("Switzerland")},
        {u"CI"_qs, tr("Cote d'Ivoire")},
        {u"CK"_qs, tr("Cook Islands")},
        {u"CL"_qs, tr("Chile")},
        {u"CM"_qs, tr("Cameroon")},
        {u"CN"_qs, tr("China")},
        {u"CO"_qs, tr("Colombia")},
        {u"CR"_qs, tr("Costa Rica")},
        {u"CU"_qs, tr("Cuba")},
        {u"CV"_qs, tr("Cape Verde")},
        {u"CW"_qs, tr("Curacao")},
        {u"CX"_qs, tr("Christmas Island")},
        {u"CY"_qs, tr("Cyprus")},
        {u"CZ"_qs, tr("Czech Republic")},
        {u"DE"_qs, tr("Germany")},
        {u"DJ"_qs, tr("Djibouti")},
        {u"DK"_qs, tr("Denmark")},
        {u"DM"_qs, tr("Dominica")},
        {u"DO"_qs, tr("Dominican Republic")},
        {u"DZ"_qs, tr("Algeria")},
        {u"EC"_qs, tr("Ecuador")},
        {u"EE"_qs, tr("Estonia")},
        {u"EG"_qs, tr("Egypt")},
        {u"EH"_qs, tr("Western Sahara")},
        {u"ER"_qs, tr("Eritrea")},
        {u"ES"_qs, tr("Spain")},
        {u"ET"_qs, tr("Ethiopia")},
        {u"FI"_qs, tr("Finland")},
        {u"FJ"_qs, tr("Fiji")},
        {u"FK"_qs, tr("Falkland Islands (Malvinas)")},
        {u"FM"_qs, tr("Micronesia, Federated States of")},
        {u"FO"_qs, tr("Faroe Islands")},
        {u"FR"_qs, tr("France")},
        {u"GA"_qs, tr("Gabon")},
        {u"GB"_qs, tr("United Kingdom")},
        {u"GD"_qs, tr("Grenada")},
        {u"GE"_qs, tr("Georgia")},
        {u"GF"_qs, tr("French Guiana")},
        {u"GG"_qs, tr("Guernsey")},
        {u"GH"_qs, tr("Ghana")},
        {u"GI"_qs, tr("Gibraltar")},
        {u"GL"_qs, tr("Greenland")},
        {u"GM"_qs, tr("Gambia")},
        {u"GN"_qs, tr("Guinea")},
        {u"GP"_qs, tr("Guadeloupe")},
        {u"GQ"_qs, tr("Equatorial Guinea")},
        {u"GR"_qs, tr("Greece")},
        {u"GS"_qs, tr("South Georgia and the South Sandwich Islands")},
        {u"GT"_qs, tr("Guatemala")},
        {u"GU"_qs, tr("Guam")},
        {u"GW"_qs, tr("Guinea-Bissau")},
        {u"GY"_qs, tr("Guyana")},
        {u"HK"_qs, tr("Hong Kong")},
        {u"HM"_qs, tr("Heard Island and McDonald Islands")},
        {u"HN"_qs, tr("Honduras")},
        {u"HR"_qs, tr("Croatia")},
        {u"HT"_qs, tr("Haiti")},
        {u"HU"_qs, tr("Hungary")},
        {u"ID"_qs, tr("Indonesia")},
        {u"IE"_qs, tr("Ireland")},
        {u"IL"_qs, tr("Israel")},
        {u"IM"_qs, tr("Isle of Man")},
        {u"IN"_qs, tr("India")},
        {u"IO"_qs, tr("British Indian Ocean Territory")},
        {u"IQ"_qs, tr("Iraq")},
        {u"IR"_qs, tr("Iran, Islamic Republic of")},
        {u"IS"_qs, tr("Iceland")},
        {u"IT"_qs, tr("Italy")},
        {u"JE"_qs, tr("Jersey")},
        {u"JM"_qs, tr("Jamaica")},
        {u"JO"_qs, tr("Jordan")},
        {u"JP"_qs, tr("Japan")},
        {u"KE"_qs, tr("Kenya")},
        {u"KG"_qs, tr("Kyrgyzstan")},
        {u"KH"_qs, tr("Cambodia")},
        {u"KI"_qs, tr("Kiribati")},
        {u"KM"_qs, tr("Comoros")},
        {u"KN"_qs, tr("Saint Kitts and Nevis")},
        {u"KP"_qs, tr("Korea, Democratic People's Republic of")},
        {u"KR"_qs, tr("Korea, Republic of")},
        {u"KW"_qs, tr("Kuwait")},
        {u"KY"_qs, tr("Cayman Islands")},
        {u"KZ"_qs, tr("Kazakhstan")},
        {u"LA"_qs, tr("Lao People's Democratic Republic")},
        {u"LB"_qs, tr("Lebanon")},
        {u"LC"_qs, tr("Saint Lucia")},
        {u"LI"_qs, tr("Liechtenstein")},
        {u"LK"_qs, tr("Sri Lanka")},
        {u"LR"_qs, tr("Liberia")},
        {u"LS"_qs, tr("Lesotho")},
        {u"LT"_qs, tr("Lithuania")},
        {u"LU"_qs, tr("Luxembourg")},
        {u"LV"_qs, tr("Latvia")},
        {u"LY"_qs, tr("Libya")},
        {u"MA"_qs, tr("Morocco")},
        {u"MC"_qs, tr("Monaco")},
        {u"MD"_qs, tr("Moldova, Republic of")},
        {u"ME"_qs, tr("Montenegro")},
        {u"MF"_qs, tr("Saint Martin (French part)")},
        {u"MG"_qs, tr("Madagascar")},
        {u"MH"_qs, tr("Marshall Islands")},
        {u"MK"_qs, tr("Macedonia, The Former Yugoslav Republic of")},
        {u"ML"_qs, tr("Mali")},
        {u"MM"_qs, tr("Myanmar")},
        {u"MN"_qs, tr("Mongolia")},
        {u"MO"_qs, tr("Macao")},
        {u"MP"_qs, tr("Northern Mariana Islands")},
        {u"MQ"_qs, tr("Martinique")},
        {u"MR"_qs, tr("Mauritania")},
        {u"MS"_qs, tr("Montserrat")},
        {u"MT"_qs, tr("Malta")},
        {u"MU"_qs, tr("Mauritius")},
        {u"MV"_qs, tr("Maldives")},
        {u"MW"_qs, tr("Malawi")},
        {u"MX"_qs, tr("Mexico")},
        {u"MY"_qs, tr("Malaysia")},
        {u"MZ"_qs, tr("Mozambique")},
        {u"NA"_qs, tr("Namibia")},
        {u"NC"_qs, tr("New Caledonia")},
        {u"NE"_qs, tr("Niger")},
        {u"NF"_qs, tr("Norfolk Island")},
        {u"NG"_qs, tr("Nigeria")},
        {u"NI"_qs, tr("Nicaragua")},
        {u"NL"_qs, tr("Netherlands")},
        {u"NO"_qs, tr("Norway")},
        {u"NP"_qs, tr("Nepal")},
        {u"NR"_qs, tr("Nauru")},
        {u"NU"_qs, tr("Niue")},
        {u"NZ"_qs, tr("New Zealand")},
        {u"OM"_qs, tr("Oman")},
        {u"PA"_qs, tr("Panama")},
        {u"PE"_qs, tr("Peru")},
        {u"PF"_qs, tr("French Polynesia")},
        {u"PG"_qs, tr("Papua New Guinea")},
        {u"PH"_qs, tr("Philippines")},
        {u"PK"_qs, tr("Pakistan")},
        {u"PL"_qs, tr("Poland")},
        {u"PM"_qs, tr("Saint Pierre and Miquelon")},
        {u"PN"_qs, tr("Pitcairn")},
        {u"PR"_qs, tr("Puerto Rico")},
        {u"PS"_qs, tr("Palestine, State of")},
        {u"PT"_qs, tr("Portugal")},
        {u"PW"_qs, tr("Palau")},
        {u"PY"_qs, tr("Paraguay")},
        {u"QA"_qs, tr("Qatar")},
        {u"RE"_qs, tr("Reunion")},
        {u"RO"_qs, tr("Romania")},
        {u"RS"_qs, tr("Serbia")},
        {u"RU"_qs, tr("Russian Federation")},
        {u"RW"_qs, tr("Rwanda")},
        {u"SA"_qs, tr("Saudi Arabia")},
        {u"SB"_qs, tr("Solomon Islands")},
        {u"SC"_qs, tr("Seychelles")},
        {u"SD"_qs, tr("Sudan")},
        {u"SE"_qs, tr("Sweden")},
        {u"SG"_qs, tr("Singapore")},
        {u"SH"_qs, tr("Saint Helena, Ascension and Tristan da Cunha")},
        {u"SI"_qs, tr("Slovenia")},
        {u"SJ"_qs, tr("Svalbard and Jan Mayen")},
        {u"SK"_qs, tr("Slovakia")},
        {u"SL"_qs, tr("Sierra Leone")},
        {u"SM"_qs, tr("San Marino")},
        {u"SN"_qs, tr("Senegal")},
        {u"SO"_qs, tr("Somalia")},
        {u"SR"_qs, tr("Suriname")},
        {u"SS"_qs, tr("South Sudan")},
        {u"ST"_qs, tr("Sao Tome and Principe")},
        {u"SV"_qs, tr("El Salvador")},
        {u"SX"_qs, tr("Sint Maarten (Dutch part)")},
        {u"SY"_qs, tr("Syrian Arab Republic")},
        {u"SZ"_qs, tr("Swaziland")},
        {u"TC"_qs, tr("Turks and Caicos Islands")},
        {u"TD"_qs, tr("Chad")},
        {u"TF"_qs, tr("French Southern Territories")},
        {u"TG"_qs, tr("Togo")},
        {u"TH"_qs, tr("Thailand")},
        {u"TJ"_qs, tr("Tajikistan")},
        {u"TK"_qs, tr("Tokelau")},
        {u"TL"_qs, tr("Timor-Leste")},
        {u"TM"_qs, tr("Turkmenistan")},
        {u"TN"_qs, tr("Tunisia")},
        {u"TO"_qs, tr("Tonga")},
        {u"TR"_qs, tr("Turkey")},
        {u"TT"_qs, tr("Trinidad and Tobago")},
        {u"TV"_qs, tr("Tuvalu")},
        {u"TW"_qs, tr("Taiwan")},
        {u"TZ"_qs, tr("Tanzania, United Republic of")},
        {u"UA"_qs, tr("Ukraine")},
        {u"UG"_qs, tr("Uganda")},
        {u"UM"_qs, tr("United States Minor Outlying Islands")},
        {u"US"_qs, tr("United States")},
        {u"UY"_qs, tr("Uruguay")},
        {u"UZ"_qs, tr("Uzbekistan")},
        {u"VA"_qs, tr("Holy See (Vatican City State)")},
        {u"VC"_qs, tr("Saint Vincent and the Grenadines")},
        {u"VE"_qs, tr("Venezuela, Bolivarian Republic of")},
        {u"VG"_qs, tr("Virgin Islands, British")},
        {u"VI"_qs, tr("Virgin Islands, U.S.")},
        {u"VN"_qs, tr("Vietnam")},
        {u"VU"_qs, tr("Vanuatu")},
        {u"WF"_qs, tr("Wallis and Futuna")},
        {u"WS"_qs, tr("Samoa")},
        {u"YE"_qs, tr("Yemen")},
        {u"YT"_qs, tr("Mayotte")},
        {u"ZA"_qs, tr("South Africa")},
        {u"ZM"_qs, tr("Zambia")},
        {u"ZW"_qs, tr("Zimbabwe")},

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
            const nonstd::expected<void, QString> result = Utils::IO::saveToFile(path, data);
            if (result)
            {
                LogMsg(tr("Successfully updated IP geolocation database."), Log::INFO);
            }
            else
            {
                LogMsg(tr("Couldn't save downloaded IP geolocation database file. Reason: %1")
                    .arg(result.error()), Log::WARNING);
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
