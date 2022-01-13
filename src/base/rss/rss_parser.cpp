/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2012  Christophe Dumez <chris@qbittorrent.org>
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

#include "rss_parser.h"

#include <QDateTime>
#include <QDebug>
#include <QGlobalStatic>
#include <QHash>
#include <QMetaObject>
#include <QRegularExpression>
#include <QStringList>
#include <QVariant>
#include <QXmlStreamEntityResolver>
#include <QXmlStreamReader>

#include "rss_article.h"

namespace
{
    class XmlStreamEntityResolver final : public QXmlStreamEntityResolver
    {
    public:
        QString resolveUndeclaredEntity(const QString &name) override
        {
            // (X)HTML entities declared in:
            //     http://www.w3.org/TR/xhtml1/DTD/xhtml-lat1.ent
            //     http://www.w3.org/TR/xhtml1/DTD/xhtml-symbol.ent
            //     http://www.w3.org/TR/xhtml1/DTD/xhtml-special.ent
            static const QHash<QString, QString> HTMLEntities
            {
                {"nbsp",   "&#160;"}, // no-break space = non-breaking space, U+00A0 ISOnum
                {"iexcl",  "&#161;"}, // inverted exclamation mark, U+00A1 ISOnum
                {"cent",   "&#162;"}, // cent sign, U+00A2 ISOnum
                {"pound",  "&#163;"}, // pound sign, U+00A3 ISOnum
                {"curren", "&#164;"}, // currency sign, U+00A4 ISOnum
                {"yen",    "&#165;"}, // yen sign = yuan sign, U+00A5 ISOnum
                {"brvbar", "&#166;"}, // broken bar = broken vertical bar, U+00A6 ISOnum
                {"sect",   "&#167;"}, // section sign, U+00A7 ISOnum
                {"uml",    "&#168;"}, // diaeresis = spacing diaeresis, U+00A8 ISOdia
                {"copy",   "&#169;"}, // copyright sign, U+00A9 ISOnum
                {"ordf",   "&#170;"}, // feminine ordinal indicator, U+00AA ISOnum
                {"laquo",  "&#171;"}, // left-pointing double angle quotation mark = left pointing guillemet, U+00AB ISOnum
                {"not",    "&#172;"}, // not sign = angled dash, U+00AC ISOnum
                {"shy",    "&#173;"}, // soft hyphen = discretionary hyphen, U+00AD ISOnum
                {"reg",    "&#174;"}, // registered sign = registered trade mark sign, U+00AE ISOnum
                {"macr",   "&#175;"}, // macron = spacing macron = overline = APL overbar, U+00AF ISOdia
                {"deg",    "&#176;"}, // degree sign, U+00B0 ISOnum
                {"plusmn", "&#177;"}, // plus-minus sign = plus-or-minus sign, U+00B1 ISOnum
                {"sup2",   "&#178;"}, // superscript two = superscript digit two = squared, U+00B2 ISOnum
                {"sup3",   "&#179;"}, // superscript three = superscript digit three = cubed, U+00B3 ISOnum
                {"acute",  "&#180;"}, // acute accent = spacing acute, U+00B4 ISOdia
                {"micro",  "&#181;"}, // micro sign, U+00B5 ISOnum
                {"para",   "&#182;"}, // pilcrow sign = paragraph sign, U+00B6 ISOnum
                {"middot", "&#183;"}, // middle dot = Georgian comma = Greek middle dot, U+00B7 ISOnum
                {"cedil",  "&#184;"}, // cedilla = spacing cedilla, U+00B8 ISOdia
                {"sup1",   "&#185;"}, // superscript one = superscript digit one, U+00B9 ISOnum
                {"ordm",   "&#186;"}, // masculine ordinal indicator, U+00BA ISOnum
                {"raquo",  "&#187;"}, // right-pointing double angle quotation mark = right pointing guillemet, U+00BB ISOnum
                {"frac14", "&#188;"}, // vulgar fraction one quarter = fraction one quarter, U+00BC ISOnum
                {"frac12", "&#189;"}, // vulgar fraction one half = fraction one half, U+00BD ISOnum
                {"frac34", "&#190;"}, // vulgar fraction three quarters = fraction three quarters, U+00BE ISOnum
                {"iquest", "&#191;"}, // inverted question mark = turned question mark, U+00BF ISOnum
                {"Agrave", "&#192;"}, // latin capital letter A with grave = latin capital letter A grave, U+00C0 ISOlat1
                {"Aacute", "&#193;"}, // latin capital letter A with acute, U+00C1 ISOlat1
                {"Acirc",  "&#194;"}, // latin capital letter A with circumflex, U+00C2 ISOlat1
                {"Atilde", "&#195;"}, // latin capital letter A with tilde, U+00C3 ISOlat1
                {"Auml",   "&#196;"}, // latin capital letter A with diaeresis, U+00C4 ISOlat1
                {"Aring",  "&#197;"}, // latin capital letter A with ring above = latin capital letter A ring, U+00C5 ISOlat1
                {"AElig",  "&#198;"}, // latin capital letter AE = latin capital ligature AE, U+00C6 ISOlat1
                {"Ccedil", "&#199;"}, // latin capital letter C with cedilla, U+00C7 ISOlat1
                {"Egrave", "&#200;"}, // latin capital letter E with grave, U+00C8 ISOlat1
                {"Eacute", "&#201;"}, // latin capital letter E with acute, U+00C9 ISOlat1
                {"Ecirc",  "&#202;"}, // latin capital letter E with circumflex, U+00CA ISOlat1
                {"Euml",   "&#203;"}, // latin capital letter E with diaeresis, U+00CB ISOlat1
                {"Igrave", "&#204;"}, // latin capital letter I with grave, U+00CC ISOlat1
                {"Iacute", "&#205;"}, // latin capital letter I with acute, U+00CD ISOlat1
                {"Icirc",  "&#206;"}, // latin capital letter I with circumflex, U+00CE ISOlat1
                {"Iuml",   "&#207;"}, // latin capital letter I with diaeresis, U+00CF ISOlat1
                {"ETH",    "&#208;"}, // latin capital letter ETH, U+00D0 ISOlat1
                {"Ntilde", "&#209;"}, // latin capital letter N with tilde, U+00D1 ISOlat1
                {"Ograve", "&#210;"}, // latin capital letter O with grave, U+00D2 ISOlat1
                {"Oacute", "&#211;"}, // latin capital letter O with acute, U+00D3 ISOlat1
                {"Ocirc",  "&#212;"}, // latin capital letter O with circumflex, U+00D4 ISOlat1
                {"Otilde", "&#213;"}, // latin capital letter O with tilde, U+00D5 ISOlat1
                {"Ouml",   "&#214;"}, // latin capital letter O with diaeresis, U+00D6 ISOlat1
                {"times",  "&#215;"}, // multiplication sign, U+00D7 ISOnum
                {"Oslash", "&#216;"}, // latin capital letter O with stroke = latin capital letter O slash, U+00D8 ISOlat1
                {"Ugrave", "&#217;"}, // latin capital letter U with grave, U+00D9 ISOlat1
                {"Uacute", "&#218;"}, // latin capital letter U with acute, U+00DA ISOlat1
                {"Ucirc",  "&#219;"}, // latin capital letter U with circumflex, U+00DB ISOlat1
                {"Uuml",   "&#220;"}, // latin capital letter U with diaeresis, U+00DC ISOlat1
                {"Yacute", "&#221;"}, // latin capital letter Y with acute, U+00DD ISOlat1
                {"THORN",  "&#222;"}, // latin capital letter THORN, U+00DE ISOlat1
                {"szlig",  "&#223;"}, // latin small letter sharp s = ess-zed, U+00DF ISOlat1
                {"agrave", "&#224;"}, // latin small letter a with grave = latin small letter a grave, U+00E0 ISOlat1
                {"aacute", "&#225;"}, // latin small letter a with acute, U+00E1 ISOlat1
                {"acirc",  "&#226;"}, // latin small letter a with circumflex, U+00E2 ISOlat1
                {"atilde", "&#227;"}, // latin small letter a with tilde, U+00E3 ISOlat1
                {"auml",   "&#228;"}, // latin small letter a with diaeresis, U+00E4 ISOlat1
                {"aring",  "&#229;"}, // latin small letter a with ring above = latin small letter a ring, U+00E5 ISOlat1
                {"aelig",  "&#230;"}, // latin small letter ae = latin small ligature ae, U+00E6 ISOlat1
                {"ccedil", "&#231;"}, // latin small letter c with cedilla, U+00E7 ISOlat1
                {"egrave", "&#232;"}, // latin small letter e with grave, U+00E8 ISOlat1
                {"eacute", "&#233;"}, // latin small letter e with acute, U+00E9 ISOlat1
                {"ecirc",  "&#234;"}, // latin small letter e with circumflex, U+00EA ISOlat1
                {"euml",   "&#235;"}, // latin small letter e with diaeresis, U+00EB ISOlat1
                {"igrave", "&#236;"}, // latin small letter i with grave, U+00EC ISOlat1
                {"iacute", "&#237;"}, // latin small letter i with acute, U+00ED ISOlat1
                {"icirc",  "&#238;"}, // latin small letter i with circumflex, U+00EE ISOlat1
                {"iuml",   "&#239;"}, // latin small letter i with diaeresis, U+00EF ISOlat1
                {"eth",    "&#240;"}, // latin small letter eth, U+00F0 ISOlat1
                {"ntilde", "&#241;"}, // latin small letter n with tilde, U+00F1 ISOlat1
                {"ograve", "&#242;"}, // latin small letter o with grave, U+00F2 ISOlat1
                {"oacute", "&#243;"}, // latin small letter o with acute, U+00F3 ISOlat1
                {"ocirc",  "&#244;"}, // latin small letter o with circumflex, U+00F4 ISOlat1
                {"otilde", "&#245;"}, // latin small letter o with tilde, U+00F5 ISOlat1
                {"ouml",   "&#246;"}, // latin small letter o with diaeresis, U+00F6 ISOlat1
                {"divide", "&#247;"}, // division sign, U+00F7 ISOnum
                {"oslash", "&#248;"}, // latin small letter o with stroke, = latin small letter o slash, U+00F8 ISOlat1
                {"ugrave", "&#249;"}, // latin small letter u with grave, U+00F9 ISOlat1
                {"uacute", "&#250;"}, // latin small letter u with acute, U+00FA ISOlat1
                {"ucirc",  "&#251;"}, // latin small letter u with circumflex, U+00FB ISOlat1
                {"uuml",   "&#252;"}, // latin small letter u with diaeresis, U+00FC ISOlat1
                {"yacute", "&#253;"}, // latin small letter y with acute, U+00FD ISOlat1
                {"thorn",  "&#254;"}, // latin small letter thorn, U+00FE ISOlat1
                {"yuml",   "&#255;"}, // latin small letter y with diaeresis, U+00FF ISOlat1

                // Latin Extended-A
                {"OElig",   "&#338;"}, //  latin capital ligature OE, U+0152 ISOlat2
                {"oelig",   "&#339;"}, //  latin small ligature oe, U+0153 ISOlat2
                // ligature is a misnomer, this is a separate character in some languages
                {"Scaron",  "&#352;"}, //  latin capital letter S with caron, U+0160 ISOlat2
                {"scaron",  "&#353;"}, //  latin small letter s with caron, U+0161 ISOlat2
                {"Yuml",    "&#376;"}, //  latin capital letter Y with diaeresis, U+0178 ISOlat2

                // Spacing Modifier Letters
                {"circ",    "&#710;"}, //  modifier letter circumflex accent, U+02C6 ISOpub
                {"tilde",   "&#732;"}, //  small tilde, U+02DC ISOdia

                // General Punctuation
                {"ensp",    "&#8194;"}, // en space, U+2002 ISOpub
                {"emsp",    "&#8195;"}, // em space, U+2003 ISOpub
                {"thinsp",  "&#8201;"}, // thin space, U+2009 ISOpub
                {"zwnj",    "&#8204;"}, // zero width non-joiner, U+200C NEW RFC 2070
                {"zwj",     "&#8205;"}, // zero width joiner, U+200D NEW RFC 2070
                {"lrm",     "&#8206;"}, // left-to-right mark, U+200E NEW RFC 2070
                {"rlm",     "&#8207;"}, // right-to-left mark, U+200F NEW RFC 2070
                {"ndash",   "&#8211;"}, // en dash, U+2013 ISOpub
                {"mdash",   "&#8212;"}, // em dash, U+2014 ISOpub
                {"lsquo",   "&#8216;"}, // left single quotation mark, U+2018 ISOnum
                {"rsquo",   "&#8217;"}, // right single quotation mark, U+2019 ISOnum
                {"sbquo",   "&#8218;"}, // single low-9 quotation mark, U+201A NEW
                {"ldquo",   "&#8220;"}, // left double quotation mark, U+201C ISOnum
                {"rdquo",   "&#8221;"}, // right double quotation mark, U+201D ISOnum
                {"bdquo",   "&#8222;"}, // double low-9 quotation mark, U+201E NEW
                {"dagger",  "&#8224;"}, // dagger, U+2020 ISOpub
                {"Dagger",  "&#8225;"}, // double dagger, U+2021 ISOpub
                {"permil",  "&#8240;"}, // per mille sign, U+2030 ISOtech
                {"lsaquo",  "&#8249;"}, // single left-pointing angle quotation mark, U+2039 ISO proposed
                // lsaquo is proposed but not yet ISO standardized
                {"rsaquo",  "&#8250;"}, // single right-pointing angle quotation mark, U+203A ISO proposed
                // rsaquo is proposed but not yet ISO standardized

                // Currency Symbols
                {"euro",   "&#8364;"}, //  euro sign, U+20AC NEW

                // Latin Extended-B
                {"fnof",     "&#402;"}, // latin small letter f with hook = function   = florin, U+0192 ISOtech

                // Greek
                {"Alpha",    "&#913;"}, // greek capital letter alpha, U+0391
                {"Beta",     "&#914;"}, // greek capital letter beta, U+0392
                {"Gamma",    "&#915;"}, // greek capital letter gamma, U+0393 ISOgrk3
                {"Delta",    "&#916;"}, // greek capital letter delta, U+0394 ISOgrk3
                {"Epsilon",  "&#917;"}, // greek capital letter epsilon, U+0395
                {"Zeta",     "&#918;"}, // greek capital letter zeta, U+0396
                {"Eta",      "&#919;"}, // greek capital letter eta, U+0397
                {"Theta",    "&#920;"}, // greek capital letter theta, U+0398 ISOgrk3
                {"Iota",     "&#921;"}, // greek capital letter iota, U+0399
                {"Kappa",    "&#922;"}, // greek capital letter kappa, U+039A
                {"Lambda",   "&#923;"}, // greek capital letter lamda, U+039B ISOgrk3
                {"Mu",       "&#924;"}, // greek capital letter mu, U+039C
                {"Nu",       "&#925;"}, // greek capital letter nu, U+039D
                {"Xi",       "&#926;"}, // greek capital letter xi, U+039E ISOgrk3
                {"Omicron",  "&#927;"}, // greek capital letter omicron, U+039F
                {"Pi",       "&#928;"}, // greek capital letter pi, U+03A0 ISOgrk3
                {"Rho",      "&#929;"}, // greek capital letter rho, U+03A1
                {"Sigma",    "&#931;"}, // greek capital letter sigma, U+03A3 ISOgrk3
                {"Tau",      "&#932;"}, // greek capital letter tau, U+03A4
                {"Upsilon",  "&#933;"}, // greek capital letter upsilon, U+03A5 ISOgrk3
                {"Phi",      "&#934;"}, // greek capital letter phi, U+03A6 ISOgrk3
                {"Chi",      "&#935;"}, // greek capital letter chi, U+03A7
                {"Psi",      "&#936;"}, // greek capital letter psi, U+03A8 ISOgrk3
                {"Omega",    "&#937;"}, // greek capital letter omega, U+03A9 ISOgrk3
                {"alpha",    "&#945;"}, // greek small letter alpha, U+03B1 ISOgrk3
                {"beta",     "&#946;"}, // greek small letter beta, U+03B2 ISOgrk3
                {"gamma",    "&#947;"}, // greek small letter gamma, U+03B3 ISOgrk3
                {"delta",    "&#948;"}, // greek small letter delta, U+03B4 ISOgrk3
                {"epsilon",  "&#949;"}, // greek small letter epsilon, U+03B5 ISOgrk3
                {"zeta",     "&#950;"}, // greek small letter zeta, U+03B6 ISOgrk3
                {"eta",      "&#951;"}, // greek small letter eta, U+03B7 ISOgrk3
                {"theta",    "&#952;"}, // greek small letter theta, U+03B8 ISOgrk3
                {"iota",     "&#953;"}, // greek small letter iota, U+03B9 ISOgrk3
                {"kappa",    "&#954;"}, // greek small letter kappa, U+03BA ISOgrk3
                {"lambda",   "&#955;"}, // greek small letter lamda, U+03BB ISOgrk3
                {"mu",       "&#956;"}, // greek small letter mu, U+03BC ISOgrk3
                {"nu",       "&#957;"}, // greek small letter nu, U+03BD ISOgrk3
                {"xi",       "&#958;"}, // greek small letter xi, U+03BE ISOgrk3
                {"omicron",  "&#959;"}, // greek small letter omicron, U+03BF NEW
                {"pi",       "&#960;"}, // greek small letter pi, U+03C0 ISOgrk3
                {"rho",      "&#961;"}, // greek small letter rho, U+03C1 ISOgrk3
                {"sigmaf",   "&#962;"}, // greek small letter final sigma, U+03C2 ISOgrk3
                {"sigma",    "&#963;"}, // greek small letter sigma, U+03C3 ISOgrk3
                {"tau",      "&#964;"}, // greek small letter tau, U+03C4 ISOgrk3
                {"upsilon",  "&#965;"}, // greek small letter upsilon, U+03C5 ISOgrk3
                {"phi",      "&#966;"}, // greek small letter phi, U+03C6 ISOgrk3
                {"chi",      "&#967;"}, // greek small letter chi, U+03C7 ISOgrk3
                {"psi",      "&#968;"}, // greek small letter psi, U+03C8 ISOgrk3
                {"omega",    "&#969;"}, // greek small letter omega, U+03C9 ISOgrk3
                {"thetasym", "&#977;"}, // greek theta symbol, U+03D1 NEW
                {"upsih",    "&#978;"}, // greek upsilon with hook symbol, U+03D2 NEW
                {"piv",      "&#982;"}, // greek pi symbol, U+03D6 ISOgrk3

                // General Punctuation
                {"bull",     "&#8226;"}, // bullet = black small circle, U+2022 ISOpub
                // bullet is NOT the same as bullet operator, U+2219
                {"hellip",   "&#8230;"}, // horizontal ellipsis = three dot leader, U+2026 ISOpub
                {"prime",    "&#8242;"}, // prime = minutes = feet, U+2032 ISOtech
                {"Prime",    "&#8243;"}, // double prime = seconds = inches, U+2033 ISOtech
                {"oline",    "&#8254;"}, // overline = spacing overscore, U+203E NEW
                {"frasl",    "&#8260;"}, // fraction slash, U+2044 NEW

                // Letterlike Symbols
                {"weierp",   "&#8472;"}, // script capital P = power set = Weierstrass p, U+2118 ISOamso
                {"image",    "&#8465;"}, // black-letter capital I = imaginary part, U+2111 ISOamso
                {"real",     "&#8476;"}, // black-letter capital R = real part symbol, U+211C ISOamso
                {"trade",    "&#8482;"}, // trade mark sign, U+2122 ISOnum
                {"alefsym",  "&#8501;"}, // alef symbol = first transfinite cardinal, U+2135 NEW
                // alef symbol is NOT the same as hebrew letter alef,
                //     U+05D0 although the same glyph could be used to depict both characters

                // Arrows
                {"larr",     "&#8592;"}, // leftwards arrow, U+2190 ISOnum
                {"uarr",     "&#8593;"}, // upwards arrow, U+2191 ISOnum
                {"rarr",     "&#8594;"}, // rightwards arrow, U+2192 ISOnum
                {"darr",     "&#8595;"}, // downwards arrow, U+2193 ISOnum
                {"harr",     "&#8596;"}, // left right arrow, U+2194 ISOamsa
                {"crarr",    "&#8629;"}, // downwards arrow with corner leftwards = carriage return, U+21B5 NEW
                {"lArr",     "&#8656;"}, // leftwards double arrow, U+21D0 ISOtech
                // Unicode does not say that lArr is the same as the 'is implied by' arrow
                //    but also does not have any other character for that function. So lArr can
                //    be used for 'is implied by' as ISOtech suggests
                {"uArr",     "&#8657;"}, // upwards double arrow, U+21D1 ISOamsa
                {"rArr",     "&#8658;"}, // rightwards double arrow, U+21D2 ISOtech
                // Unicode does not say this is the 'implies' character but does not have
                //     another character with this function so rArr can be used for 'implies'
                //     as ISOtech suggests
                {"dArr",     "&#8659;"}, // downwards double arrow, U+21D3 ISOamsa
                {"hArr",     "&#8660;"}, // left right double arrow, U+21D4 ISOamsa

                // Mathematical Operators
                {"forall",   "&#8704;"}, // for all, U+2200 ISOtech
                {"part",     "&#8706;"}, // partial differential, U+2202 ISOtech
                {"exist",    "&#8707;"}, // there exists, U+2203 ISOtech
                {"empty",    "&#8709;"}, // empty set = null set, U+2205 ISOamso
                {"nabla",    "&#8711;"}, // nabla = backward difference, U+2207 ISOtech
                {"isin",     "&#8712;"}, // element of, U+2208 ISOtech
                {"notin",    "&#8713;"}, // not an element of, U+2209 ISOtech
                {"ni",       "&#8715;"}, // contains as member, U+220B ISOtech
                {"prod",     "&#8719;"}, // n-ary product = product sign, U+220F ISOamsb
                // prod is NOT the same character as U+03A0 'greek capital letter pi' though
                //     the same glyph might be used for both
                {"sum",      "&#8721;"}, // n-ary summation, U+2211 ISOamsb
                // sum is NOT the same character as U+03A3 'greek capital letter sigma'
                //     though the same glyph might be used for both
                {"minus",    "&#8722;"}, // minus sign, U+2212 ISOtech
                {"lowast",   "&#8727;"}, // asterisk operator, U+2217 ISOtech
                {"radic",    "&#8730;"}, // square root = radical sign, U+221A ISOtech
                {"prop",     "&#8733;"}, // proportional to, U+221D ISOtech
                {"infin",    "&#8734;"}, // infinity, U+221E ISOtech
                {"ang",      "&#8736;"}, // angle, U+2220 ISOamso
                {"and",      "&#8743;"}, // logical and = wedge, U+2227 ISOtech
                {"or",       "&#8744;"}, // logical or = vee, U+2228 ISOtech
                {"cap",      "&#8745;"}, // intersection = cap, U+2229 ISOtech
                {"cup",      "&#8746;"}, // union = cup, U+222A ISOtech
                {"int",      "&#8747;"}, // integral, U+222B ISOtech
                {"there4",   "&#8756;"}, // therefore, U+2234 ISOtech
                {"sim",      "&#8764;"}, // tilde operator = varies with = similar to, U+223C ISOtech
                // tilde operator is NOT the same character as the tilde, U+007E,
                //     although the same glyph might be used to represent both
                {"cong",     "&#8773;"}, // approximately equal to, U+2245 ISOtech
                {"asymp",    "&#8776;"}, // almost equal to = asymptotic to, U+2248 ISOamsr
                {"ne",       "&#8800;"}, // not equal to, U+2260 ISOtech
                {"equiv",    "&#8801;"}, // identical to, U+2261 ISOtech
                {"le",       "&#8804;"}, // less-than or equal to, U+2264 ISOtech
                {"ge",       "&#8805;"}, // greater-than or equal to, U+2265 ISOtech
                {"sub",      "&#8834;"}, // subset of, U+2282 ISOtech
                {"sup",      "&#8835;"}, // superset of, U+2283 ISOtech
                {"nsub",     "&#8836;"}, // not a subset of, U+2284 ISOamsn
                {"sube",     "&#8838;"}, // subset of or equal to, U+2286 ISOtech
                {"supe",     "&#8839;"}, // superset of or equal to, U+2287 ISOtech
                {"oplus",    "&#8853;"}, // circled plus = direct sum, U+2295 ISOamsb
                {"otimes",   "&#8855;"}, // circled times = vector product, U+2297 ISOamsb
                {"perp",     "&#8869;"}, // up tack = orthogonal to = perpendicular, U+22A5 ISOtech
                {"sdot",     "&#8901;"}, // dot operator, U+22C5 ISOamsb
                // dot operator is NOT the same character as U+00B7 middle dot

                // Miscellaneous Technical
                {"lceil",    "&#8968;"}, // left ceiling = APL upstile, U+2308 ISOamsc
                {"rceil",    "&#8969;"}, // right ceiling, U+2309 ISOamsc
                {"lfloor",   "&#8970;"}, // left floor = APL downstile, U+230A ISOamsc
                {"rfloor",   "&#8971;"}, // right floor, U+230B ISOamsc
                {"lang",     "&#9001;"}, // left-pointing angle bracket = bra, U+2329 ISOtech
                // lang is NOT the same character as U+003C 'less than sign'
                //     or U+2039 'single left-pointing angle quotation mark'
                {"rang",     "&#9002;"}, // right-pointing angle bracket = ket, U+232A ISOtech
                // rang is NOT the same character as U+003E 'greater than sign'
                //     or U+203A 'single right-pointing angle quotation mark'

                // Geometric Shapes
                {"loz",      "&#9674;"}, // lozenge, U+25CA ISOpub

                // Miscellaneous Symbols
                {"spades",   "&#9824;"}, // black spade suit, U+2660 ISOpub
                {"clubs",    "&#9827;"}, // black club suit = shamrock, U+2663 ISOpub
                {"hearts",   "&#9829;"}, // black heart suit = valentine, U+2665 ISOpub
                {"diams",    "&#9830;"}  // black diamond suit, U+2666 ISOpub
            };
            return HTMLEntities.value(name);
        }
    };

