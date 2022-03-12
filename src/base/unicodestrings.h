/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Mike Tzou
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

#pragma once

#include "global.h"

// Because of the poor handling of UTF-8 characters in MSVC (emits warning C4819),
// we put all problematic UTF-8 chars/strings in this file.
// See issue #3059 for more details (https://github.com/qbittorrent/qBittorrent/issues/3059).

inline const QString C_COPYRIGHT = u"©"_qs;
inline const QString C_INEQUALITY = u"≠"_qs;
inline const QString C_INFINITY = u"∞"_qs;
inline const QString C_NON_BREAKING_SPACE = u" "_qs;
inline const QString C_THIN_SPACE = u" "_qs;
inline const QString C_UTP = u"μTP"_qs;

inline const QString C_LOCALE_ARABIC = u"عربي"_qs;
inline const QString C_LOCALE_ARMENIAN = u"Հայերեն"_qs;
inline const QString C_LOCALE_AZERBAIJANI = u"Azərbaycan dili"_qs;
inline const QString C_LOCALE_BASQUE = u"Euskara"_qs;
inline const QString C_LOCALE_BULGARIAN = u"Български"_qs;
inline const QString C_LOCALE_BYELORUSSIAN = u"Беларуская"_qs;
inline const QString C_LOCALE_CATALAN = u"Català"_qs;
inline const QString C_LOCALE_CHINESE_SIMPLIFIED = u"简体中文"_qs;
inline const QString C_LOCALE_CHINESE_TRADITIONAL_HK = u"香港正體字"_qs;
inline const QString C_LOCALE_CHINESE_TRADITIONAL_TW = u"正體中文"_qs;
inline const QString C_LOCALE_CROATIAN = u"Hrvatski"_qs;
inline const QString C_LOCALE_CZECH = u"Čeština"_qs;
inline const QString C_LOCALE_DANISH = u"Dansk"_qs;
inline const QString C_LOCALE_DUTCH = u"Nederlands"_qs;
inline const QString C_LOCALE_ENGLISH = u"English"_qs;
inline const QString C_LOCALE_ENGLISH_AUSTRALIA = u"English (Australia)"_qs;
inline const QString C_LOCALE_ENGLISH_UNITEDKINGDOM = u"English (United Kingdom)"_qs;
inline const QString C_LOCALE_ESPERANTO = u"Esperanto"_qs;
inline const QString C_LOCALE_ESTONIAN = u"Eesti, eesti keel"_qs;
inline const QString C_LOCALE_FINNISH = u"Suomi"_qs;
inline const QString C_LOCALE_FRENCH = u"Français"_qs;
inline const QString C_LOCALE_GALICIAN = u"Galego"_qs;
inline const QString C_LOCALE_GEORGIAN = u"ქართული"_qs;
inline const QString C_LOCALE_GERMAN = u"Deutsch"_qs;
inline const QString C_LOCALE_GREEK = u"Ελληνικά"_qs;
inline const QString C_LOCALE_HEBREW = u"עברית"_qs;
inline const QString C_LOCALE_HINDI = u"हिन्दी, हिंदी"_qs;
inline const QString C_LOCALE_HUNGARIAN = u"Magyar"_qs;
inline const QString C_LOCALE_ICELANDIC = u"Íslenska"_qs;
inline const QString C_LOCALE_INDONESIAN = u"Bahasa Indonesia"_qs;
inline const QString C_LOCALE_ITALIAN = u"Italiano"_qs;
inline const QString C_LOCALE_JAPANESE = u"日本語"_qs;
inline const QString C_LOCALE_KOREAN = u"한국어"_qs;
inline const QString C_LOCALE_LATGALIAN = u"Latgalīšu volūda"_qs;
inline const QString C_LOCALE_LATVIAN = u"Latviešu valoda"_qs;
inline const QString C_LOCALE_LITHUANIAN = u"Lietuvių"_qs;
inline const QString C_LOCALE_MALAY = u"بهاس ملايو"_qs;
inline const QString C_LOCALE_MONGOLIAN = u"Монгол хэл"_qs;
inline const QString C_LOCALE_NORWEGIAN = u"Norsk"_qs;
inline const QString C_LOCALE_OCCITAN = u"lenga d'òc"_qs;
inline const QString C_LOCALE_PERSIAN = u"فارسی"_qs;
inline const QString C_LOCALE_POLISH = u"Polski"_qs;
inline const QString C_LOCALE_PORTUGUESE = u"Português"_qs;
inline const QString C_LOCALE_PORTUGUESE_BRAZIL = u"Português brasileiro"_qs;
inline const QString C_LOCALE_ROMANIAN = u"Română"_qs;
inline const QString C_LOCALE_RUSSIAN = u"Русский"_qs;
inline const QString C_LOCALE_SERBIAN = u"Српски"_qs;
inline const QString C_LOCALE_SLOVAK = u"Slovenčina"_qs;
inline const QString C_LOCALE_SLOVENIAN = u"Slovenščina"_qs;
inline const QString C_LOCALE_SPANISH = u"Español"_qs;
inline const QString C_LOCALE_SWEDISH = u"Svenska"_qs;
inline const QString C_LOCALE_THAI = u"ไทย"_qs;
inline const QString C_LOCALE_TURKISH = u"Türkçe"_qs;
inline const QString C_LOCALE_UKRAINIAN = u"Українська"_qs;
inline const QString C_LOCALE_UZBEK = u"أۇزبېك‎"_qs;
inline const QString C_LOCALE_VIETNAMESE = u"Tiếng Việt"_qs;
