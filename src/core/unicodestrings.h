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

#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif

const char C_INFINITY[] = "∞";
const char C_UP[] = "▲";
const char C_DOWN[] = "▼";
const char C_COPYRIGHT[] = "©";
const char C_LOCALE_ENGLISH[] = "English";
const char C_LOCALE_ENGLISH_AUSTRALIA[] = "English(Australia)";
const char C_LOCALE_ENGLISH_UNITEDKINGDOM[] = "English(United Kingdom)";
const char C_LOCALE_FRENCH[] = "Français";
const char C_LOCALE_GERMAN[] = "Deutsch";
const char C_LOCALE_HUNGARIAN[] = "Magyar";
const char C_LOCALE_INDONESIAN[] = "Bahasa Indonesia";
const char C_LOCALE_ITALIAN[] = "Italiano";
const char C_LOCALE_DUTCH[] = "Nederlands";
const char C_LOCALE_SPANISH[] = "Español";
const char C_LOCALE_CATALAN[] = "Català";
const char C_LOCALE_GALICIAN[] = "Galego";
const char C_LOCALE_PORTUGUESE[] = "Português";
const char C_LOCALE_PORTUGUESE_BRAZIL[] = "Português brasileiro";
const char C_LOCALE_POLISH[] = "Polski";
const char C_LOCALE_LITHUANIAN[] = "Lietuvių";
const char C_LOCALE_CZECH[] = "Čeština";
const char C_LOCALE_SLOVAK[] = "Slovenčina";
const char C_LOCALE_SERBIAN[] = "Српски";
const char C_LOCALE_CROATIAN[] = "Hrvatski";
const char C_LOCALE_ARMENIAN[] = "Հայերեն";
const char C_LOCALE_ROMANIAN[] = "Română";
const char C_LOCALE_TURKISH[] = "Türkçe";
const char C_LOCALE_GREEK[] = "Ελληνικά";
const char C_LOCALE_SWEDISH[] = "Svenska";
const char C_LOCALE_FINNISH[] = "Suomi";
const char C_LOCALE_NORWEGIAN[] = "Norsk";
const char C_LOCALE_DANISH[] = "Dansk";
const char C_LOCALE_BULGARIAN[] = "Български";
const char C_LOCALE_UKRAINIAN[] = "Українська";
const char C_LOCALE_RUSSIAN[] = "Русский";
const char C_LOCALE_JAPANESE[] = "日本語";
const char C_LOCALE_HEBREW[] = "עברית";
const char C_LOCALE_HINDI[] = "हिन्दी, हिंदी";
const char C_LOCALE_ARABIC[] = "عربي";
const char C_LOCALE_GEORGIAN[] = "ქართული";
const char C_LOCALE_BYELORUSSIAN[] = "Беларуская";
const char C_LOCALE_BASQUE[] = "Euskara";
const char C_LOCALE_VIETNAMESE[] = "tiếng Việt";
const char C_LOCALE_CHINESE_TRADITIONAL[] = "中文 (繁體)";
const char C_LOCALE_CHINESE_SIMPLIFIED[] = "中文 (简体)";
const char C_LOCALE_KOREAN[] = "한글";
const char C_THANKS[] =
    "<p>I would first like to thank sourceforge.net for hosting qBittorrent project and for their support.</p>\
    <p>I am pleased that people from all over the world are contributing to qBittorrent: Ishan Arora (India), Arnaud Demaizière (France) and Stephanos Antaris (Greece). Their help is greatly appreciated</p>\
    <p>I also want to thank Στέφανος Αντάρης (santaris@csd.auth.gr) and Mirco Chinelli (infinity89@fastwebmail.it) for working on Mac OS X packaging.</p>\
	<p>I am grateful to Peter Koeleman (peter@qbittorrent.org) and Mohammad Dib (mdib@qbittorrent.org) for working on qBittorrent port to Windows.</p>\
	<p>Thanks a lot to our graphist Mateusz Toboła (tobejodok@qbittorrent.org) for his great work.</p>";
