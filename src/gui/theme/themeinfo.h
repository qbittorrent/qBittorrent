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

#ifndef QBT_THEMES_THEMEINFO_H
#define QBT_THEMES_THEMEINFO_H

#include <map>

#include <QString>

class QDebug;
class QSettings;

namespace Theme
{
    /**
     * @brief Utility class to encapsulate general theme attributes
     *
     * Contains name and description of the theme
     *
     * These data reside in section [Info] of the theme file:
     *
     * [Info]
     * Name=Theme name
     * Description=Description of the theme
     * Inherits=Other theme name
     * Name_de=Name in German
     * Name_gr=Name in Greek
     * Description_fr=Description in French
     * Description_uk=Description in Ukrainian
     *
     */
    class ThemeInfo
    {
    public:
        ThemeInfo();
        ThemeInfo(QSettings &settings);

        void save(QSettings &settings) const;

        QString name() const;
        QString localizedName() const;
        QString description() const;
        QString localizedDescription() const;
        QString inheritedTheme() const;

        void setName(const QString &name);
        void setDescription(const QString &description);
        void setInheritedTheme(const QString &name);

        static const QString sectionName;

    private:
        QString m_name;
        QString m_description;
        QString m_inheritedTheme;

        using LocalizedStrings = std::map<QString, QString>;
        /**
         * @brief Function reads key values whose names start with "${name}"
         * @returns dictionary of affix -> value
         */
        static LocalizedStrings readAffixedKeys(QSettings &settings, const QString &name);
        static void writeAffixedKeys(const LocalizedStrings &values, QSettings &settings, const QString &name);

        static QString findLocalizedString(const LocalizedStrings &strings);

        LocalizedStrings m_localizedNames;
        LocalizedStrings m_localizedDescriptions;
    };

    QDebug operator<<(QDebug debug, const ThemeInfo &info);

    // comparison operators: there may not be two themes with equal names of the same kind in the app

    inline bool operator<(const ThemeInfo &left, const ThemeInfo &right)
    {
        return left.name() < right.name();
    }

    inline bool operator==(const ThemeInfo &left, const ThemeInfo &right)
    {
        return left.name() == right.name();
    }
}

#endif