    // Ported to Qt from KDElibs4
    QDateTime parseDate(const QString &string)
    {
        const char shortDay[][4] =
        {
            "Mon", "Tue", "Wed",
            "Thu", "Fri", "Sat",
            "Sun"
        };
        const char longDay[][10] =
        {
            "Monday", "Tuesday", "Wednesday",
            "Thursday", "Friday", "Saturday",
            "Sunday"
        };
        const char shortMonth[][4] =
        {
            "Jan", "Feb", "Mar", "Apr",
            "May", "Jun", "Jul", "Aug",
            "Sep", "Oct", "Nov", "Dec"
        };

        const QString str = string.trimmed();
        if (str.isEmpty())
            return QDateTime::currentDateTime();

        int nyear  = 6;   // indexes within string to values
        int nmonth = 4;
        int nday   = 2;
        int nwday  = 1;
        int nhour  = 7;
        int nmin   = 8;
        int nsec   = 9;
        // Also accept obsolete form "Weekday, DD-Mon-YY HH:MM:SS ±hhmm"
        QRegularExpression rx {"^(?:([A-Z][a-z]+),\\s*)?(\\d{1,2})(\\s+|-)([^-\\s]+)(\\s+|-)(\\d{2,4})\\s+(\\d\\d):(\\d\\d)(?::(\\d\\d))?\\s+(\\S+)$"};
        QRegularExpressionMatch rxMatch;
        QStringList parts;
        if (str.indexOf(rx, 0, &rxMatch) == 0)
        {
            // Check that if date has '-' separators, both separators are '-'.
            parts = rxMatch.capturedTexts();
            const bool h1 = (parts[3] == QLatin1String("-"));
            const bool h2 = (parts[5] == QLatin1String("-"));
            if (h1 != h2)
                return QDateTime::currentDateTime();
        }
        else
        {
            // Check for the obsolete form "Wdy Mon DD HH:MM:SS YYYY"
            rx = QRegularExpression {"^([A-Z][a-z]+)\\s+(\\S+)\\s+(\\d\\d)\\s+(\\d\\d):(\\d\\d):(\\d\\d)\\s+(\\d\\d\\d\\d)$"};
            if (str.indexOf(rx, 0, &rxMatch) != 0)
                return QDateTime::currentDateTime();

            nyear  = 7;
            nmonth = 2;
            nday   = 3;
            nwday  = 1;
            nhour  = 4;
            nmin   = 5;
            nsec   = 6;
            parts = rxMatch.capturedTexts();
        }

        bool ok[4];
        const int day = parts[nday].toInt(&ok[0]);
        int year = parts[nyear].toInt(&ok[1]);
        const int hour = parts[nhour].toInt(&ok[2]);
        const int minute = parts[nmin].toInt(&ok[3]);
        if (!ok[0] || !ok[1] || !ok[2] || !ok[3])
            return QDateTime::currentDateTime();

        int second = 0;
        if (!parts[nsec].isEmpty())
        {
            second = parts[nsec].toInt(&ok[0]);
            if (!ok[0])
                return QDateTime::currentDateTime();
        }

        const bool leapSecond = (second == 60);
        if (leapSecond)
            second = 59;   // apparently a leap second - validate below, once time zone is known
        int month = 0;
        for ( ; (month < 12) && (parts[nmonth] != shortMonth[month]); ++month);
        int dayOfWeek = -1;
        if (!parts[nwday].isEmpty())
        {
            // Look up the weekday name
            while ((++dayOfWeek < 7) && (shortDay[dayOfWeek] != parts[nwday]));
            if (dayOfWeek >= 7)
                for (dayOfWeek = 0; (dayOfWeek < 7) && (longDay[dayOfWeek] != parts[nwday]); ++dayOfWeek);
        }

        //       if (month >= 12 || dayOfWeek >= 7
        //       ||  (dayOfWeek < 0  &&  format == RFCDateDay))
        //         return QDateTime;
        const int i = parts[nyear].size();
        if (i < 4)
        {
            // It's an obsolete year specification with less than 4 digits
            year += ((i == 2) && (year < 50)) ? 2000 : 1900;
        }

        // Parse the UTC offset part
        int offset = 0;           // set default to '-0000'
        bool negOffset = false;
        if (parts.count() > 10)
        {
            rx = QRegularExpression {"^([+-])(\\d\\d)(\\d\\d)$"};
            if (parts[10].indexOf(rx, 0, &rxMatch) == 0)
            {
                // It's a UTC offset ±hhmm
                parts = rxMatch.capturedTexts();
                offset = parts[2].toInt(&ok[0]) * 3600;
                const int offsetMin = parts[3].toInt(&ok[1]);
                if (!ok[0] || !ok[1] || offsetMin > 59)
                    return {};
                offset += offsetMin * 60;
                negOffset = (parts[1] == QLatin1String("-"));
                if (negOffset)
                    offset = -offset;
            }
            else
            {
                // Check for an obsolete time zone name
                const QByteArray zone = parts[10].toLatin1();
                if ((zone.length() == 1) && (isalpha(zone[0])) && (toupper(zone[0]) != 'J'))
                {
                    negOffset = true;    // military zone: RFC 2822 treats as '-0000'
                }
                else if ((zone != "UT") && (zone != "GMT"))
                { // treated as '+0000'
                    offset = (zone == "EDT")
                            ? -4 * 3600
                            : ((zone == "EST") || (zone == "CDT"))
                              ? -5 * 3600
                              : ((zone == "CST") || (zone == "MDT"))
                                ? -6 * 3600
                                : ((zone == "MST") || (zone == "PDT"))
                                  ? -7 * 3600
                                  : (zone == "PST")
                                    ? -8 * 3600
                                    : 0;
                    if (!offset)
                    {
                        // Check for any other alphabetic time zone
                        bool nonalpha = false;
                        for (int i = 0, end = zone.size(); (i < end) && !nonalpha; ++i)
                            nonalpha = !isalpha(zone[i]);
                        if (nonalpha)
                            return {};
                        // TODO: Attempt to recognize the time zone abbreviation?
                        negOffset = true;    // unknown time zone: RFC 2822 treats as '-0000'
                    }
                }
            }
        }

        const QDate qDate(year, month + 1, day);   // convert date, and check for out-of-range
        if (!qDate.isValid())
            return QDateTime::currentDateTime();

        const QTime qTime(hour, minute, second);
        QDateTime result(qDate, qTime, Qt::UTC);
        if (offset)
            result = result.addSecs(-offset);
        if (!result.isValid())
            return QDateTime::currentDateTime();    // invalid date/time

        if (leapSecond)
        {
            // Validate a leap second time. Leap seconds are inserted after 23:59:59 UTC.
            // Convert the time to UTC and check that it is 00:00:00.
            if ((hour*3600 + minute*60 + 60 - offset + 86400*5) % 86400)   // (max abs(offset) is 100 hours)
                return QDateTime::currentDateTime();    // the time isn't the last second of the day
        }

        return result;
    }
}

