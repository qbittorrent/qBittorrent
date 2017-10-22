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

#ifndef QBT_THEME_COLORPROVIDERS_H
#define QBT_THEME_COLORPROVIDERS_H

#include <QWidget>

#include "colorprovider_p.h"

namespace Theme
{
    namespace Serialization
    {
        void registerColorProviders();

        class ExplicitColor: public Color
        {
        public:
            explicit ExplicitColor(const QColor &color);
            explicit ExplicitColor(const QString &serialized);

            QColor value() const override;
            QString serializedValue() const override;
            QString serializationKey() const override;

        private:
            QColor m_value;
        };

        // links to a color from app default palette;
        class PaletteColor: public Color
        {
        public:
            explicit PaletteColor(QPalette::ColorGroup group, QPalette::ColorRole role);
            explicit PaletteColor(const QString &serialized);

            QColor value() const override;
            QString serializedValue() const override;
            QString serializationKey() const override;

            static void reloadDefaultPalette();

        private:
            static QPalette m_palette;
            QPalette::ColorGroup m_group;
            QPalette::ColorRole m_role;
        };

        class ColorPickingWidget: public QWidget
        {
        public:
            ColorPickingWidget(QWidget *parent);

            /**
             * @brief ...
             *
             * @return may return nullptr
             */
            virtual ColorProvider::EntityUPtr selectedColor() const = 0;
            virtual void selectColor(const Color &color) = 0;
        };

        /*! \brief Interface provides Widget-based UI for color providers
         *
         * This interface may be used when a color theme is edited and user wants to
         * select one of the supported colors using QWidget-based UI
         */
        class ColorProviderWidgetUI
        {
        public:
            virtual ~ColorProviderWidgetUI();
            virtual ColorPickingWidget *createEditorWidget(QWidget *parentWidget) const = 0;
        };

        namespace impl
        {
            class ExplicitColorWidget;
            class PaletteColorWidget;
        }

        class ExplicitColorProvider: public ColorProvider, public ColorProviderWidgetUI
        {
        public:
            ExplicitColorProvider();

        private:
            ColorPickingWidget *createEditorWidget(QWidget *parentWidget) const override;
            ColorProvider::EntityUPtr load(const QString &serialized) const override;
        };

        class PaletteColorProvider: public ColorProvider, public ColorProviderWidgetUI
        {
        public:
            PaletteColorProvider();

        private:
            ColorPickingWidget *createEditorWidget(QWidget *parentWidget) const override;
            ColorProvider::EntityUPtr load(const QString &serialized) const override;
            void applicationPaletteChanged() const override;
        };
    }
}

#endif // QBT_THEME_COLORPROVIDERS_H
