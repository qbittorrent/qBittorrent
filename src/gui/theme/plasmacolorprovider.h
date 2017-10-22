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

#ifndef QBT_THEME_PLASMACOLORPROVIDERS_H
#define QBT_THEME_PLASMACOLORPROVIDERS_H

#include "colorprovider_p.h"
#include "utils/plasmacolorscheme.h"

class QWidget;

namespace Theme
{
    namespace Serialization
    {
        void registerPlasmaColorProviders();

        /**
         * @brief Provides colors from Plasma color scheme
         *
         */
        class PlasmaColor: public Color
        {
            Q_GADGET
        public:
            enum class Enhancement
            {
                None,
                Intensify,
                Reduce
            };

            Q_ENUM(Enhancement)

            explicit PlasmaColor(QPalette::ColorGroup group, PlasmaColorScheme::ColorSet set, PlasmaColorScheme::ColorRole role);
            explicit PlasmaColor(const QString &serialized);

            QColor value() const override;
            QString serializedValue() const override;
            QString serializationKey() const override;

            static void reloadDefaultPalette();

        private:
            using ColorCorrectionFunc = QColor (*)(const QColor&);
            static ColorCorrectionFunc m_intencifyColor;
            static ColorCorrectionFunc m_reduceColor;

            static std::unique_ptr<PlasmaColorScheme> m_colorSheme;
            QPalette::ColorGroup m_group;
            PlasmaColorScheme::ColorSet m_set;
            PlasmaColorScheme::ColorRole m_role;
            Enhancement m_enhancement;
        };

        class PlasmaColorProvider: public ColorProvider
        {
        public:
            PlasmaColorProvider();

        private:
            ColorProvider::EntityUPtr load(const QString &serialized) const override;
            void applicationPaletteChanged() const override;
        };
    }
}

#endif // QBT_THEME_PLASMACOLORPROVIDERS_H