using namespace RSS::Private;

const int ParsingResultTypeId = qRegisterMetaType<ParsingResult>();

Parser::Parser(const QString lastBuildDate)
{
    m_result.lastBuildDate = lastBuildDate;
}

void Parser::parse(const QByteArray &feedData)
{
    QMetaObject::invokeMethod(this, [this, feedData]() { parse_impl(feedData); }
                              , Qt::QueuedConnection);
}

// read and create items from a rss document
void Parser::parse_impl(const QByteArray &feedData)
{
    QXmlStreamReader xml(feedData);
    XmlStreamEntityResolver resolver;
    xml.setEntityResolver(&resolver);
    bool foundChannel = false;

    while (xml.readNextStartElement())
    {
        if (xml.name() == QLatin1String("rss"))
        {
            // Find channels
            while (xml.readNextStartElement())
            {
                if (xml.name() == QLatin1String("channel"))
                {
                    parseRSSChannel(xml);
                    foundChannel = true;
                    break;
                }

                qDebug() << "Skip rss item: " << xml.name();
                xml.skipCurrentElement();
            }
            break;
        }
        if (xml.name() == QLatin1String("feed"))
        { // Atom feed
            parseAtomChannel(xml);
            foundChannel = true;
            break;
        }

        qDebug() << "Skip root item: " << xml.name();
        xml.skipCurrentElement();
    }

    if (xml.hasError())
    {
        m_result.error = tr("%1 (line: %2, column: %3, offset: %4).")
                .arg(xml.errorString()).arg(xml.lineNumber())
                .arg(xml.columnNumber()).arg(xml.characterOffset());
    }
    else if (!foundChannel)
    {
        m_result.error = tr("Invalid RSS feed.");
    }

    emit finished(m_result);
    m_result.articles.clear(); // clear articles only
    m_articleIDs.clear();
}

