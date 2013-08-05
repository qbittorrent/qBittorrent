/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2011  Christophe Dumez
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

#include "iconprovider.h"
#include "preferences.h"

IconProvider* IconProvider::m_instance = 0;

IconProvider::IconProvider()
{
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
  m_useSystemTheme = Preferences().useSystemIconTheme();
#endif
}

IconProvider * IconProvider::instance()
{
  if (!m_instance)
    m_instance = new IconProvider;
  return m_instance;
}

void IconProvider::drop()
{
  if (m_instance) {
    delete m_instance;
    m_instance = 0;
  }
}

QIcon IconProvider::getIcon(const QString &iconId)
{
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
  if (m_useSystemTheme) {
    QIcon icon = QIcon::fromTheme(iconId, QIcon(":/Icons/oxygen/"+iconId+".png"));
    icon = generateDifferentSizes(icon);
    return icon;
  }
#endif
  return QIcon(":/Icons/oxygen/"+iconId+".png");
}

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
void IconProvider::useSystemIconTheme(bool enable)
{
  m_useSystemTheme = enable;
}

// Makes sure the icon is at least available in 16px and 24px size
// It scales the icon from the theme if necessary
// Otherwise, the UI looks broken if the icon is not available
// in the correct size.
QIcon IconProvider::generateDifferentSizes(const QIcon& icon)
{
  QIcon new_icon;
  QList<QSize> required_sizes;
  required_sizes << QSize(16, 16) << QSize(24, 24);
  QList<QIcon::Mode> modes;
  modes << QIcon::Normal << QIcon::Active << QIcon::Selected << QIcon::Disabled;
  foreach (const QSize& size, required_sizes) {
    foreach (QIcon::Mode mode, modes) {
      QPixmap pixoff = icon.pixmap(size, mode, QIcon::Off);
      if (pixoff.height() > size.height())
        pixoff = pixoff.scaled(size, Qt::KeepAspectRatio,  Qt::SmoothTransformation);
      new_icon.addPixmap(pixoff, mode, QIcon::Off);
      QPixmap pixon = icon.pixmap(size, mode, QIcon::On);
      if (pixon.height() > size.height())
        pixon = pixoff.scaled(size, Qt::KeepAspectRatio,  Qt::SmoothTransformation);
      new_icon.addPixmap(pixon, mode, QIcon::On);
    }
  }
  return new_icon;
}
#endif

QString IconProvider::getIconPath(const QString& iconId)
{
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
  if (m_useSystemTheme) {
    QString path = QDir::temp().absoluteFilePath(iconId+".png");
    if (!QFile::exists(path)) {
      const QIcon icon = QIcon::fromTheme(iconId);
      if (icon.isNull()) return ":/Icons/oxygen/"+iconId+".png";
      QPixmap px = icon.pixmap(32);
      px.save(path);
    }
    return path;
  }
#endif
  return ":/Icons/oxygen/"+iconId+".png";
}
