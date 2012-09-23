/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#include "misc.h"

#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QByteArray>
#include <QDebug>
#include <QProcess>
#include <QSettings>

#ifdef DISABLE_GUI
#include <QCoreApplication>
#else
#include <QApplication>
#include <QDesktopWidget>
#endif

#ifdef Q_WS_WIN
#include <windows.h>
#include <PowrProf.h>
const int UNLEN = 256;
#else
#include <unistd.h>
#include <sys/types.h>
#endif

#ifdef Q_WS_MAC
#include <CoreServices/CoreServices.h>
#include <Carbon/Carbon.h>
#endif

#ifndef DISABLE_GUI
#if defined(Q_WS_X11) && defined(QT_DBUS_LIB)
#include <QDBusInterface>
#include <QDBusMessage>
#endif
#endif // DISABLE_GUI

using namespace libtorrent;

static struct { const char *source; const char *comment; } units[] = {
  QT_TRANSLATE_NOOP3("misc", "B", "bytes"),
  QT_TRANSLATE_NOOP3("misc", "KiB", "kibibytes (1024 bytes)"),
  QT_TRANSLATE_NOOP3("misc", "MiB", "mebibytes (1024 kibibytes)"),
  QT_TRANSLATE_NOOP3("misc", "GiB", "gibibytes (1024 mibibytes)"),
  QT_TRANSLATE_NOOP3("misc", "TiB", "tebibytes (1024 gibibytes)")
};

#ifndef DISABLE_GUI
void misc::shutdownComputer(bool sleep) {
#if defined(Q_WS_X11) && defined(QT_DBUS_LIB)
  // Use dbus to power off / suspend the system
  if (sleep) {
    // Recent systems use UPower
    QDBusInterface upowerIface("org.freedesktop.UPower", "/org/freedesktop/UPower",
                               "org.freedesktop.UPower", QDBusConnection::systemBus());
    if (upowerIface.isValid()) {
      upowerIface.call("Suspend");
      return;
    }
    // HAL (older systems)
    QDBusInterface halIface("org.freedesktop.Hal", "/org/freedesktop/Hal/devices/computer",
                            "org.freedesktop.Hal.Device.SystemPowerManagement",
                            QDBusConnection::systemBus());
    halIface.call("Suspend", 5);
  } else {
    // Recent systems use ConsoleKit
    QDBusInterface consolekitIface("org.freedesktop.ConsoleKit", "/org/freedesktop/ConsoleKit/Manager",
                                   "org.freedesktop.ConsoleKit.Manager", QDBusConnection::systemBus());
    if (consolekitIface.isValid()) {
      consolekitIface.call("Stop");
      return;
    }
    // HAL (older systems)
    QDBusInterface halIface("org.freedesktop.Hal", "/org/freedesktop/Hal/devices/computer",
                            "org.freedesktop.Hal.Device.SystemPowerManagement",
                            QDBusConnection::systemBus());
    halIface.call("Shutdown");
  }
#endif
#ifdef Q_WS_MAC
  AEEventID EventToSend;
  if (sleep)
    EventToSend = kAESleep;
  else
    EventToSend = kAEShutDown;
  AEAddressDesc targetDesc;
  static const ProcessSerialNumber kPSNOfSystemProcess = { 0, kSystemProcess };
  AppleEvent eventReply = {typeNull, NULL};
  AppleEvent appleEventToSend = {typeNull, NULL};

  OSStatus error = noErr;

  error = AECreateDesc(typeProcessSerialNumber, &kPSNOfSystemProcess,
                       sizeof(kPSNOfSystemProcess), &targetDesc);

  if (error != noErr)
  {
    return;
  }

  error = AECreateAppleEvent(kCoreEventClass, EventToSend, &targetDesc,
                             kAutoGenerateReturnID, kAnyTransactionID, &appleEventToSend);

  AEDisposeDesc(&targetDesc);
  if (error != noErr)
  {
    return;
  }

  error = AESend(&appleEventToSend, &eventReply, kAENoReply,
                 kAENormalPriority, kAEDefaultTimeout, NULL, NULL);

  AEDisposeDesc(&appleEventToSend);
  if (error != noErr)
  {
    return;
  }

  AEDisposeDesc(&eventReply);
#endif
#ifdef Q_WS_WIN
  HANDLE hToken;              // handle to process token
  TOKEN_PRIVILEGES tkp;       // pointer to token structure
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    return;
  // Get the LUID for shutdown privilege.
  LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME,
                       &tkp.Privileges[0].Luid);

  tkp.PrivilegeCount = 1;  // one privilege to set
  tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

  // Get shutdown privilege for this process.

  AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
                        (PTOKEN_PRIVILEGES) NULL, 0);

  // Cannot test the return value of AdjustTokenPrivileges.

  if (GetLastError() != ERROR_SUCCESS)
    return;

  if (sleep)
    SetSuspendState(false, false, false);
  else
    InitiateSystemShutdownA(0, tr("qBittorrent will shutdown the computer now because all downloads are complete.").toLocal8Bit().data(), 10, true, false);

  // Disable shutdown privilege.
  tkp.Privileges[0].Attributes = 0;
  AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
                        (PTOKEN_PRIVILEGES) NULL, 0);
