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
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QLocale>

#include "base/logger.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/utils/fs.h"
#include "base/utils/gzip.h"
#include "downloadmanager.h"
#include "geoipdatabase.h"

static const QString DATABASE_URL = QStringLiteral("https://download.db-ip.com/free/dbip-country-lite-%1.mmdb.gz");
static const char GEODB_FOLDER[] = "GeoDB";
static const char GEODB_FILENAME[] = "dbip-country-lite.mmdb";

using namespace Net;

// GeoIPManager

GeoIPManager *GeoIPManager::m_instance = nullptr;

GeoIPManager::GeoIPManager()
    : m_enabled(false)
    , m_geoIPDatabase(nullptr)
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

    const QString filepath = Utils::Fs::expandPathAbs(
        QString::fromLatin1("%1%2/%3").arg(specialFolderLocation(SpecialFolder::Data), GEODB_FOLDER, GEODB_FILENAME));

    QString error;
    m_geoIPDatabase = GeoIPDatabase::load(filepath, error);
    if (m_geoIPDatabase)
        Logger::instance()->addMessage(tr("IP geolocation database loaded. Type: %1. Build time: %2.")
            .arg(m_geoIPDatabase->type(), m_geoIPDatabase->buildEpoch().toString()),
            Log::INFO);
    else
        Logger::instance()->addMessage(tr("Couldn't load IP geolocation database. Reason: %1").arg(error), Log::WARNING);

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
    const QString curUrl = DATABASE_URL.arg(QLocale::c().toString(curDatetime, "yyyy-MM"));
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
        {"AD", tr("Andorra")},
        {"AE", tr("United Arab Emirates")},
        {"AF", tr("Afghanistan")},
        {"AG", tr("Antigua and Barbuda")},
        {"AI", tr("Anguilla")},
        {"AL", tr("Albania")},
        {"AM", tr("Armenia")},
        {"AO", tr("Angola")},
        {"AQ", tr("Antarctica")},
        {"AR", tr("Argentina")},
        {"AS", tr("American Samoa")},
        {"AT", tr("Austria")},
        {"AU", tr("Australia")},
        {"AW", tr("Aruba")},
        {"AX", tr("Aland Islands")},
        {"AZ", tr("Azerbaijan")},
        {"BA", tr("Bosnia and Herzegovina")},
        {"BB", tr("Barbados")},
        {"BD", tr("Bangladesh")},
        {"BE", tr("Belgium")},
        {"BF", tr("Burkina Faso")},
        {"BG", tr("Bulgaria")},
        {"BH", tr("Bahrain")},
        {"BI", tr("Burundi")},
        {"BJ", tr("Benin")},
        {"BL", tr("Saint Barthelemy")},
        {"BM", tr("Bermuda")},
        {"BN", tr("Brunei Darussalam")},
        {"BO", tr("Bolivia, Plurinational State of")},
        {"BQ", tr("Bonaire, Sint Eustatius and Saba")},
        {"BR", tr("Brazil")},
        {"BS", tr("Bahamas")},
        {"BT", tr("Bhutan")},
        {"BV", tr("Bouvet Island")},
        {"BW", tr("Botswana")},
        {"BY", tr("Belarus")},
        {"BZ", tr("Belize")},
        {"CA", tr("Canada")},
        {"CC", tr("Cocos (Keeling) Islands")},
        {"CD", tr("Congo, The Democratic Republic of the")},
        {"CF", tr("Central African Republic")},
        {"CG", tr("Congo")},
        {"CH", tr("Switzerland")},
        {"CI", tr("Cote d'Ivoire")},
        {"CK", tr("Cook Islands")},
        {"CL", tr("Chile")},
        {"CM", tr("Cameroon")},
        {"CN", tr("China")},
        {"CO", tr("Colombia")},
        {"CR", tr("Costa Rica")},
        {"CU", tr("Cuba")},
        {"CV", tr("Cape Verde")},
        {"CW", tr("Curacao")},
        {"CX", tr("Christmas Island")},
        {"CY", tr("Cyprus")},
        {"CZ", tr("Czech Republic")},
        {"DE", tr("Germany")},
        {"DJ", tr("Djibouti")},
        {"DK", tr("Denmark")},
        {"DM", tr("Dominica")},
        {"DO", tr("Dominican Republic")},
        {"DZ", tr("Algeria")},
        {"EC", tr("Ecuador")},
        {"EE", tr("Estonia")},
        {"EG", tr("Egypt")},
        {"EH", tr("Western Sahara")},
        {"ER", tr("Eritrea")},
        {"ES", tr("Spain")},
        {"ET", tr("Ethiopia")},
        {"FI", tr("Finland")},
        {"FJ", tr("Fiji")},
        {"FK", tr("Falkland Islands (Malvinas)")},
        {"FM", tr("Micronesia, Federated States of")},
        {"FO", tr("Faroe Islands")},
        {"FR", tr("France")},
        {"GA", tr("Gabon")},
        {"GB", tr("United Kingdom")},
        {"GD", tr("Grenada")},
        {"GE", tr("Georgia")},
        {"GF", tr("French Guiana")},
        {"GG", tr("Guernsey")},
        {"GH", tr("Ghana")},
        {"GI", tr("Gibraltar")},
        {"GL", tr("Greenland")},
        {"GM", tr("Gambia")},
        {"GN", tr("Guinea")},
        {"GP", tr("Guadeloupe")},
        {"GQ", tr("Equatorial Guinea")},
        {"GR", tr("Greece")},
        {"GS", tr("South Georgia and the South Sandwich Islands")},
        {"GT", tr("Guatemala")},
        {"GU", tr("Guam")},
        {"GW", tr("Guinea-Bissau")},
        {"GY", tr("Guyana")},
        {"HK", tr("Hong Kong")},
        {"HM", tr("Heard Island and McDonald Islands")},
        {"HN", tr("Honduras")},
        {"HR", tr("Croatia")},
        {"HT", tr("Haiti")},
        {"HU", tr("Hungary")},
        {"ID", tr("Indonesia")},
        {"IE", tr("Ireland")},
        {"IL", tr("Israel")},
        {"IM", tr("Isle of Man")},
        {"IN", tr("India")},
        {"IO", tr("British Indian Ocean Territory")},
        {"IQ", tr("Iraq")},
        {"IR", tr("Iran, Islamic Republic of")},
        {"IS", tr("Iceland")},
        {"IT", tr("Italy")},
        {"JE", tr("Jersey")},
        {"JM", tr("Jamaica")},
        {"JO", tr("Jordan")},
        {"JP", tr("Japan")},
        {"KE", tr("Kenya")},
        {"KG", tr("Kyrgyzstan")},
        {"KH", tr("Cambodia")},
        {"KI", tr("Kiribati")},
        {"KM", tr("Comoros")},
        {"KN", tr("Saint Kitts and Nevis")},
        {"KP", tr("Korea, Democratic People's Republic of")},
        {"KR", tr("Korea, Republic of")},
        {"KW", tr("Kuwait")},
        {"KY", tr("Cayman Islands")},
        {"KZ", tr("Kazakhstan")},
        {"LA", tr("Lao People's Democratic Republic")},
        {"LB", tr("Lebanon")},
        {"LC", tr("Saint Lucia")},
        {"LI", tr("Liechtenstein")},
        {"LK", tr("Sri Lanka")},
        {"LR", tr("Liberia")},
        {"LS", tr("Lesotho")},
        {"LT", tr("Lithuania")},
        {"LU", tr("Luxembourg")},
        {"LV", tr("Latvia")},
        {"LY", tr("Libya")},
        {"MA", tr("Morocco")},
        {"MC", tr("Monaco")},
        {"MD", tr("Moldova, Republic of")},
        {"ME", tr("Montenegro")},
        {"MF", tr("Saint Martin (French part)")},
        {"MG", tr("Madagascar")},
        {"MH", tr("Marshall Islands")},
        {"MK", tr("Macedonia, The Former Yugoslav Republic of")},
        {"ML", tr("Mali")},
        {"MM", tr("Myanmar")},
        {"MN", tr("Mongolia")},
        {"MO", tr("Macao")},
        {"MP", tr("Northern Mariana Islands")},
        {"MQ", tr("Martinique")},
        {"MR", tr("Mauritania")},
        {"MS", tr("Montserrat")},
        {"MT", tr("Malta")},
        {"MU", tr("Mauritius")},
        {"MV", tr("Maldives")},
        {"MW", tr("Malawi")},
        {"MX", tr("Mexico")},
        {"MY", tr("Malaysia")},
        {"MZ", tr("Mozambique")},
        {"NA", tr("Namibia")},
        {"NC", tr("New Caledonia")},
        {"NE", tr("Niger")},
        {"NF", tr("Norfolk Island")},
        {"NG", tr("Nigeria")},
        {"NI", tr("Nicaragua")},
        {"NL", tr("Netherlands")},
        {"NO", tr("Norway")},
        {"NP", tr("Nepal")},
        {"NR", tr("Nauru")},
        {"NU", tr("Niue")},
        {"NZ", tr("New Zealand")},
        {"OM", tr("Oman")},
        {"PA", tr("Panama")},
        {"PE", tr("Peru")},
        {"PF", tr("French Polynesia")},
        {"PG", tr("Papua New Guinea")},
        {"PH", tr("Philippines")},
        {"PK", tr("Pakistan")},
        {"PL", tr("Poland")},
        {"PM", tr("Saint Pierre and Miquelon")},
        {"PN", tr("Pitcairn")},
        {"PR", tr("Puerto Rico")},
        {"PS", tr("Palestine, State of")},
        {"PT", tr("Portugal")},
        {"PW", tr("Palau")},
        {"PY", tr("Paraguay")},
        {"QA", tr("Qatar")},
        {"RE", tr("Reunion")},
        {"RO", tr("Romania")},
        {"RS", tr("Serbia")},
        {"RU", tr("Russian Federation")},
        {"RW", tr("Rwanda")},
        {"SA", tr("Saudi Arabia")},
        {"SB", tr("Solomon Islands")},
        {"SC", tr("Seychelles")},
        {"SD", tr("Sudan")},
        {"SE", tr("Sweden")},
        {"SG", tr("Singapore")},
        {"SH", tr("Saint Helena, Ascension and Tristan da Cunha")},
        {"SI", tr("Slovenia")},
        {"SJ", tr("Svalbard and Jan Mayen")},
        {"SK", tr("Slovakia")},
        {"SL", tr("Sierra Leone")},
        {"SM", tr("San Marino")},
        {"SN", tr("Senegal")},
        {"SO", tr("Somalia")},
        {"SR", tr("Suriname")},
        {"SS", tr("South Sudan")},
        {"ST", tr("Sao Tome and Principe")},
        {"SV", tr("El Salvador")},
        {"SX", tr("Sint Maarten (Dutch part)")},
        {"SY", tr("Syrian Arab Republic")},
        {"SZ", tr("Swaziland")},
        {"TC", tr("Turks and Caicos Islands")},
        {"TD", tr("Chad")},
        {"TF", tr("French Southern Territories")},
        {"TG", tr("Togo")},
        {"TH", tr("Thailand")},
        {"TJ", tr("Tajikistan")},
        {"TK", tr("Tokelau")},
        {"TL", tr("Timor-Leste")},
        {"TM", tr("Turkmenistan")},
        {"TN", tr("Tunisia")},
        {"TO", tr("Tonga")},
        {"TR", tr("Turkey")},
        {"TT", tr("Trinidad and Tobago")},
        {"TV", tr("Tuvalu")},
        {"TW", tr("Taiwan")},
        {"TZ", tr("Tanzania, United Republic of")},
        {"UA", tr("Ukraine")},
        {"UG", tr("Uganda")},
        {"UM", tr("United States Minor Outlying Islands")},
        {"US", tr("United States")},
        {"UY", tr("Uruguay")},
        {"UZ", tr("Uzbekistan")},
        {"VA", tr("Holy See (Vatican City State)")},
        {"VC", tr("Saint Vincent and the Grenadines")},
        {"VE", tr("Venezuela, Bolivarian Republic of")},
        {"VG", tr("Virgin Islands, British")},
        {"VI", tr("Virgin Islands, U.S.")},
        {"VN", tr("Vietnam")},
        {"VU", tr("Vanuatu")},
        {"WF", tr("Wallis and Futuna")},
        {"WS", tr("Samoa")},
        {"YE", tr("Yemen")},
        {"YT", tr("Mayotte")},
        {"ZA", tr("South Africa")},
        {"ZM", tr("Zambia")},
        {"ZW", tr("Zimbabwe")},

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
                .arg(m_geoIPDatabase->type(), m_geoIPDatabase->buildEpoch().toString()),
                Log::INFO);
            const QString targetPath = Utils::Fs::expandPathAbs(
                        specialFolderLocation(SpecialFolder::Data) + GEODB_FOLDER);
            if (!QDir(targetPath).exists())
                QDir().mkpath(targetPath);
            QFile targetFile(QString::fromLatin1("%1/%2").arg(targetPath, GEODB_FILENAME));
            if (!targetFile.open(QFile::WriteOnly) || (targetFile.write(data) == -1))
                LogMsg(tr("Couldn't save downloaded IP geolocation database file."), Log::WARNING);
            else
                LogMsg(tr("Successfully updated IP geolocation database."), Log::INFO);
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