void Parser::parseRssArticle(QXmlStreamReader &xml)
{
    QVariantHash article;
    QString altTorrentUrl;

    while (!xml.atEnd())
    {
        xml.readNext();
        const QString name(xml.name().toString());

        if (xml.isEndElement() && (name == QLatin1String("item")))
            break;

        if (xml.isStartElement())
        {
            if (name == QLatin1String("title"))
            {
                article[Article::KeyTitle] = xml.readElementText().trimmed();
            }
            else if (name == QLatin1String("enclosure"))
            {
                if (xml.attributes().value("type") == QLatin1String("application/x-bittorrent"))
                    article[Article::KeyTorrentURL] = xml.attributes().value(QLatin1String("url")).toString();
                else if (xml.attributes().value("type").isEmpty())
                    altTorrentUrl = xml.attributes().value(QLatin1String("url")).toString();
            }
            else if (name == QLatin1String("link"))
            {
                const QString text {xml.readElementText().trimmed()};
                if (text.startsWith(QLatin1String("magnet:"), Qt::CaseInsensitive))
                    article[Article::KeyTorrentURL] = text; // magnet link instead of a news URL
                else
                    article[Article::KeyLink] = text;
            }
            else if (name == QLatin1String("description"))
            {
                article[Article::KeyDescription] = xml.readElementText(QXmlStreamReader::IncludeChildElements);
            }
            else if (name == QLatin1String("pubDate"))
            {
                article[Article::KeyDate] = parseDate(xml.readElementText().trimmed());
            }
            else if (name == QLatin1String("author"))
            {
                article[Article::KeyAuthor] = xml.readElementText().trimmed();
            }
            else if (name == QLatin1String("guid"))
            {
                article[Article::KeyId] = xml.readElementText().trimmed();
            }
            else
            {
                article[name] = xml.readElementText(QXmlStreamReader::IncludeChildElements);
            }
        }
    }

    if (article[Article::KeyTorrentURL].toString().isEmpty())
        article[Article::KeyTorrentURL] = altTorrentUrl;

    addArticle(article);
}