#endif
}
#endif // DISABLE_GUI

#ifndef DISABLE_GUI
// Get screen center
QPoint misc::screenCenter(QWidget *win) {
  int scrn = 0;
  const QWidget *w = win->window();

  if (w)
    scrn = QApplication::desktop()->screenNumber(w);
  else if (QApplication::desktop()->isVirtualDesktop())
    scrn = QApplication::desktop()->screenNumber(QCursor::pos());
  else
    scrn = QApplication::desktop()->screenNumber(win);

  QRect desk(QApplication::desktop()->availableGeometry(scrn));
  return QPoint((desk.width() - win->frameGeometry().width()) / 2, (desk.height() - win->frameGeometry().height()) / 2);
}
#endif

/**
 * Detects the version of python by calling
 * "python --version" and parsing the output.
 */
int misc::pythonVersion() {
  static int version = -1;
  if (version < 0) {
    QProcess python_proc;
    python_proc.start("python", QStringList() << "--version", QIODevice::ReadOnly);
    if (!python_proc.waitForFinished()) return -1;
    if (python_proc.exitCode() < 0) return -1;
    QByteArray output = python_proc.readAllStandardOutput();
    if (output.isEmpty())
      output = python_proc.readAllStandardError();
    const QByteArray version_str = output.split(' ').last();
    qDebug() << "Python version is:" << version_str.trimmed();
    if (version_str.startsWith("3."))
      version = 3;
    else
      version = 2;
  }
  return version;
}

// return best userfriendly storage unit (B, KiB, MiB, GiB, TiB)
// use Binary prefix standards from IEC 60027-2
// see http://en.wikipedia.org/wiki/Kilobyte
// value must be given in bytes
QString misc::friendlyUnit(qreal val, bool is_speed) {
  if (val < 0)
    return tr("Unknown", "Unknown (size)");
  int i = 0;
  while(val >= 1024. && i++<6)
    val /= 1024.;
  QString ret;
  if (i == 0)
    ret = QString::number((long)val) + " " + tr(units[0].source, units[0].comment);
  else
    ret = QString::number(val, 'f', 1) + " " + tr(units[i].source, units[i].comment);
  if (is_speed)
    ret += tr("/s", "per second");
  return ret;
}

bool misc::isPreviewable(QString extension) {
  if (extension.isEmpty()) return false;
  extension = extension.toUpper();
  if (extension == "AVI") return true;
  if (extension == "MP3") return true;
  if (extension == "OGG") return true;
  if (extension == "OGV") return true;
  if (extension == "OGM") return true;
  if (extension == "WMV") return true;
  if (extension == "WMA") return true;
  if (extension == "MPEG") return true;
  if (extension == "MPG") return true;
  if (extension == "ASF") return true;
  if (extension == "QT") return true;
  if (extension == "RM") return true;
  if (extension == "RMVB") return true;
  if (extension == "RMV") return true;
  if (extension == "SWF") return true;
  if (extension == "FLV") return true;
  if (extension == "WAV") return true;
  if (extension == "MOV") return true;
  if (extension == "VOB") return true;
  if (extension == "MID") return true;
  if (extension == "AC3") return true;
  if (extension == "MP4") return true;
  if (extension == "MP2") return true;
  if (extension == "AVI") return true;
  if (extension == "FLAC") return true;
  if (extension == "AU") return true;
  if (extension == "MPE") return true;
  if (extension == "MOV") return true;
  if (extension == "MKV") return true;
  if (extension == "AIF") return true;
  if (extension == "AIFF") return true;
  if (extension == "AIFC") return true;
  if (extension == "RA") return true;
  if (extension == "RAM") return true;
  if (extension == "M4P") return true;
  if (extension == "M4A") return true;
  if (extension == "3GP") return true;
  if (extension == "AAC") return true;
  if (extension == "SWA") return true;
  if (extension == "MPC") return true;
  if (extension == "MPP") return true;
  if (extension == "M3U") return true;
  return false;
}

