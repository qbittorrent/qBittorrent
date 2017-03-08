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

#include "colorproviders.h"

#include <QMetaEnum>

namespace
{
    struct ColorProvidersRegistrator
    {
        ColorProvidersRegistrator()
        {
            using Registry = Theme::Serialization::ColorsProviderRegistry;
            using ProviderUPtr = Registry::ProviderUPtr;

            Registry::instance().registerProvider(ProviderUPtr(new Theme::Serialization::ExplicitColorProvider()));
            Registry::instance().registerProvider(ProviderUPtr(new Theme::Serialization::PaletteColorProvider()));
        }
    };

    const QMetaEnum &QPaletteGroupMeta()
    {
        static QMetaEnum res = QMetaEnum::fromType<QPalette::ColorGroup>();
        return res;
    }

    const QMetaEnum &QPaletteRoleMeta()
    {
        static QMetaEnum res = QMetaEnum::fromType<QPalette::ColorRole>();
        return res;
    }
}

void Theme::Serialization::registerColorProviders()
{
    static ColorProvidersRegistrator colorProvidersRegistrator;
}

Theme::Serialization::ExplicitColor::ExplicitColor(const QColor &color)
    : m_value {color}
{
}

/* ExplicitColor is just what it sounds like: a single QColor value.
 * It is serialized into a string formatted as follows: <R>,<G>,<B>, e.g.: 127,234,12
 */
Theme::Serialization::ExplicitColor::ExplicitColor(const QString &serialized)
    : m_value {QColor(serialized)}
{
    if (!m_value.isValid())
        throw ValueParsingError("QColor could not recognize color string", serialized);
}

QColor Theme::Serialization::ExplicitColor::value() const
{
    return m_value;
}

QString Theme::Serialization::ExplicitColor::serializedValue() const
{
    return explicitSerializedValue();
}

QString Theme::Serialization::ExplicitColor::serializationKey() const
{
    return QLatin1String("RGB");
}

QPalette Theme::Serialization::PaletteColor::m_palette;

Theme::Serialization::PaletteColor::PaletteColor(QPalette::ColorGroup group, QPalette::ColorRole role)
    : m_group(group)
    , m_role(role)
{
}

Theme::Serialization::PaletteColor::PaletteColor(const QString &serialized)
{
    const auto list = serialized.split(QLatin1Char(':'));
    if (list.size() != 2)
        throw ValueParsingError("QPalette color notation should have 2 components", serialized);

    bool parsedOk = false;
    const int groupRawVal = QPaletteGroupMeta().keyToValue(list[0].toLatin1().constData(), &parsedOk);
    if (!parsedOk)
        throw ValueParsingError("Could not parse QPalette group name", serialized);
    m_group = static_cast<QPalette::ColorGroup>(groupRawVal);

    const int roleRawVal = QPaletteRoleMeta().keyToValue(list[1].toLatin1().constData(), &parsedOk);
    if (!parsedOk)
        throw ValueParsingError("Could not parse QPalette role name", serialized);
    m_role = static_cast<QPalette::ColorRole>(roleRawVal);
}

void Theme::Serialization::PaletteColor::reloadDefaultPalette()
{
    m_palette = QPalette();     // which asks QApplication instance for the current default palette
}

QColor Theme::Serialization::PaletteColor::value() const
{
    return m_palette.color(m_group, m_role);
}

QString Theme::Serialization::PaletteColor::serializedValue() const
{
    return QString(QLatin1String("%1:%2"))
           .arg(QPaletteGroupMeta().key(m_group)).arg(QPaletteRoleMeta().key(m_role));
}

QString Theme::Serialization::PaletteColor::serializationKey() const
{
    return QLatin1String("QPalette");
}

void Theme::Serialization::PaletteColorProvider::applicationPaletteChanged() const
{
    PaletteColor::reloadDefaultPalette();
}

Theme::Serialization::ExplicitColorProvider::ExplicitColorProvider()
    : ColorProvider(ExplicitColor(QColor(Qt::black)).serializationKey())
{
}

Theme::Serialization::ExplicitColorProvider::EntityUPtr
Theme::Serialization::ExplicitColorProvider::load(const QString &serialized) const
{
    return EntityUPtr(new ExplicitColor(serialized));
}

Theme::Serialization::PaletteColorProvider::PaletteColorProvider()
    : ColorProvider(PaletteColor(QPalette::Active, QPalette::Window).serializationKey())
{
}

Theme::Serialization::PaletteColorProvider::EntityUPtr
Theme::Serialization::PaletteColorProvider::load(const QString &serialized) const
{
    return EntityUPtr(new PaletteColor(serialized));
}

Theme::Serialization::ColorProviderWidgetUI::~ColorProviderWidgetUI() = default;

Theme::Serialization::ColorPickingWidget *
Theme::Serialization::ExplicitColorProvider::createEditorWidget(QWidget *parentWidget) const
{
    Q_UNUSED(parentWidget);
    throw std::logic_error("Not implemented");
}

Theme::Serialization::ColorPickingWidget *
Theme::Serialization::PaletteColorProvider::createEditorWidget(QWidget *parentWidget) const
{
    Q_UNUSED(parentWidget);
    throw std::logic_error("Not implemented");
}
