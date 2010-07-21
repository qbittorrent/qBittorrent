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

#ifndef QINISETTINGS_H
#define QINISETTINGS_H

#include <QSettings>

class QIniSettings : public QSettings {
  Q_OBJECT

public:
  QIniSettings(const QString &organization, const QString &application = QString(), QObject *parent = 0 ):
#ifdef Q_WS_WIN
      QSettings(QSettings::IniFormat, QSettings::UserScope, organization, application, parent)
#else
      QSettings(organization, application, parent)
#endif
  {

  }

  QIniSettings(const QString &fileName, Format format, QObject *parent = 0 ) : QSettings(fileName, format, parent) {

  }

  QIniSettings& operator =(const QIniSettings &other) {
    Q_UNUSED(other);
    Q_ASSERT(0);
    return *this;
  }

#ifdef Q_WS_WIN
  QVariant value(const QString & key, const QVariant &defaultValue = QVariant()) {
    QString key_tmp(key);
    QVariant ret = QSettings::value(key_tmp);
    if(ret.isNull()) {
      if(format() == QSettings::NativeFormat) {

        // Fallback on Windows, use \ in key instead of /
        if(key_tmp.contains("/")) {
          ret = QSettings::value(key_tmp.replace("/", "\\"));
        } else {
          if(key_tmp.contains("\\")) {
            ret = QSettings::value(key_tmp.replace("\\", "/"));
          }
        }

      } else {
        // Keep compatibility with qBittorrent < 2.3.0
        // Import the setting from the registry
        QSettings old_settings(organizationName(), applicationName());
        ret = old_settings.value(key_tmp);
        if(!ret.isNull()) {
          setValue(key_tmp, ret);
          old_settings.remove(key_tmp);
        }
      }
    }
    if(ret.isNull())
      return defaultValue;
    return ret;
  }

  void setValue(const QString &key, const QVariant &val) {
    QString key_tmp(key);
    if(format() == QSettings::NativeFormat)
      key_tmp = key_tmp.replace("/", "\\");
    QSettings::setValue(key_tmp, val);
  }
#endif
};

#endif // QINISETTINGS_H