void Parser::parseRSSChannel(QXmlStreamReader &xml)
{
    while (!xml.atEnd())
    {
        xml.readNext();

        if (xml.isStartElement())
        {
            if (xml.name() == QLatin1String("title"))
            {
                m_result.title = xml.readElementText();
            }
            else if (xml.name() == QLatin1String("lastBuildDate"))
            {
                const QString lastBuildDate = xml.readElementText();
                if (!lastBuildDate.isEmpty())
                {
                    if (m_result.lastBuildDate == lastBuildDate)
                    {
                        qDebug() << "The RSS feed has not changed since last time, aborting parsing.";
                        return;
                    }
                    m_result.lastBuildDate = lastBuildDate;
                }
            }
            else if (xml.name() == QLatin1String("item"))
            {
                parseRssArticle(xml);
            }
        }
    }
}

void Parser::parseAtomArticle(QXmlStreamReader &xml)
{
    QVariantHash article;
    bool doubleContent = false;

    while (!xml.atEnd())
    {
        xml.readNext();
        const QString name(xml.name().toString());

        if (xml.isEndElement() && (name == QLatin1String("entry")))
            break;

        if (xml.isStartElement())
        {
            if (name == QLatin1String("title"))
            {
                article[Article::KeyTitle] = xml.readElementText().trimmed();
            }
            else if (name == QLatin1String("link"))
            {
                const QString link = (xml.attributes().isEmpty()
                                ? xml.readElementText().trimmed()
                                : xml.attributes().value(QLatin1String("href")).toString());

                if (link.startsWith(QLatin1String("magnet:"), Qt::CaseInsensitive))
                    article[Article::KeyTorrentURL] = link; // magnet link instead of a news URL
                else
                    // Atom feeds can have relative links, work around this and
                    // take the stress of figuring article full URI from UI
                    // Assemble full URI
                    article[Article::KeyLink] = (m_baseUrl.isEmpty() ? link : m_baseUrl + link);

            }
            else if ((name == QLatin1String("summary")) || (name == QLatin1String("content")))
            {
                if (doubleContent)
                { // Duplicate content -> ignore
                    xml.skipCurrentElement();
                    continue;
                }

                // Try to also parse broken articles, which don't use html '&' escapes
                // Actually works great for non-broken content too
                const QString feedText = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
                if (!feedText.isEmpty())
                {
                    article[Article::KeyDescription] = feedText;
                    doubleContent = true;
                }
            }
            else if (name == QLatin1String("updated"))
            {
                // ATOM uses standard compliant date, don't do fancy stuff
                const QDateTime articleDate = QDateTime::fromString(xml.readElementText().trimmed(), Qt::ISODate);
                article[Article::KeyDate] = (articleDate.isValid() ? articleDate : QDateTime::currentDateTime());
            }
            else if (name == QLatin1String("author"))
            {
                while (xml.readNextStartElement())
                {
                    if (xml.name() == QLatin1String("name"))
                        article[Article::KeyAuthor] = xml.readElementText().trimmed();
                    else
                        xml.skipCurrentElement();
                }
            }
            else if (name == QLatin1String("id"))
            {
                article[Article::KeyId] = xml.readElementText().trimmed();
            }
            else
            {
                article[name] = xml.readElementText(QXmlStreamReader::IncludeChildElements);
            }
        }
    }

    addArticle(article);
}

