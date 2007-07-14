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
#include <QTcpSocket>
#include <QSettings>
#include <QTcpSocket>
#include <QTcpServer>
#include <QPlastiqueStyle>
#include <QCleanlooksStyle>
#include <QMotifStyle>
#include <QCDEStyle>
#ifdef Q_WS_WIN
  #include <QWindowsXPStyle>
#endif
#ifdef Q_WS_MAC
  #include <QMacStyle>
#endif

#include <stdlib.h>

#include "GUI.h"
#include "misc.h"

void useStyle(QApplication *app, QString style){
  std::cout << "* Style: Using " << style.toStdString()  << " style\n";
  if(style == "Cleanlooks"){
    app->setStyle(new QCleanlooksStyle());
    return;
  }
  if(style == "Motif"){
    app->setStyle(new QMotifStyle());
    return;
  }
  if(style == "CDE"){
    app->setStyle(new QCDEStyle());
    return;
  }
#ifdef Q_WS_MAC
  if(style == "MacOS"){
    app->setStyle(new QMacStyle());
    return;
  }
#endif
#ifdef Q_WS_WIN
  if(style == "WinXP"){
    app->setStyle(new QWindowsXPStyle());
    return;
  }
#endif
  app->setStyle(new QPlastiqueStyle());
}

// Main
int main(int argc, char *argv[]){
  QFile file;
  QString locale;
  if(argc > 1){
    if(QString(argv[1])=="--version"){
      std::cout << "qBittorrent " << VERSION << '\n';
      return 0;
    }
    if(QString(argv[1])=="--help"){
      std::cout << "Usage: \n";
      std::cout << '\t' << argv[0] << " --version : displays program version\n";
      std::cout << '\t' << argv[0] << " --help : displays this help message\n";
      std::cout << '\t' << argv[0] << " [files or urls] : starts program and download given parameters (optional)\n";
      return 0;
    }
  }
  // Set environment variable
  if(putenv("QBITTORRENT="VERSION)){
    std::cerr << "Couldn't set environment variable...\n";
  }
  //Check if there is another instance running
  QTcpSocket tcpSocket;
  tcpSocket.connectToHost(QHostAddress::LocalHost, 1666);
  if (tcpSocket.waitForConnected(1000)){
    std::cout << "Another qBittorrent instance is already running...\n";
    // Send parameters
    if(argc > 1){
      QStringList params;
      for(int i=1;i<argc;++i){
        params << QString(argv[i]);
        std::cout << QString(argv[i]).toStdString() << '\n';
      }
      QByteArray block = params.join("\n").toUtf8();
      std::cout << "writting: " << block.data() << '\n';
      std::cout << "size: " << block.size() << '\n';
      uint val = tcpSocket.write(block);
      if(tcpSocket.waitForBytesWritten(5000)){
        std::cout << "written(" <<val<<"): " << block.data() << '\n';
      }else{
        std::cerr << "Writing to the socket timed out\n";
      }
      tcpSocket.disconnectFromHost();
      std::cout << "disconnected\n";
    }
    tcpSocket.close();
    return 0;
  }
  QApplication app(argc, argv);
  QSettings settings("qBittorrent", "qBittorrent");
  QString style;
#ifdef Q_WS_WIN
  style = settings.value("Options/Style", "WinXP").toString();
#endif
#ifdef Q_WS_MAC
  style = settings.value("Options/Style", "MacOS").toString();
#endif
#ifndef Q_WS_WIN
  #ifndef Q_WS_MAC
  style = settings.value("Options/Style", "Plastique").toString();
  #endif
#endif
  useStyle(&app, style);
  QSplashScreen *splash = new QSplashScreen(QPixmap(":/Icons/splash.png"));
  splash->show();
  // Open options file to read locale
  locale = settings.value("Options/Language/Locale", QString()).toString();
  QTranslator translator;
  if(locale.isEmpty()){
    locale = QLocale::system().name();
    settings.setValue("Options/Language/Locale", locale);
  }
  if(translator.load(QString(":/lang/qbittorrent_") + locale)){
    qDebug("%s locale recognized, using translation.", (const char*)locale.toUtf8());
  }else{
    qDebug("%s locale unrecognized, using default (en_GB).", (const char*)locale.toUtf8());
  }
  app.installTranslator(&translator);
  app.setApplicationName("qBittorrent");
  app.setQuitOnLastWindowClosed(false);
  // Read torrents given on command line
  QStringList torrentCmdLine = app.arguments();
  // Remove first argument (program name)
  torrentCmdLine.removeFirst();
  GUI window(0, torrentCmdLine);
  splash->finish(&window);
  delete splash;
  return app.exec();
}


