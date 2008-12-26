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
 * Contact : chris@qbittorrent.org
 */

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QFile>
#include <QSplashScreen>
#include <QSettings>
#ifdef QT_4_4
  #include <QLocalSocket>
  #include <unistd.h>
  #include <sys/types.h>
#else
  #include <QTcpSocket>
  #include <QHostAddress>
#endif
#include <QPlastiqueStyle>
#include "qgnomelook.h"
#include <QMotifStyle>
#include <QCDEStyle>
#ifdef Q_WS_WIN
  #include <QWindowsXPStyle>
#endif
#ifdef Q_WS_MAC
  #include <QMacStyle>
#endif
#ifndef Q_WS_WIN
  #include <signal.h>
#endif

#include <stdlib.h>
#include "GUI.h"
#include "misc.h"
#include "ico.h"

QApplication *app;

#ifndef Q_WS_WIN
  void sigtermHandler(int) {
    qDebug("Catching SIGTERM, exiting cleanly");
    app->exit();
  }
#endif

void useStyle(QApplication *app, int style){
  switch(style) {
    case 1:
      app->setStyle(new QPlastiqueStyle());
      break;
    case 2:
      app->setStyle(new QGnomeLookStyle());
      break;
    case 3:
      app->setStyle(new QMotifStyle());
      break;
    case 4:
      app->setStyle(new QCDEStyle());
      break;
#ifdef Q_WS_MAC
    case 5:
    app->setStyle(new QMacStyle());
    break;
#endif
#ifdef Q_WS_WIN
    case 6:
    app->setStyle(new QWindowsXPStyle());
    break;
#endif
  }
}

// Main
int main(int argc, char *argv[]){
  QFile file;
  QString locale;
  if(argc > 1){
    if(QString::fromUtf8(argv[1]) == QString::fromUtf8("--version")){
      std::cout << "qBittorrent " << VERSION << '\n';
      return 0;
    }
    if(QString::fromUtf8(argv[1]) == QString::fromUtf8("--help")){
      std::cout << "Usage: \n";
      std::cout << '\t' << argv[0] << " --version : displays program version\n";
      std::cout << '\t' << argv[0] << " --help : displays this help message\n";
      std::cout << '\t' << argv[0] << " [files or urls] : starts program and download given parameters (optional)\n";
      return 0;
    }
  }
  // Set environment variable
  if(putenv((char*)"QBITTORRENT="VERSION)) {
    std::cerr << "Couldn't set environment variable...\n";
  }
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  //Check if there is another instance running
#ifdef QT_4_4
  QLocalSocket localSocket;
  QString uid = QString::number(getuid());
#else
  QTcpSocket localSocket;
#endif
#ifdef QT_4_4
  localSocket.connectToServer("qBittorrent-"+uid, QIODevice::WriteOnly);
#else
  int serverPort = settings.value(QString::fromUtf8("uniqueInstancePort"), -1).toInt();
  if(serverPort != -1) {
  localSocket.connectToHost(QHostAddress::LocalHost, serverPort, QIODevice::WriteOnly);
#endif
  if (localSocket.waitForConnected(1000)){
    std::cout << "Another qBittorrent instance is already running...\n";
    // Send parameters
    if(argc > 1){
      QStringList params;
      for(int i=1;i<argc;++i){
        params << QString::fromUtf8(argv[i]);
        std::cout << argv[i] << '\n';
      }
      QByteArray block = params.join("\n").toUtf8();
      std::cout << "writting: " << block.data() << '\n';
      std::cout << "size: " << block.size() << '\n';
      uint val = localSocket.write(block);
      if(localSocket.waitForBytesWritten(5000)){
        std::cout << "written(" <<val<<"): " << block.data() << '\n';
      }else{
        std::cerr << "Writing to the socket timed out\n";
      }
#ifdef QT_4_4
      localSocket.disconnectFromServer();
#else
      localSocket.disconnectFromHost();
#endif
      std::cout << "disconnected\n";
    }
    localSocket.close();
    return 0;
  }
#ifndef QT_4_4
  }
#endif
  app = new QApplication(argc, argv);
  useStyle(app, settings.value("Preferences/General/Style", 0).toInt());
  app->setStyleSheet("QStatusBar::item { border-width: 0; }");
  QSplashScreen *splash = new QSplashScreen(QPixmap(QString::fromUtf8(":/Icons/splash.png")));
  splash->show();
  // Open options file to read locale
  locale = settings.value(QString::fromUtf8("Preferences/General/Locale"), QString()).toString();
  QTranslator translator;
  if(locale.isEmpty()){
    locale = QLocale::system().name();
    settings.setValue(QString::fromUtf8("Preferences/General/Locale"), locale);
  }
  if(translator.load(QString::fromUtf8(":/lang/qbittorrent_") + locale)){
    qDebug("%s locale recognized, using translation.", (const char*)locale.toUtf8());
  }else{
    qDebug("%s locale unrecognized, using default (en_GB).", (const char*)locale.toUtf8());
  }
  app->installTranslator(&translator);
  app->setApplicationName(QString::fromUtf8("qBittorrent"));
  app->setQuitOnLastWindowClosed(false);
  // Read torrents given on command line
  QStringList torrentCmdLine = app->arguments();
  // Remove first argument (program name)
  torrentCmdLine.removeFirst();
  GUI window(0, torrentCmdLine);
  splash->finish(&window);
  delete splash;
#ifndef Q_WS_WIN
  signal(SIGTERM, sigtermHandler);
#endif
  return app->exec();
}


