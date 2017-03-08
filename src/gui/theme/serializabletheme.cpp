/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Eugene Shalygin <eugene.shalygin@gmail.com>
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

#include "serializabletheme.h"

#include <set>

#include <QSettings>

#include "themeinfo.h"
#include "themeprovider.h"

namespace
{
    void appendAbsentElements(Theme::ElementsMap &res, QSettings &settings)
    {
        // we ignore all keys in [Info] group
        const QString infoPrefix = Theme::ThemeInfo::sectionName + QLatin1Char('/');
        const auto keys = settings.allKeys();
        for (const auto& key: keys) {
            if (!key.startsWith(infoPrefix) && // ignore all keys in [Info] group
                res.find(key) == res.end()) {
                    QVariant v = settings.value(key);
                    // if there were commas in the value, QVariant contains string list
                    // and we have to join it back
                    res[key] = v.type() == QVariant::StringList
                                ? v.toStringList().join(QLatin1Char(','))
                                : v.toString();
            }
        }
    }

    struct LoadedTheme
    {
        Theme::ThemeInfo info;
        Theme::ElementsMap elements;
    };

    /**
     * @brief Loads serialized themes
     *
     * This function handles themes inheritance and takes care of appending default theme if needed
     */
    LoadedTheme collectThemeElements(Theme::Kind kind, const QString& name, const QString& defaultInherited)
    {
        // we are going to load themes from inheritance hierarchy from descendant
        // to ancestor, appending ancestor element to the dictionary if corresponding keys
        // are not already present in it.
        bool firstThemeWasLoaded = false; // we need info object from the first theme only
        LoadedTheme res;

        QString themeName = name;
        std::set<QString> loadedThemeNames; // this will be used to protect us against infinite
                                            // inheritance loops

        Q_ASSERT(!themeName.isEmpty());
        do {
            const QString themeFileName = Theme::ThemeProvider::instance().locateTheme(kind, themeName);
            QSettings settings(themeFileName, QSettings::IniFormat);
            Theme::ThemeInfo info = Theme::ThemeInfo(settings);
            if (!firstThemeWasLoaded) {
                res.info = info;
                firstThemeWasLoaded = true;
            }
            appendAbsentElements(res.elements, settings);

            loadedThemeNames.insert(info.name());

            themeName = info.inheritedTheme();
            if (themeName.isEmpty()) {
                themeName = defaultInherited;
            }
        } while (loadedThemeNames.count(themeName) == 0);

        return res;
    }
}

Theme::ThemeSerializer::ThemeSerializer(Kind kind, const QString &themeName,
                                            const NamesMap& names, ElementDeserializer deserializer)
    : m_names {names}
    , m_info {}
    , m_kind {kind}
{
    const QString defaultThemeName = ThemeProvider::instance().defaultThemeName(m_kind);
    QString nameToUse = themeName.isEmpty() ? defaultThemeName : themeName;
    const LoadedTheme loaded = collectThemeElements(m_kind, nameToUse, defaultThemeName);
    m_info = std::move(loaded.info);
    for (const auto &p : loaded.elements) {
        const auto it = m_names.find(p.first.toLatin1());
        if (it == m_names.end()) {
            qCWarning(theme) << "Unexpected theme element '" << p.first << '\'';
            continue;
        }
        deserializer(it->second, p.second);
    }
}

void Theme::ThemeSerializer::save(const QString& themeFileName,
                                        bool explicitValues, ElementSerializer serializer) const
{
    QSettings settings(themeFileName, QSettings::IniFormat);

    m_info.save(settings);
    for (const auto &p : m_names) {
        settings.setValue(p.first, serializer(p.second, explicitValues));
    }
}

const Theme::ThemeInfo &Theme::ThemeSerializer::info() const
{
    return m_info;
}
