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

#ifndef QBT_THEMEPROVIDER_H
#define QBT_THEMEPROVIDER_H

#include <memory>
#include <vector>

#include <QFlags>

#include "themecommon.h"

class QPalette;
class QStringList;
class Application;

namespace Theme
{
    class ColorTheme;
    class FontTheme;
    class SerializableColorTheme;
    class SerializableFontTheme;
    class ThemeInfo;

    class ThemeProvider: public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY(ThemeProvider)

    public:
        static ThemeProvider &instance();

        /**
         * @brief Collection of available colour themes
         *
         * @returns map them info object -> file path
         */
        std::map<ThemeInfo, QString> availableThemes(Kind kind) const;

        QString defaultThemeName(Kind kind) const;

        const ColorTheme &colorTheme() const;
        const FontTheme &fontTheme() const;

        void setCurrentTheme(Kind kind, const QString &themeName);
        ThemeInfo currentTheme(Kind kind) const;

        /**
         * @brief Locate file with given named theme
         *
         * @param themeName Name of the theme to search for
         * @return File path
         */
        QString locateTheme(Kind kind, const QString &themeName) const;

        static const QLatin1String colorThemeFileExtension;
        static const QLatin1String fontThemeFileExtension;

        enum class ExportOption
        {
            NoOptions = 0x0,
            WriteExplicitValues = 0x1 //!< write explicit values, i.e. RGB instead of palette members
        };

        Q_DECLARE_FLAGS(ExportOptios, ExportOption)

        void exportTheme(Kind kind, const QString &themeName, const QString &fileName, ExportOptios options = ExportOption::NoOptions);

    signals:
        void colorThemeChanged();
        void fontThemeChanged();

    protected:
        ThemeProvider();
        ~ThemeProvider();

    private slots:
        void applicationPaletteChanged(const QPalette&);

    private:
        friend class ::Application;
        static void initInstance();
        void loadConfiguration();
        /**
         * @brief Theme search paths arranged by priority
         *
         * @return QStringList with paths. The firs one is of highest priority.
         */
        QStringList themeSearchPaths() const;


        std::unique_ptr<SerializableColorTheme> m_currentColorTheme;
        std::unique_ptr<SerializableFontTheme> m_currentFontTheme;
    };
}

#endif // QBT_THEMEPROVIDER_H
