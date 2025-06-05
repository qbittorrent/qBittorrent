/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023-2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2019  Prince Gupta <jagannatharjun11@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If
 * you modify file(s), you may extend this exception to your version of the
 * file(s), but you are not obligated to do so. If you do not wish to do so,
 * delete this exception statement from your version.
 */

#pragma once

#include <QtSystemDetection>
#include <QtVersionChecks>
#include <QColor>
#include <QHash>
#include <QIcon>
#include <QObject>
#include <QPixmap>
#include <QString>

#include "uithemesource.h"

#if (QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)) && defined(Q_OS_WIN)
#define QBT_HAS_COLORSCHEME_OPTION
#endif

#ifdef QBT_HAS_COLORSCHEME_OPTION
#include "base/settingvalue.h"
#include "colorscheme.h"
#endif

class UIThemeManager final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(UIThemeManager)

public:
    static void initInstance();
    static void freeInstance();
    static UIThemeManager *instance();

#ifdef QBT_HAS_COLORSCHEME_OPTION
    ColorScheme colorScheme() const;
    void setColorScheme(ColorScheme value);
#endif

    QIcon getIcon(const QString &iconId, const QString &fallback = {}) const;
    QIcon getFlagIcon(const QString &countryIsoCode) const;
    QPixmap getScaledPixmap(const QString &iconId, int height) const;

    QColor getColor(const QString &id) const;

signals:
    void themeChanged();

private:
    UIThemeManager(); // singleton class

    void applyPalette() const;
    void applyStyleSheet() const;
    void onColorSchemeChanged();

#ifdef QBT_HAS_COLORSCHEME_OPTION
    void applyColorScheme() const;
#endif

    static UIThemeManager *m_instance;
    const bool m_useCustomTheme;
#ifdef QBT_HAS_COLORSCHEME_OPTION
    SettingValue<ColorScheme> m_colorSchemeSetting;
#endif
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    const bool m_useSystemIcons;
#endif
    std::unique_ptr<UIThemeSource> m_themeSource;
    mutable QHash<QString, QIcon> m_icons;
    mutable QHash<QString, QIcon> m_darkModeIcons;
    mutable QHash<QString, QIcon> m_flags;
};