QString misc::bcLinkToMagnet(QString bc_link) {
  QByteArray raw_bc = bc_link.toUtf8();
  raw_bc = raw_bc.mid(8); // skip bc://bt/
  raw_bc = QByteArray::fromBase64(raw_bc); // Decode base64
  // Format is now AA/url_encoded_filename/size_bytes/info_hash/ZZ
  QStringList parts = QString(raw_bc).split("/");
  if (parts.size() != 5) return QString::null;
  QString filename = parts.at(1);
  QString hash = parts.at(3);
  QString magnet = "magnet:?xt=urn:btih:" + hash;
  magnet += "&dn=" + filename;
  return magnet;
}

QString misc::magnetUriToName(const QString& magnet_uri) {
  QString name = "";
  QRegExp regHex("dn=([^&]+)");
  const int pos = regHex.indexIn(magnet_uri);
  if (pos > -1) {
    const QString found = regHex.cap(1);
    // URL decode
    name = QUrl::fromPercentEncoding(found.toLocal8Bit()).replace("+", " ");
  }
  return name;
}

QList<QUrl> misc::magnetUriToTrackers(const QString& magnet_uri)
{
  QList<QUrl> trackers;
  QRegExp rx("tr=([^&]+)");
  int pos = 0;

  while ((pos = rx.indexIn(magnet_uri, pos)) != -1) {
    const QUrl tracker = QUrl::fromEncoded(rx.cap(1).toUtf8());
    qDebug() << Q_FUNC_INFO << "Found tracker: " << tracker.toString();
    trackers << tracker;
    pos += rx.matchedLength();
  }

  return trackers;
}

QString misc::magnetUriToHash(const QString& magnet_uri) {
  QString hash = "";
  QRegExp regHex("urn:btih:([0-9A-Za-z]+)");
  // Hex
  int pos = regHex.indexIn(magnet_uri);
  if (pos > -1) {
    const QString found = regHex.cap(1);
    qDebug() << Q_FUNC_INFO << "regex found: " << found;
    if (found.length() == 40) {
      const sha1_hash sha1(QByteArray::fromHex(found.toAscii()).constData());
      qDebug("magnetUriToHash (Hex): hash: %s", qPrintable(misc::toQString(sha1)));
      return misc::toQString(sha1);
    }
  }
  // Base 32
  QRegExp regBase32("urn:btih:([A-Za-z2-7=]+)");
  pos = regBase32.indexIn(magnet_uri);
  if (pos > -1) {
    const QString found = regBase32.cap(1);
    if (found.length() > 20 && (found.length()*5)%40 == 0) {
      const sha1_hash sha1(base32decode(regBase32.cap(1).toStdString()));
      hash = misc::toQString(sha1);
    }
  }
  qDebug("magnetUriToHash (base32): hash: %s", qPrintable(hash));
  return hash;
}

// Take a number of seconds and return an user-friendly
// time duration like "1d 2h 10m".
QString misc::userFriendlyDuration(qlonglong seconds) {
  if (seconds < 0 || seconds >= MAX_ETA) {
    return QString::fromUtf8("∞");
  }
  if (seconds == 0) {
    return "0";
  }
  if (seconds < 60) {
    return tr("< 1m", "< 1 minute");
  }
  int minutes = seconds / 60;
  if (minutes < 60) {
    return tr("%1m","e.g: 10minutes").arg(QString::number(minutes));
  }
  int hours = minutes / 60;
  minutes = minutes - hours*60;
  if (hours < 24) {
    return tr("%1h %2m", "e.g: 3hours 5minutes").arg(QString::number(hours)).arg(QString::number(minutes));
  }
  int days = hours / 24;
  hours = hours - days * 24;
  if (days < 100) {
    return tr("%1d %2h", "e.g: 2days 10hours").arg(QString::number(days)).arg(QString::number(hours));
  }
  return QString::fromUtf8("∞");
}