void Parser::parseAtomChannel(QXmlStreamReader &xml)
{
    m_baseUrl = xml.attributes().value("xml:base").toString();

    while (!xml.atEnd())
    {
        xml.readNext();

        if (xml.isStartElement())
        {
            if (xml.name() == QLatin1String("title"))
            {
                m_result.title = xml.readElementText();
            }
            else if (xml.name() == QLatin1String("updated"))
            {
                const QString lastBuildDate = xml.readElementText();
                if (!lastBuildDate.isEmpty())
                {
                    if (m_result.lastBuildDate == lastBuildDate)
                    {
                        qDebug() << "The RSS feed has not changed since last time, aborting parsing.";
                        return;
                    }
                    m_result.lastBuildDate = lastBuildDate;
                }
            }
            else if (xml.name() == QLatin1String("entry"))
            {
                parseAtomArticle(xml);
            }
        }
    }
}

void Parser::addArticle(QVariantHash article)
{
    QVariant &torrentURL = article[Article::KeyTorrentURL];
    if (torrentURL.toString().isEmpty())
        torrentURL = article.value(Article::KeyLink);

    // If item does not have an ID, fall back to some other identifier.
    QVariant &localId = article[Article::KeyId];
    if (localId.toString().isEmpty())
    {
        localId = article.value(Article::KeyTorrentURL);
        if (localId.toString().isEmpty())
        {
            localId = article.value(Article::KeyTitle);
            if (localId.toString().isEmpty())
            {
                // The article could not be uniquely identified
                // since it has no appropriate data.
                // Just ignore it.
                return;
            }
        }
    }

    if (m_articleIDs.contains(localId.toString()))
    {
        // The article could not be uniquely identified
        // since the Feed has duplicate identifiers.
        // Just ignore it.
        return;
    }

    m_articleIDs.insert(localId.toString());
    m_result.articles.prepend(article);
}