const char C_TRANSLATORS[] =
    "<ul><li><u>Arabic:</u> SDERAWI (abz8868@msn.com), sn51234 (nesseyan@gmail.com) and  Ibrahim Saed ibraheem_alex(Transifex)</li>\
    <li><u>Armenian:</u> Hrant Ohanyan (hrantohanyan@mail.am)</li>\
    <li><u>Basque:</u> Xabier Aramendi (azpidatziak@gmail.com)</li>\
    <li><u>Belarusian:</u> Mihas Varantsou (meequz@gmail.com)</li>\
    <li><u>Bulgarian:</u> Tsvetan & Boyko Bankoff (emerge_life@users.sourceforge.net)</li>\
    <li><u>Catalan:</u> Francisco Luque Contreras (frannoe@ya.com)</li>\
    <li><u>Chinese (Simplified):</u> Guo Yue (yue.guo0418@gmail.com)</li>\
    <li><u>Chinese (Traditional):</u> Yi-Shun Wang (dnextstep@gmail.com) and 冥王歐西里斯 s8321414(Transifex)</li>\
    <li><u>Croatian:</u> Oliver Mucafir (oliver.untwist@gmail.com)</li>\
    <li><u>Czech:</u> Jirka Vilim (web@tets.cz) and Petr Cernobila abr(Transifex)</li>\
    <li><u>Danish:</u> Mathias Nielsen (comoneo@gmail.com)</li>\
    <li><u>Dutch:</u> Pieter Heyvaert (pieter_heyvaert@hotmail.com)</li>\
    <li><u>English(Australia):</u> Robert Readman readmanr(Transifex)</li>\
    <li><u>English(United Kingdom):</u> Robert Readman readmanr(Transifex)</li>\
    <li><u>Finnish:</u> Niklas Laxström (nikerabbit@users.sourceforge.net), Pekka Niemi (pekka.niemi@iki.fi) and Jiri Grönroos artnay(Transifex)</li>\
    <li><u>Galician:</u> Marcos Lans (marcoslansgarza@gmail.com) and antiparvos(Transifex)</li>\
    <li><u>Georgian:</u> Beqa Arabuli (arabulibeqa@yahoo.com)</li>\
    <li><u>German:</u> Niels Hoffmann (zentralmaschine@users.sourceforge.net), schnurlos (schnurlos@gmail.com)</li>\
    <li><u>Greek:</u> Tsvetan Bankov (emerge_life@users.sourceforge.net), Stephanos Antaris (santaris@csd.auth.gr), sledgehammer999(hammered999@gmail.com), Γιάννης Ανθυμίδης Evropi(Transifex) and Panagiotis Tabakis(tabakisp@gmail.com)</li>\
    <li><u>Hebrew:</u> David Deutsch (d.deffo@gmail.com)</li>\
    <li><u>Hungarian:</u> Majoros Péter (majoros.peterj@gmail.com)</li>\
    <li><u>Italian:</u> bovirus (bovirus@live.it) and Matteo Sechi (bu17714@gmail.com)</li>\
    <li><u>Japanese:</u> Masato Hashimoto (cabezon.hashimoto@gmail.com)</li>\
    <li><u>Korean:</u> Jin Woo Sin (jin828sin@users.sourceforge.net)</li>\
    <li><u>Lithuanian:</u> Naglis Jonaitis (njonaitis@gmail.com)</li>\
    <li><u>Norwegian:</u> Tomaso</li>\
    <li><u>Polish:</u> Mariusz Fik (fisiu@opensuse.org)</li>\
    <li><u>Portuguese:</u> Sérgio Marques smarquespt(Transifex)</li>\
    <li><u>Portuguese(Brazil):</u> Nick Marinho (nickmarinho@gmail.com)</li>\
    <li><u>Romanian:</u> Obada Denis (obadadenis@users.sourceforge.net), Adrian Gabor Adriannho(Transifex) and Mihai Coman z0id(Transifex)</li>\
    <li><u>Russian:</u> Nick Khazov (m2k3d0n at users.sourceforge.net), Alexey Morsov (samurai@ricom.ru), Nick Tiskov Dayman(daymansmail (at) gmail (dot) com), Dmitry DmitryKX(Transifex) and kraleksandr kraleksandr(Transifex)</li>\
    <li><u>Serbian:</u> Anaximandar Milet (anaximandar@operamail.com)</li>\
    <li><u>Slovak:</u>  helix84</li>\
    <li><u>Spanish:</u> Alfredo Monclús (alfrix), Francisco Luque Contreras (frannoe@ya.com), José Antonio Moray moray33(Transifex) and Diego de las Heras(Transifex)</li>\
    <li><u>Swedish:</u> Daniel Nylander (po@danielnylander.se) and Emil Hammarberg Ooglogput(Transifex)</li>\
    <li><u>Turkish:</u> Hasan YILMAZ (iletisim@hedefturkce.com) and Erdem Bingöl (erdem84@gmail.com)</li>\
    <li><u>Ukrainian:</u> Oleh Prypin (blaxpirit@gmail.com)</li>\
    <li><u>Vietnamese:</u> Anh Phan ppanhh(Transifex)</li></ul>";