QString misc::getUserIDString() {
  QString uid = "0";
#ifdef Q_WS_WIN
  char buffer[UNLEN+1] = {0};
  DWORD buffer_len = UNLEN + 1;
  if (!GetUserNameA(buffer, &buffer_len))
    uid = QString(buffer);
#else
  uid = QString::number(getuid());
#endif
  return uid;
}

QStringList misc::toStringList(const QList<bool> &l) {
  QStringList ret;
  foreach (const bool &b, l) {
    ret << (b ? "1" : "0");
  }
  return ret;
}

QList<int> misc::intListfromStringList(const QStringList &l) {
  QList<int> ret;
  foreach (const QString &s, l) {
    ret << s.toInt();
  }
  return ret;
}

QList<bool> misc::boolListfromStringList(const QStringList &l) {
  QList<bool> ret;
  foreach (const QString &s, l) {
    ret << (s=="1");
  }
  return ret;
}

bool misc::isUrl(const QString &s)
{
  const QString scheme = QUrl(s).scheme();
  QRegExp is_url("http[s]?|ftp", Qt::CaseInsensitive);
  return is_url.exactMatch(scheme);
}

QString misc::parseHtmlLinks(const QString &raw_text)
{
  QString result = raw_text;
  QRegExp reURL("(\\s|^)"                                     //start with whitespace or beginning of line
                "("
                "("                                      //case 1 -- URL with scheme
                "(http(s?))\\://"                    //start with scheme
                "([a-zA-Z0-9_-]+\\.)+"               //  domainpart.  at least one of these must exist
                "([a-zA-Z0-9\\?%=&/_\\.:#;-]+)"      //  everything to 1st non-URI char, must be at least one char after the previous dot (cannot use ".*" because it can be too greedy)
                ")"
                "|"
                "("                                     //case 2a -- no scheme, contains common TLD  example.com
                "([a-zA-Z0-9_-]+\\.)+"              //  domainpart.  at least one of these must exist
                "(?="                               //  must be followed by TLD
                "AERO|aero|"                  //N.B. assertions are non-capturing
                "ARPA|arpa|"
                "ASIA|asia|"
                "BIZ|biz|"
                "CAT|cat|"
                "COM|com|"
                "COOP|coop|"
                "EDU|edu|"
                "GOV|gov|"
                "INFO|info|"
                "INT|int|"
                "JOBS|jobs|"
                "MIL|mil|"
                "MOBI|mobi|"
                "MUSEUM|museum|"
                "NAME|name|"
                "NET|net|"
                "ORG|org|"
                "PRO|pro|"
                "RO|ro|"
                "RU|ru|"
                "TEL|tel|"
                "TRAVEL|travel"
                ")"
                "([a-zA-Z0-9\\?%=&/_\\.:#;-]+)"     //  everything to 1st non-URI char, must be at least one char after the previous dot (cannot use ".*" because it can be too greedy)
                ")"
                "|"
                "("                                     // case 2b no scheme, no TLD, must have at least 2 aphanum strings plus uncommon TLD string  --> del.icio.us
                "([a-zA-Z0-9_-]+\\.) {2,}"           //2 or more domainpart.   --> del.icio.
                "[a-zA-Z]{2,}"                      //one ab  (2 char or longer) --> us
                "([a-zA-Z0-9\\?%=&/_\\.:#;-]*)"     // everything to 1st non-URI char, maybe nothing  in case of del.icio.us/path
                ")"
                ")"
                );


  // Capture links
  result.replace(reURL, "\\1<a href=\"\\2\">\\2</a>");

  // Capture links without scheme
  QRegExp reNoScheme("<a\\s+href=\"(?!http(s?))([a-zA-Z0-9\\?%=&/_\\.-:#]+)\\s*\">");
  result.replace(reNoScheme, "<a href=\"http://\\1\">");

  return result;
}

#if LIBTORRENT_VERSION_MINOR < 16
QString misc::toQString(const boost::posix_time::ptime& boostDate) {
  if (boostDate.is_not_a_date_time()) return "";
  struct std::tm tm;
  try {
    tm = boost::posix_time::to_tm(boostDate);
  } catch(std::exception e) {
      return "";
  }
  const time_t t = mktime(&tm);
  const QDateTime dt = QDateTime::fromTime_t(t);
  return dt.toString(Qt::DefaultLocaleLongDate);
}
#else
QString misc::toQString(time_t t)
{
  return QDateTime::fromTime_t(t).toString(Qt::DefaultLocaleLongDate);
}
#endif
