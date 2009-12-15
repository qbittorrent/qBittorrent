/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez, Frédéric Lassabe
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

#ifndef HEADLESSLOADER_H
#define HEADLESSLOADER_H

#include <QObject>
#include <QCoreApplication>
#include "preferences.h"
#include "bittorrent.h"

class HeadlessLoader: QObject {
  Q_OBJECT

  private:
    Bittorrent *BTSession;

  public:
    HeadlessLoader(QStringList torrentCmdLine) {
      // Enable Web UI
      Preferences::setWebUiEnabled(true);
      // TODO: Listen on socket for parameters
      // Instanciate Bittorrent Object
      BTSession = new Bittorrent();
      connect(BTSession, SIGNAL(newConsoleMessage(QString)), this, SLOT(displayConsoleMessage(QString)));
      // Resume unfinished torrents
      BTSession->startUpTorrents();
      // TODO: Process command line parameter
    }

    ~HeadlessLoader() {
      delete BTSession;
    }

  public slots:
    // Call this function to exit qBittorrent headless loader
    // and return to prompt (object will be deleted by main)
    void exit() {
     qApp->quit();
    }

    void displayConsoleMessage(QString msg) {
      std::cout << msg.toLocal8Bit().data() << std::endl;
    }
};

#endif
