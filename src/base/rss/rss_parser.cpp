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

#include "base/global.h"
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
                {u"nbsp"_qs,   u"&#160;"_qs}, // no-break space = non-breaking space, U+00A0 ISOnum
                {u"iexcl"_qs,  u"&#161;"_qs}, // inverted exclamation mark, U+00A1 ISOnum
                {u"cent"_qs,   u"&#162;"_qs}, // cent sign, U+00A2 ISOnum
                {u"pound"_qs,  u"&#163;"_qs}, // pound sign, U+00A3 ISOnum
                {u"curren"_qs, u"&#164;"_qs}, // currency sign, U+00A4 ISOnum
                {u"yen"_qs,    u"&#165;"_qs}, // yen sign = yuan sign, U+00A5 ISOnum
                {u"brvbar"_qs, u"&#166;"_qs}, // broken bar = broken vertical bar, U+00A6 ISOnum
                {u"sect"_qs,   u"&#167;"_qs}, // section sign, U+00A7 ISOnum
                {u"uml"_qs,    u"&#168;"_qs}, // diaeresis = spacing diaeresis, U+00A8 ISOdia
                {u"copy"_qs,   u"&#169;"_qs}, // copyright sign, U+00A9 ISOnum
                {u"ordf"_qs,   u"&#170;"_qs}, // feminine ordinal indicator, U+00AA ISOnum
                {u"laquo"_qs,  u"&#171;"_qs}, // left-pointing double angle quotation mark = left pointing guillemet, U+00AB ISOnum
                {u"not"_qs,    u"&#172;"_qs}, // not sign = angled dash, U+00AC ISOnum
                {u"shy"_qs,    u"&#173;"_qs}, // soft hyphen = discretionary hyphen, U+00AD ISOnum
                {u"reg"_qs,    u"&#174;"_qs}, // registered sign = registered trade mark sign, U+00AE ISOnum
                {u"macr"_qs,   u"&#175;"_qs}, // macron = spacing macron = overline = APL overbar, U+00AF ISOdia
                {u"deg"_qs,    u"&#176;"_qs}, // degree sign, U+00B0 ISOnum
                {u"plusmn"_qs, u"&#177;"_qs}, // plus-minus sign = plus-or-minus sign, U+00B1 ISOnum
                {u"sup2"_qs,   u"&#178;"_qs}, // superscript two = superscript digit two = squared, U+00B2 ISOnum
                {u"sup3"_qs,   u"&#179;"_qs}, // superscript three = superscript digit three = cubed, U+00B3 ISOnum
                {u"acute"_qs,  u"&#180;"_qs}, // acute accent = spacing acute, U+00B4 ISOdia
                {u"micro"_qs,  u"&#181;"_qs}, // micro sign, U+00B5 ISOnum
                {u"para"_qs,   u"&#182;"_qs}, // pilcrow sign = paragraph sign, U+00B6 ISOnum
                {u"middot"_qs, u"&#183;"_qs}, // middle dot = Georgian comma = Greek middle dot, U+00B7 ISOnum
                {u"cedil"_qs,  u"&#184;"_qs}, // cedilla = spacing cedilla, U+00B8 ISOdia
                {u"sup1"_qs,   u"&#185;"_qs}, // superscript one = superscript digit one, U+00B9 ISOnum
                {u"ordm"_qs,   u"&#186;"_qs}, // masculine ordinal indicator, U+00BA ISOnum
                {u"raquo"_qs,  u"&#187;"_qs}, // right-pointing double angle quotation mark = right pointing guillemet, U+00BB ISOnum
                {u"frac14"_qs, u"&#188;"_qs}, // vulgar fraction one quarter = fraction one quarter, U+00BC ISOnum
                {u"frac12"_qs, u"&#189;"_qs}, // vulgar fraction one half = fraction one half, U+00BD ISOnum
                {u"frac34"_qs, u"&#190;"_qs}, // vulgar fraction three quarters = fraction three quarters, U+00BE ISOnum
                {u"iquest"_qs, u"&#191;"_qs}, // inverted question mark = turned question mark, U+00BF ISOnum
                {u"Agrave"_qs, u"&#192;"_qs}, // latin capital letter A with grave = latin capital letter A grave, U+00C0 ISOlat1
                {u"Aacute"_qs, u"&#193;"_qs}, // latin capital letter A with acute, U+00C1 ISOlat1
                {u"Acirc"_qs,  u"&#194;"_qs}, // latin capital letter A with circumflex, U+00C2 ISOlat1
                {u"Atilde"_qs, u"&#195;"_qs}, // latin capital letter A with tilde, U+00C3 ISOlat1
                {u"Auml"_qs,   u"&#196;"_qs}, // latin capital letter A with diaeresis, U+00C4 ISOlat1
                {u"Aring"_qs,  u"&#197;"_qs}, // latin capital letter A with ring above = latin capital letter A ring, U+00C5 ISOlat1
                {u"AElig"_qs,  u"&#198;"_qs}, // latin capital letter AE = latin capital ligature AE, U+00C6 ISOlat1
                {u"Ccedil"_qs, u"&#199;"_qs}, // latin capital letter C with cedilla, U+00C7 ISOlat1
                {u"Egrave"_qs, u"&#200;"_qs}, // latin capital letter E with grave, U+00C8 ISOlat1
                {u"Eacute"_qs, u"&#201;"_qs}, // latin capital letter E with acute, U+00C9 ISOlat1
                {u"Ecirc"_qs,  u"&#202;"_qs}, // latin capital letter E with circumflex, U+00CA ISOlat1
                {u"Euml"_qs,   u"&#203;"_qs}, // latin capital letter E with diaeresis, U+00CB ISOlat1
                {u"Igrave"_qs, u"&#204;"_qs}, // latin capital letter I with grave, U+00CC ISOlat1
                {u"Iacute"_qs, u"&#205;"_qs}, // latin capital letter I with acute, U+00CD ISOlat1
                {u"Icirc"_qs,  u"&#206;"_qs}, // latin capital letter I with circumflex, U+00CE ISOlat1
                {u"Iuml"_qs,   u"&#207;"_qs}, // latin capital letter I with diaeresis, U+00CF ISOlat1
                {u"ETH"_qs,    u"&#208;"_qs}, // latin capital letter ETH, U+00D0 ISOlat1
                {u"Ntilde"_qs, u"&#209;"_qs}, // latin capital letter N with tilde, U+00D1 ISOlat1
                {u"Ograve"_qs, u"&#210;"_qs}, // latin capital letter O with grave, U+00D2 ISOlat1
                {u"Oacute"_qs, u"&#211;"_qs}, // latin capital letter O with acute, U+00D3 ISOlat1
                {u"Ocirc"_qs,  u"&#212;"_qs}, // latin capital letter O with circumflex, U+00D4 ISOlat1
                {u"Otilde"_qs, u"&#213;"_qs}, // latin capital letter O with tilde, U+00D5 ISOlat1
                {u"Ouml"_qs,   u"&#214;"_qs}, // latin capital letter O with diaeresis, U+00D6 ISOlat1
                {u"times"_qs,  u"&#215;"_qs}, // multiplication sign, U+00D7 ISOnum
                {u"Oslash"_qs, u"&#216;"_qs}, // latin capital letter O with stroke = latin capital letter O slash, U+00D8 ISOlat1
                {u"Ugrave"_qs, u"&#217;"_qs}, // latin capital letter U with grave, U+00D9 ISOlat1
                {u"Uacute"_qs, u"&#218;"_qs}, // latin capital letter U with acute, U+00DA ISOlat1
                {u"Ucirc"_qs,  u"&#219;"_qs}, // latin capital letter U with circumflex, U+00DB ISOlat1
                {u"Uuml"_qs,   u"&#220;"_qs}, // latin capital letter U with diaeresis, U+00DC ISOlat1
                {u"Yacute"_qs, u"&#221;"_qs}, // latin capital letter Y with acute, U+00DD ISOlat1
                {u"THORN"_qs,  u"&#222;"_qs}, // latin capital letter THORN, U+00DE ISOlat1
                {u"szlig"_qs,  u"&#223;"_qs}, // latin small letter sharp s = ess-zed, U+00DF ISOlat1
                {u"agrave"_qs, u"&#224;"_qs}, // latin small letter a with grave = latin small letter a grave, U+00E0 ISOlat1
                {u"aacute"_qs, u"&#225;"_qs}, // latin small letter a with acute, U+00E1 ISOlat1
                {u"acirc"_qs,  u"&#226;"_qs}, // latin small letter a with circumflex, U+00E2 ISOlat1
                {u"atilde"_qs, u"&#227;"_qs}, // latin small letter a with tilde, U+00E3 ISOlat1
                {u"auml"_qs,   u"&#228;"_qs}, // latin small letter a with diaeresis, U+00E4 ISOlat1
                {u"aring"_qs,  u"&#229;"_qs}, // latin small letter a with ring above = latin small letter a ring, U+00E5 ISOlat1
                {u"aelig"_qs,  u"&#230;"_qs}, // latin small letter ae = latin small ligature ae, U+00E6 ISOlat1
                {u"ccedil"_qs, u"&#231;"_qs}, // latin small letter c with cedilla, U+00E7 ISOlat1
                {u"egrave"_qs, u"&#232;"_qs}, // latin small letter e with grave, U+00E8 ISOlat1
                {u"eacute"_qs, u"&#233;"_qs}, // latin small letter e with acute, U+00E9 ISOlat1
                {u"ecirc"_qs,  u"&#234;"_qs}, // latin small letter e with circumflex, U+00EA ISOlat1
                {u"euml"_qs,   u"&#235;"_qs}, // latin small letter e with diaeresis, U+00EB ISOlat1
                {u"igrave"_qs, u"&#236;"_qs}, // latin small letter i with grave, U+00EC ISOlat1
                {u"iacute"_qs, u"&#237;"_qs}, // latin small letter i with acute, U+00ED ISOlat1
                {u"icirc"_qs,  u"&#238;"_qs}, // latin small letter i with circumflex, U+00EE ISOlat1
                {u"iuml"_qs,   u"&#239;"_qs}, // latin small letter i with diaeresis, U+00EF ISOlat1
                {u"eth"_qs,    u"&#240;"_qs}, // latin small letter eth, U+00F0 ISOlat1
                {u"ntilde"_qs, u"&#241;"_qs}, // latin small letter n with tilde, U+00F1 ISOlat1
                {u"ograve"_qs, u"&#242;"_qs}, // latin small letter o with grave, U+00F2 ISOlat1
                {u"oacute"_qs, u"&#243;"_qs}, // latin small letter o with acute, U+00F3 ISOlat1
                {u"ocirc"_qs,  u"&#244;"_qs}, // latin small letter o with circumflex, U+00F4 ISOlat1
                {u"otilde"_qs, u"&#245;"_qs}, // latin small letter o with tilde, U+00F5 ISOlat1
                {u"ouml"_qs,   u"&#246;"_qs}, // latin small letter o with diaeresis, U+00F6 ISOlat1
                {u"divide"_qs, u"&#247;"_qs}, // division sign, U+00F7 ISOnum
                {u"oslash"_qs, u"&#248;"_qs}, // latin small letter o with stroke, = latin small letter o slash, U+00F8 ISOlat1
                {u"ugrave"_qs, u"&#249;"_qs}, // latin small letter u with grave, U+00F9 ISOlat1
                {u"uacute"_qs, u"&#250;"_qs}, // latin small letter u with acute, U+00FA ISOlat1
                {u"ucirc"_qs,  u"&#251;"_qs}, // latin small letter u with circumflex, U+00FB ISOlat1
                {u"uuml"_qs,   u"&#252;"_qs}, // latin small letter u with diaeresis, U+00FC ISOlat1
                {u"yacute"_qs, u"&#253;"_qs}, // latin small letter y with acute, U+00FD ISOlat1
                {u"thorn"_qs,  u"&#254;"_qs}, // latin small letter thorn, U+00FE ISOlat1
                {u"yuml"_qs,   u"&#255;"_qs}, // latin small letter y with diaeresis, U+00FF ISOlat1

                // Latin Extended-A
                {u"OElig"_qs,   u"&#338;"_qs}, //  latin capital ligature OE, U+0152 ISOlat2
                {u"oelig"_qs,   u"&#339;"_qs}, //  latin small ligature oe, U+0153 ISOlat2
                // ligature is a misnomer, this is a separate character in some languages
                {u"Scaron"_qs,  u"&#352;"_qs}, //  latin capital letter S with caron, U+0160 ISOlat2
                {u"scaron"_qs,  u"&#353;"_qs}, //  latin small letter s with caron, U+0161 ISOlat2
                {u"Yuml"_qs,    u"&#376;"_qs}, //  latin capital letter Y with diaeresis, U+0178 ISOlat2

                // Spacing Modifier Letters
                {u"circ"_qs,    u"&#710;"_qs}, //  modifier letter circumflex accent, U+02C6 ISOpub
                {u"tilde"_qs,   u"&#732;"_qs}, //  small tilde, U+02DC ISOdia

                // General Punctuation
                {u"ensp"_qs,    u"&#8194;"_qs}, // en space, U+2002 ISOpub
                {u"emsp"_qs,    u"&#8195;"_qs}, // em space, U+2003 ISOpub
                {u"thinsp"_qs,  u"&#8201;"_qs}, // thin space, U+2009 ISOpub
                {u"zwnj"_qs,    u"&#8204;"_qs}, // zero width non-joiner, U+200C NEW RFC 2070
                {u"zwj"_qs,     u"&#8205;"_qs}, // zero width joiner, U+200D NEW RFC 2070
                {u"lrm"_qs,     u"&#8206;"_qs}, // left-to-right mark, U+200E NEW RFC 2070
                {u"rlm"_qs,     u"&#8207;"_qs}, // right-to-left mark, U+200F NEW RFC 2070
                {u"ndash"_qs,   u"&#8211;"_qs}, // en dash, U+2013 ISOpub
                {u"mdash"_qs,   u"&#8212;"_qs}, // em dash, U+2014 ISOpub
                {u"lsquo"_qs,   u"&#8216;"_qs}, // left single quotation mark, U+2018 ISOnum
                {u"rsquo"_qs,   u"&#8217;"_qs}, // right single quotation mark, U+2019 ISOnum
                {u"sbquo"_qs,   u"&#8218;"_qs}, // single low-9 quotation mark, U+201A NEW
                {u"ldquo"_qs,   u"&#8220;"_qs}, // left double quotation mark, U+201C ISOnum
                {u"rdquo"_qs,   u"&#8221;"_qs}, // right double quotation mark, U+201D ISOnum
                {u"bdquo"_qs,   u"&#8222;"_qs}, // double low-9 quotation mark, U+201E NEW
                {u"dagger"_qs,  u"&#8224;"_qs}, // dagger, U+2020 ISOpub
                {u"Dagger"_qs,  u"&#8225;"_qs}, // double dagger, U+2021 ISOpub
                {u"permil"_qs,  u"&#8240;"_qs}, // per mille sign, U+2030 ISOtech
                {u"lsaquo"_qs,  u"&#8249;"_qs}, // single left-pointing angle quotation mark, U+2039 ISO proposed
                // lsaquo is proposed but not yet ISO standardized
                {u"rsaquo"_qs,  u"&#8250;"_qs}, // single right-pointing angle quotation mark, U+203A ISO proposed
                // rsaquo is proposed but not yet ISO standardized

                // Currency Symbols
                {u"euro"_qs,   u"&#8364;"_qs}, //  euro sign, U+20AC NEW

                // Latin Extended-B
                {u"fnof"_qs,     u"&#402;"_qs}, // latin small letter f with hook = function   = florin, U+0192 ISOtech

                // Greek
                {u"Alpha"_qs,    u"&#913;"_qs}, // greek capital letter alpha, U+0391
                {u"Beta"_qs,     u"&#914;"_qs}, // greek capital letter beta, U+0392
                {u"Gamma"_qs,    u"&#915;"_qs}, // greek capital letter gamma, U+0393 ISOgrk3
                {u"Delta"_qs,    u"&#916;"_qs}, // greek capital letter delta, U+0394 ISOgrk3
                {u"Epsilon"_qs,  u"&#917;"_qs}, // greek capital letter epsilon, U+0395
                {u"Zeta"_qs,     u"&#918;"_qs}, // greek capital letter zeta, U+0396
                {u"Eta"_qs,      u"&#919;"_qs}, // greek capital letter eta, U+0397
                {u"Theta"_qs,    u"&#920;"_qs}, // greek capital letter theta, U+0398 ISOgrk3
                {u"Iota"_qs,     u"&#921;"_qs}, // greek capital letter iota, U+0399
                {u"Kappa"_qs,    u"&#922;"_qs}, // greek capital letter kappa, U+039A
                {u"Lambda"_qs,   u"&#923;"_qs}, // greek capital letter lamda, U+039B ISOgrk3
                {u"Mu"_qs,       u"&#924;"_qs}, // greek capital letter mu, U+039C
                {u"Nu"_qs,       u"&#925;"_qs}, // greek capital letter nu, U+039D
                {u"Xi"_qs,       u"&#926;"_qs}, // greek capital letter xi, U+039E ISOgrk3
                {u"Omicron"_qs,  u"&#927;"_qs}, // greek capital letter omicron, U+039F
                {u"Pi"_qs,       u"&#928;"_qs}, // greek capital letter pi, U+03A0 ISOgrk3
                {u"Rho"_qs,      u"&#929;"_qs}, // greek capital letter rho, U+03A1
                {u"Sigma"_qs,    u"&#931;"_qs}, // greek capital letter sigma, U+03A3 ISOgrk3
                {u"Tau"_qs,      u"&#932;"_qs}, // greek capital letter tau, U+03A4
                {u"Upsilon"_qs,  u"&#933;"_qs}, // greek capital letter upsilon, U+03A5 ISOgrk3
                {u"Phi"_qs,      u"&#934;"_qs}, // greek capital letter phi, U+03A6 ISOgrk3
                {u"Chi"_qs,      u"&#935;"_qs}, // greek capital letter chi, U+03A7
                {u"Psi"_qs,      u"&#936;"_qs}, // greek capital letter psi, U+03A8 ISOgrk3
                {u"Omega"_qs,    u"&#937;"_qs}, // greek capital letter omega, U+03A9 ISOgrk3
                {u"alpha"_qs,    u"&#945;"_qs}, // greek small letter alpha, U+03B1 ISOgrk3
                {u"beta"_qs,     u"&#946;"_qs}, // greek small letter beta, U+03B2 ISOgrk3
                {u"gamma"_qs,    u"&#947;"_qs}, // greek small letter gamma, U+03B3 ISOgrk3
                {u"delta"_qs,    u"&#948;"_qs}, // greek small letter delta, U+03B4 ISOgrk3
                {u"epsilon"_qs,  u"&#949;"_qs}, // greek small letter epsilon, U+03B5 ISOgrk3
                {u"zeta"_qs,     u"&#950;"_qs}, // greek small letter zeta, U+03B6 ISOgrk3
                {u"eta"_qs,      u"&#951;"_qs}, // greek small letter eta, U+03B7 ISOgrk3
                {u"theta"_qs,    u"&#952;"_qs}, // greek small letter theta, U+03B8 ISOgrk3
                {u"iota"_qs,     u"&#953;"_qs}, // greek small letter iota, U+03B9 ISOgrk3
                {u"kappa"_qs,    u"&#954;"_qs}, // greek small letter kappa, U+03BA ISOgrk3
                {u"lambda"_qs,   u"&#955;"_qs}, // greek small letter lamda, U+03BB ISOgrk3
                {u"mu"_qs,       u"&#956;"_qs}, // greek small letter mu, U+03BC ISOgrk3
                {u"nu"_qs,       u"&#957;"_qs}, // greek small letter nu, U+03BD ISOgrk3
                {u"xi"_qs,       u"&#958;"_qs}, // greek small letter xi, U+03BE ISOgrk3
                {u"omicron"_qs,  u"&#959;"_qs}, // greek small letter omicron, U+03BF NEW
                {u"pi"_qs,       u"&#960;"_qs}, // greek small letter pi, U+03C0 ISOgrk3
                {u"rho"_qs,      u"&#961;"_qs}, // greek small letter rho, U+03C1 ISOgrk3
                {u"sigmaf"_qs,   u"&#962;"_qs}, // greek small letter final sigma, U+03C2 ISOgrk3
                {u"sigma"_qs,    u"&#963;"_qs}, // greek small letter sigma, U+03C3 ISOgrk3
                {u"tau"_qs,      u"&#964;"_qs}, // greek small letter tau, U+03C4 ISOgrk3
                {u"upsilon"_qs,  u"&#965;"_qs}, // greek small letter upsilon, U+03C5 ISOgrk3
                {u"phi"_qs,      u"&#966;"_qs}, // greek small letter phi, U+03C6 ISOgrk3
                {u"chi"_qs,      u"&#967;"_qs}, // greek small letter chi, U+03C7 ISOgrk3
                {u"psi"_qs,      u"&#968;"_qs}, // greek small letter psi, U+03C8 ISOgrk3
                {u"omega"_qs,    u"&#969;"_qs}, // greek small letter omega, U+03C9 ISOgrk3
                {u"thetasym"_qs, u"&#977;"_qs}, // greek theta symbol, U+03D1 NEW
                {u"upsih"_qs,    u"&#978;"_qs}, // greek upsilon with hook symbol, U+03D2 NEW
                {u"piv"_qs,      u"&#982;"_qs}, // greek pi symbol, U+03D6 ISOgrk3

                // General Punctuation
                {u"bull"_qs,     u"&#8226;"_qs}, // bullet = black small circle, U+2022 ISOpub
                // bullet is NOT the same as bullet operator, U+2219
                {u"hellip"_qs,   u"&#8230;"_qs}, // horizontal ellipsis = three dot leader, U+2026 ISOpub
                {u"prime"_qs,    u"&#8242;"_qs}, // prime = minutes = feet, U+2032 ISOtech
                {u"Prime"_qs,    u"&#8243;"_qs}, // double prime = seconds = inches, U+2033 ISOtech
                {u"oline"_qs,    u"&#8254;"_qs}, // overline = spacing overscore, U+203E NEW
                {u"frasl"_qs,    u"&#8260;"_qs}, // fraction slash, U+2044 NEW

                // Letterlike Symbols
                {u"weierp"_qs,   u"&#8472;"_qs}, // script capital P = power set = Weierstrass p, U+2118 ISOamso
                {u"image"_qs,    u"&#8465;"_qs}, // black-letter capital I = imaginary part, U+2111 ISOamso
                {u"real"_qs,     u"&#8476;"_qs}, // black-letter capital R = real part symbol, U+211C ISOamso
                {u"trade"_qs,    u"&#8482;"_qs}, // trade mark sign, U+2122 ISOnum
                {u"alefsym"_qs,  u"&#8501;"_qs}, // alef symbol = first transfinite cardinal, U+2135 NEW
                // alef symbol is NOT the same as hebrew letter alef,
                //     U+05D0 although the same glyph could be used to depict both characters

                // Arrows
                {u"larr"_qs,     u"&#8592;"_qs}, // leftwards arrow, U+2190 ISOnum
                {u"uarr"_qs,     u"&#8593;"_qs}, // upwards arrow, U+2191 ISOnum
                {u"rarr"_qs,     u"&#8594;"_qs}, // rightwards arrow, U+2192 ISOnum
                {u"darr"_qs,     u"&#8595;"_qs}, // downwards arrow, U+2193 ISOnum
                {u"harr"_qs,     u"&#8596;"_qs}, // left right arrow, U+2194 ISOamsa
                {u"crarr"_qs,    u"&#8629;"_qs}, // downwards arrow with corner leftwards = carriage return, U+21B5 NEW
                {u"lArr"_qs,     u"&#8656;"_qs}, // leftwards double arrow, U+21D0 ISOtech
                // Unicode does not say that lArr is the same as the 'is implied by' arrow
                //    but also does not have any other character for that function. So lArr can
                //    be used for 'is implied by' as ISOtech suggests
                {u"uArr"_qs,     u"&#8657;"_qs}, // upwards double arrow, U+21D1 ISOamsa
                {u"rArr"_qs,     u"&#8658;"_qs}, // rightwards double arrow, U+21D2 ISOtech
                // Unicode does not say this is the 'implies' character but does not have
                //     another character with this function so rArr can be used for 'implies'
                //     as ISOtech suggests
                {u"dArr"_qs,     u"&#8659;"_qs}, // downwards double arrow, U+21D3 ISOamsa
                {u"hArr"_qs,     u"&#8660;"_qs}, // left right double arrow, U+21D4 ISOamsa

                // Mathematical Operators
                {u"forall"_qs,   u"&#8704;"_qs}, // for all, U+2200 ISOtech
                {u"part"_qs,     u"&#8706;"_qs}, // partial differential, U+2202 ISOtech
                {u"exist"_qs,    u"&#8707;"_qs}, // there exists, U+2203 ISOtech
                {u"empty"_qs,    u"&#8709;"_qs}, // empty set = null set, U+2205 ISOamso
                {u"nabla"_qs,    u"&#8711;"_qs}, // nabla = backward difference, U+2207 ISOtech
                {u"isin"_qs,     u"&#8712;"_qs}, // element of, U+2208 ISOtech
                {u"notin"_qs,    u"&#8713;"_qs}, // not an element of, U+2209 ISOtech
                {u"ni"_qs,       u"&#8715;"_qs}, // contains as member, U+220B ISOtech
                {u"prod"_qs,     u"&#8719;"_qs}, // n-ary product = product sign, U+220F ISOamsb
                // prod is NOT the same character as U+03A0 'greek capital letter pi' though
                //     the same glyph might be used for both
                {u"sum"_qs,      u"&#8721;"_qs}, // n-ary summation, U+2211 ISOamsb
                // sum is NOT the same character as U+03A3 'greek capital letter sigma'
                //     though the same glyph might be used for both
                {u"minus"_qs,    u"&#8722;"_qs}, // minus sign, U+2212 ISOtech
                {u"lowast"_qs,   u"&#8727;"_qs}, // asterisk operator, U+2217 ISOtech
                {u"radic"_qs,    u"&#8730;"_qs}, // square root = radical sign, U+221A ISOtech
                {u"prop"_qs,     u"&#8733;"_qs}, // proportional to, U+221D ISOtech
                {u"infin"_qs,    u"&#8734;"_qs}, // infinity, U+221E ISOtech
                {u"ang"_qs,      u"&#8736;"_qs}, // angle, U+2220 ISOamso
                {u"and"_qs,      u"&#8743;"_qs}, // logical and = wedge, U+2227 ISOtech
                {u"or"_qs,       u"&#8744;"_qs}, // logical or = vee, U+2228 ISOtech
                {u"cap"_qs,      u"&#8745;"_qs}, // intersection = cap, U+2229 ISOtech
                {u"cup"_qs,      u"&#8746;"_qs}, // union = cup, U+222A ISOtech
                {u"int"_qs,      u"&#8747;"_qs}, // integral, U+222B ISOtech
                {u"there4"_qs,   u"&#8756;"_qs}, // therefore, U+2234 ISOtech
                {u"sim"_qs,      u"&#8764;"_qs}, // tilde operator = varies with = similar to, U+223C ISOtech
                // tilde operator is NOT the same character as the tilde, U+007E,
                //     although the same glyph might be used to represent both
                {u"cong"_qs,     u"&#8773;"_qs}, // approximately equal to, U+2245 ISOtech
                {u"asymp"_qs,    u"&#8776;"_qs}, // almost equal to = asymptotic to, U+2248 ISOamsr
                {u"ne"_qs,       u"&#8800;"_qs}, // not equal to, U+2260 ISOtech
                {u"equiv"_qs,    u"&#8801;"_qs}, // identical to, U+2261 ISOtech
                {u"le"_qs,       u"&#8804;"_qs}, // less-than or equal to, U+2264 ISOtech
                {u"ge"_qs,       u"&#8805;"_qs}, // greater-than or equal to, U+2265 ISOtech
                {u"sub"_qs,      u"&#8834;"_qs}, // subset of, U+2282 ISOtech
                {u"sup"_qs,      u"&#8835;"_qs}, // superset of, U+2283 ISOtech
                {u"nsub"_qs,     u"&#8836;"_qs}, // not a subset of, U+2284 ISOamsn
                {u"sube"_qs,     u"&#8838;"_qs}, // subset of or equal to, U+2286 ISOtech
                {u"supe"_qs,     u"&#8839;"_qs}, // superset of or equal to, U+2287 ISOtech
                {u"oplus"_qs,    u"&#8853;"_qs}, // circled plus = direct sum, U+2295 ISOamsb
                {u"otimes"_qs,   u"&#8855;"_qs}, // circled times = vector product, U+2297 ISOamsb
                {u"perp"_qs,     u"&#8869;"_qs}, // up tack = orthogonal to = perpendicular, U+22A5 ISOtech
                {u"sdot"_qs,     u"&#8901;"_qs}, // dot operator, U+22C5 ISOamsb
                // dot operator is NOT the same character as U+00B7 middle dot

                // Miscellaneous Technical
                {u"lceil"_qs,    u"&#8968;"_qs}, // left ceiling = APL upstile, U+2308 ISOamsc
                {u"rceil"_qs,    u"&#8969;"_qs}, // right ceiling, U+2309 ISOamsc
                {u"lfloor"_qs,   u"&#8970;"_qs}, // left floor = APL downstile, U+230A ISOamsc
                {u"rfloor"_qs,   u"&#8971;"_qs}, // right floor, U+230B ISOamsc
                {u"lang"_qs,     u"&#9001;"_qs}, // left-pointing angle bracket = bra, U+2329 ISOtech
                // lang is NOT the same character as U+003C 'less than sign'
                //     or U+2039 'single left-pointing angle quotation mark'
                {u"rang"_qs,     u"&#9002;"_qs}, // right-pointing angle bracket = ket, U+232A ISOtech
                // rang is NOT the same character as U+003E 'greater than sign'
                //     or U+203A 'single right-pointing angle quotation mark'

                // Geometric Shapes
                {u"loz"_qs,      u"&#9674;"_qs}, // lozenge, U+25CA ISOpub

                // Miscellaneous Symbols
                {u"spades"_qs,   u"&#9824;"_qs}, // black spade suit, U+2660 ISOpub
                {u"clubs"_qs,    u"&#9827;"_qs}, // black club suit = shamrock, U+2663 ISOpub
                {u"hearts"_qs,   u"&#9829;"_qs}, // black heart suit = valentine, U+2665 ISOpub
                {u"diams"_qs,    u"&#9830;"_qs}  // black diamond suit, U+2666 ISOpub
            };
            return HTMLEntities.value(name);
        }
    };

    // Ported to Qt from KDElibs4
    QDateTime parseDate(const QString &string)
    {
        const char16_t shortDay[][4] =
        {
            u"Mon", u"Tue", u"Wed",
            u"Thu", u"Fri", u"Sat",
            u"Sun"
        };
        const char16_t longDay[][10] =
        {
            u"Monday", u"Tuesday", u"Wednesday",
            u"Thursday", u"Friday", u"Saturday",
            u"Sunday"
        };
        const char16_t shortMonth[][4] =
        {
            u"Jan", u"Feb", u"Mar", u"Apr",
            u"May", u"Jun", u"Jul", u"Aug",
            u"Sep", u"Oct", u"Nov", u"Dec"
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
        QRegularExpression rx {u"^(?:([A-Z][a-z]+),\\s*)?(\\d{1,2})(\\s+|-)([^-\\s]+)(\\s+|-)(\\d{2,4})\\s+(\\d\\d):(\\d\\d)(?::(\\d\\d))?\\s+(\\S+)$"_qs};
        QRegularExpressionMatch rxMatch;
        QStringList parts;
        if (str.indexOf(rx, 0, &rxMatch) == 0)
        {
            // Check that if date has '-' separators, both separators are '-'.
            parts = rxMatch.capturedTexts();
            const bool h1 = (parts[3] == u"-");
            const bool h2 = (parts[5] == u"-");
            if (h1 != h2)
                return QDateTime::currentDateTime();
        }
        else
        {
            // Check for the obsolete form "Wdy Mon DD HH:MM:SS YYYY"
            rx = QRegularExpression {u"^([A-Z][a-z]+)\\s+(\\S+)\\s+(\\d\\d)\\s+(\\d\\d):(\\d\\d):(\\d\\d)\\s+(\\d\\d\\d\\d)$"_qs};
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
            rx = QRegularExpression {u"^([+-])(\\d\\d)(\\d\\d)$"_qs};
            if (parts[10].indexOf(rx, 0, &rxMatch) == 0)
            {
                // It's a UTC offset ±hhmm
                parts = rxMatch.capturedTexts();
                offset = parts[2].toInt(&ok[0]) * 3600;
                const int offsetMin = parts[3].toInt(&ok[1]);
                if (!ok[0] || !ok[1] || offsetMin > 59)
                    return {};
                offset += offsetMin * 60;
                negOffset = (parts[1] == u"-");
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

const int PARSINGRESULT_TYPEID = qRegisterMetaType<RSS::Private::ParsingResult>();

RSS::Private::Parser::Parser(const QString lastBuildDate)
{
    m_result.lastBuildDate = lastBuildDate;
}

// read and create items from a rss document
void RSS::Private::Parser::parse(const QByteArray &feedData)
{
    QXmlStreamReader xml {feedData};
    XmlStreamEntityResolver resolver;
    xml.setEntityResolver(&resolver);
    bool foundChannel = false;

    while (xml.readNextStartElement())
    {
        if (xml.name() == u"rss")
        {
            // Find channels
            while (xml.readNextStartElement())
            {
                if (xml.name() == u"channel")
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
        if (xml.name() == u"feed")
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
    m_result.articles.clear();
    m_result.error.clear();
    m_articleIDs.clear();
}

void RSS::Private::Parser::parseRssArticle(QXmlStreamReader &xml)
{
    QVariantHash article;
    QString altTorrentUrl;

    while (!xml.atEnd())
    {
        xml.readNext();
        const QString name(xml.name().toString());

        if (xml.isEndElement() && (name == u"item"))
            break;

        if (xml.isStartElement())
        {
            if (name == u"title")
            {
                article[Article::KeyTitle] = xml.readElementText().trimmed();
            }
            else if (name == u"enclosure")
            {
                if (xml.attributes().value(u"type"_qs) == u"application/x-bittorrent")
                    article[Article::KeyTorrentURL] = xml.attributes().value(u"url"_qs).toString();
                else if (xml.attributes().value(u"type"_qs).isEmpty())
                    altTorrentUrl = xml.attributes().value(u"url"_qs).toString();
            }
            else if (name == u"link")
            {
                const QString text {xml.readElementText().trimmed()};
                if (text.startsWith(u"magnet:", Qt::CaseInsensitive))
                    article[Article::KeyTorrentURL] = text; // magnet link instead of a news URL
                else
                    article[Article::KeyLink] = text;
            }
            else if (name == u"description")
            {
                article[Article::KeyDescription] = xml.readElementText(QXmlStreamReader::IncludeChildElements);
            }
            else if (name == u"pubDate")
            {
                article[Article::KeyDate] = parseDate(xml.readElementText().trimmed());
            }
            else if (name == u"author")
            {
                article[Article::KeyAuthor] = xml.readElementText().trimmed();
            }
            else if (name == u"guid")
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

void RSS::Private::Parser::parseRSSChannel(QXmlStreamReader &xml)
{
    while (!xml.atEnd())
    {
        xml.readNext();

        if (xml.isStartElement())
        {
            if (xml.name() == u"title")
            {
                m_result.title = xml.readElementText();
            }
            else if (xml.name() == u"lastBuildDate")
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
            else if (xml.name() == u"item")
            {
                parseRssArticle(xml);
            }
        }
    }
}

void RSS::Private::Parser::parseAtomArticle(QXmlStreamReader &xml)
{
    QVariantHash article;
    bool doubleContent = false;

    while (!xml.atEnd())
    {
        xml.readNext();
        const QString name(xml.name().toString());

        if (xml.isEndElement() && (name == u"entry"))
            break;

        if (xml.isStartElement())
        {
            if (name == u"title")
            {
                article[Article::KeyTitle] = xml.readElementText().trimmed();
            }
            else if (name == u"link")
            {
                const QString link = (xml.attributes().isEmpty()
                                ? xml.readElementText().trimmed()
                                : xml.attributes().value(u"href"_qs).toString());

                if (link.startsWith(u"magnet:", Qt::CaseInsensitive))
                    article[Article::KeyTorrentURL] = link; // magnet link instead of a news URL
                else
                    // Atom feeds can have relative links, work around this and
                    // take the stress of figuring article full URI from UI
                    // Assemble full URI
                    article[Article::KeyLink] = (m_baseUrl.isEmpty() ? link : m_baseUrl + link);

            }
            else if ((name == u"summary") || (name == u"content"))
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
            else if (name == u"updated")
            {
                // ATOM uses standard compliant date, don't do fancy stuff
                const QDateTime articleDate = QDateTime::fromString(xml.readElementText().trimmed(), Qt::ISODate);
                article[Article::KeyDate] = (articleDate.isValid() ? articleDate : QDateTime::currentDateTime());
            }
            else if (name == u"author")
            {
                while (xml.readNextStartElement())
                {
                    if (xml.name() == u"name")
                        article[Article::KeyAuthor] = xml.readElementText().trimmed();
                    else
                        xml.skipCurrentElement();
                }
            }
            else if (name == u"id")
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

void RSS::Private::Parser::parseAtomChannel(QXmlStreamReader &xml)
{
    m_baseUrl = xml.attributes().value(u"xml:base"_qs).toString();

    while (!xml.atEnd())
    {
        xml.readNext();

        if (xml.isStartElement())
        {
            if (xml.name() == u"title")
            {
                m_result.title = xml.readElementText();
            }
            else if (xml.name() == u"updated")
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
            else if (xml.name() == u"entry")
            {
                parseAtomArticle(xml);
            }
        }
    }
}

void RSS::Private::Parser::addArticle(QVariantHash article)
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
