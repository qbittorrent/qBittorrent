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

#include "themeinfo.h"

#include <QDebug>
#include <QLocale>
#include <QSettings>

namespace
{
#define GROUP_NAME "Info"
#define FULL_SETTING_KEY(name) GROUP_NAME "/" name
    const QLatin1String nameFullKey(FULL_SETTING_KEY("Name"));
    const QLatin1String descriptionFullKey(FULL_SETTING_KEY("Description"));
    const QLatin1String inheritsFullKey(FULL_SETTING_KEY("Inherits"));

    const QLatin1String nameKey = QLatin1String("Name");
    const QLatin1String descriptionKey = QLatin1String("Description");
}

const QString Theme::ThemeInfo::sectionName = QLatin1String(GROUP_NAME);

Theme::ThemeInfo::ThemeInfo() = default;

Theme::ThemeInfo::ThemeInfo(QSettings &settings)
    : m_name {settings.value(nameFullKey, QLatin1Literal("Unnamed")).toString()}
    , m_description {settings.value(descriptionFullKey, QString()).toString()}
    , m_inheritedTheme {settings.value(inheritsFullKey, QString()).toString()}
{
    settings.beginGroup(QLatin1String(GROUP_NAME));
    m_localizedNames = readAffixedKeys(settings, QString(nameKey) + QLatin1Char('_'));
    m_localizedDescriptions = readAffixedKeys(settings, QString(descriptionKey) + QLatin1Char('_'));
    settings.endGroup();
}

void Theme::ThemeInfo::save(QSettings &settings) const
{
    settings.setValue(nameFullKey, m_name);
    settings.setValue(descriptionFullKey, m_description);
    settings.setValue(inheritsFullKey, m_inheritedTheme);

    settings.beginGroup(QLatin1String(GROUP_NAME));
    writeAffixedKeys(m_localizedNames, settings, QString(nameKey) + QLatin1Char('_'));
    writeAffixedKeys(m_localizedDescriptions, settings, QString(descriptionKey) + QLatin1Char('_'));
    settings.endGroup();
}

Theme::ThemeInfo::LocalizedStrings
Theme::ThemeInfo::readAffixedKeys(QSettings &settings, const QString &name)
{
    LocalizedStrings res;
    const QStringList keys = settings.allKeys();
    for (const QString &key : keys)
        if (key.startsWith(name))
            res[key.mid(name.size())] = settings.value(key).toString();
    return res;
}

void Theme::ThemeInfo::writeAffixedKeys(const LocalizedStrings &values, QSettings &settings, const QString &name)
{
    for (const auto &pair : values)
        settings.setValue(name + pair.first, pair.second);
}

QString Theme::ThemeInfo::name() const
{
    return m_name;
}

QString Theme::ThemeInfo::description() const
{
    return m_description;
}

QString Theme::ThemeInfo::inheritedTheme() const
{
    return m_inheritedTheme;
}

void Theme::ThemeInfo::setName(const QString &name)
{
    m_name = name;
}

void Theme::ThemeInfo::setDescription(const QString &description)
{
    m_description = description;
}

void Theme::ThemeInfo::setInheritedTheme(const QString &name)
{
    m_inheritedTheme = name;
}

QString Theme::ThemeInfo::findLocalizedString(const LocalizedStrings &strings)
{
    static QLocale loc;
    auto i = strings.find(loc.name());
    if (i != strings.end())
        return i->second;

    i = strings.find(QLocale::languageToString(loc.language()));
    if (i != strings.end())
        return i->second;

    return {};
}

QString Theme::ThemeInfo::localizedName() const
{
    const QString localized = findLocalizedString(m_localizedNames);
    return localized.isEmpty() ? m_name : localized;
}

QString Theme::ThemeInfo::localizedDescription() const
{
    const QString localized = findLocalizedString(m_localizedDescriptions);
    return localized.isEmpty() ? m_description : localized;
}

QDebug Theme::operator<<(QDebug debug, const Theme::ThemeInfo &info)
{
    debug << info.name() << " [" << info.description() << ']';
    return debug;
}
