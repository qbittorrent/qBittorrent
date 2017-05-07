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

#include "plasmacolorprovider.h"

#include <QMetaEnum>

#include "utils/colorutils.h"
#include "utils/plasmacolorscheme.h"

namespace
{
    struct ColorProvidersRegistrator
    {
        ColorProvidersRegistrator()
        {
            using Registry = Theme::Serialization::ColorsProviderRegistry;
            using ProviderUPtr = Registry::ProviderUPtr;

            Registry::instance().registerProvider(ProviderUPtr(new Theme::Serialization::PlasmaColorProvider()));
        }
    };

    const QMetaEnum &QPaletteGroupMeta()
    {
        static QMetaEnum res = QMetaEnum::fromType<QPalette::ColorGroup>();
        return res;
    }

    const QMetaEnum &plasmaColorSetMeta()
    {
        static QMetaEnum res = QMetaEnum::fromType<PlasmaColorScheme::ColorSet>();
        return res;
    }

    const QMetaEnum &plasmaColorRoleMeta()
    {
        static QMetaEnum res = QMetaEnum::fromType<PlasmaColorScheme::ColorRole>();
        return res;
    }

    const QMetaEnum &enhancementMeta()
    {
        static QMetaEnum res = QMetaEnum::fromType<Theme::Serialization::PlasmaColor::Enhancement>();
        return res;
    }

    QColor lighten(const QColor &c)
    {
        return Utils::Color::lighten(c, 0.25);
    }

    QColor darken(const QColor &c)
    {
        return Utils::Color::darken(c, 0.25);
    }
}

std::unique_ptr<PlasmaColorScheme> Theme::Serialization::PlasmaColor::m_colorSheme;
Theme::Serialization::PlasmaColor::ColorCorrectionFunc Theme::Serialization::PlasmaColor::m_intencifyColor;
Theme::Serialization::PlasmaColor::ColorCorrectionFunc Theme::Serialization::PlasmaColor::m_reduceColor;

void Theme::Serialization::registerPlasmaColorProviders()
{
    static ColorProvidersRegistrator registrator;
}

Theme::Serialization::PlasmaColor::PlasmaColor(QPalette::ColorGroup group,
                                               PlasmaColorScheme::ColorSet set,
                                               PlasmaColorScheme::ColorRole role)
    : m_group {group}
    , m_set {set}
    , m_role {role}
{
}

namespace {
    template <class Enum>
    Enum stringToEnum(const QString &str, const QMetaEnum &meta)
    {
        bool ok = false;
        const int rawVal = meta.keyToValue(str.toLatin1().constData(), &ok);
        if (!ok) {
            throw std::runtime_error("Can not parse " + str.toStdString() + " into " + meta.name());
        }
        return static_cast<Enum>(rawVal);
    }
}

Theme::Serialization::PlasmaColor::PlasmaColor(const QString& serialized)
{
    const auto list = serialized.split(QLatin1Char(':'));
    if (list.size() < 1 || list.size() > 4)
        throw ValueParsingError("Plasma color notation should have 1 or 4 components", serialized);

    try {
        m_role = stringToEnum<PlasmaColorScheme::ColorRole>(list[0], plasmaColorRoleMeta());
        m_group = list.size() > 1 ? stringToEnum<QPalette::ColorGroup>(list[1], QPaletteGroupMeta())
                                  : QPalette::Active;
        m_set = list.size() > 2 ? stringToEnum<PlasmaColorScheme::ColorSet>(list[2], plasmaColorSetMeta())
                                : PlasmaColorScheme::View;
        m_enhancement = list.size() > 3 ? stringToEnum<Enhancement>(list[3], enhancementMeta())
                                        : Enhancement::None;
    }
    catch (std::runtime_error&) {
        throw ValueParsingError("Could not parse Plasma color", serialized);
    }
}

void Theme::Serialization::PlasmaColor::reloadDefaultPalette()
{
    if (!m_colorSheme) {
        m_colorSheme.reset(new PlasmaColorScheme());
    }
    else {
        m_colorSheme->reload();
    }
    const bool isDarkTheme = Theme::isDarkTheme();
    m_intencifyColor = isDarkTheme ? &lighten : &darken;
    m_reduceColor = isDarkTheme ? &darken : &lighten;
}

QString Theme::Serialization::PlasmaColor::serializationKey() const
{
    return QLatin1String("Plasma");
}

QString Theme::Serialization::PlasmaColor::serializedValue() const
{
    return QString(QLatin1String("%1:%2:%3:4%"))
            .arg(plasmaColorRoleMeta().key(m_role))
            .arg(QPaletteGroupMeta().key(m_group))
            .arg(plasmaColorSetMeta().key(m_set))
            .arg(enhancementMeta().key(static_cast<int>(m_enhancement)));
}

QColor Theme::Serialization::PlasmaColor::value() const
{
    QColor c = m_colorSheme->color(m_role, m_group, m_set);
    switch (m_enhancement) {
        case Enhancement::None:
            return c;
        case Enhancement::Intensify:
            return m_intencifyColor(c);
        case Enhancement::Reduce:
            return m_reduceColor(c);
        default:
            throw std::runtime_error("Unexpected value of m_enhancement");
    }
}

Theme::Serialization::PlasmaColorProvider::PlasmaColorProvider()
    : ColorProvider(PlasmaColor(QPalette::Active, PlasmaColorScheme::View, PlasmaColorScheme::ForegroundActive).serializationKey())
{
    PlasmaColor::reloadDefaultPalette();
}

void Theme::Serialization::PlasmaColorProvider::applicationPaletteChanged() const
{
    PlasmaColor::reloadDefaultPalette();
}

Theme::Serialization::ColorProvider::EntityUPtr
Theme::Serialization::PlasmaColorProvider::load(const QString& serialized) const
{
    return EntityUPtr(new PlasmaColor(serialized));
}
