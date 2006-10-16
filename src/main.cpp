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
#include <QtXml/QDomDocument>
#include <QLocale>
#include <QTranslator>
#include <QFile>
#include <QSplashScreen>
#include <QTcpSocket>

#include <stdlib.h>

#include "GUI.h"
#include "misc.h"

// Main
int main(int argc, char *argv[]){
  QDomDocument doc("options");
  QDomElement root;
  QDomElement tag;
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
  // Buggy with Qt 4.1.2 : should try with another version
  //Check if there is another instance running
//   QTcpSocket *tcpSocket= new QTcpSocket();
//   tcpSocket->connectToHost(QHostAddress::LocalHost, 1666);
//   if (tcpSocket->waitForConnected(1000)){
//     std::cout << "Another qBittorrent instance is already running...\n";
//     // Send parameters
//     if(argc > 1){
//       QByteArray block;
//       QDataStream out(&block, QIODevice::WriteOnly);
//       out.setVersion(QDataStream::Qt_4_0);
//       out << (quint16)0;
//       std::cout << "deb size: " << block.data() << '\n';
//       for(int i=1;i<argc;++i){
//         out << QString(argv[i]);
//         std::cout << QString(argv[i]).toStdString() << '\n';
//       }
//       std::cout << "writting: " << block.data() << '\n';
//       out.device()->seek(0);
//       std::cout << "size: " << block.size() << '\n';
//       out << (quint16)(block.size() - sizeof(quint16));
//       tcpSocket->write(block);
//       std::cout << "written: " << block.data() << '\n';
//       tcpSocket->waitForConnected(5000);
//       tcpSocket->disconnectFromHost();
//       std::cout << "disconnected\n";
//       sleep(5);
//     }
//     tcpSocket->close();
//     delete tcpSocket;
//     return 0;
//   }
  // Check if another instance is running
  QTcpSocket tcpSocket;
  tcpSocket.connectToHost(QHostAddress::LocalHost, 1666);
  if (tcpSocket.waitForConnected(1000)){
    qDebug("Another qBittorrent instance is already running...");
    //  Write params to a temporary file
    QFile paramsFile(QDir::tempPath()+QDir::separator()+"qBT-params.txt");
    int count = 0;
    while(paramsFile.exists() && count <10){
      // If file exists, then the other instance is already reading params
      // We wait...
      SleeperThread::msleep(1000);
      ++count;
    }
    paramsFile.remove();
    // Write params to the file
    if (!paramsFile.open(QIODevice::WriteOnly | QIODevice::Text)){
      std:: cerr << "could not pass parameters\n";
      return 1;
    }
    qDebug("Passing parameters");
    for(int i=1;i<argc;++i){
      paramsFile.write(QByteArray(argv[i]).append("\n"));
    }
    paramsFile.close();
    tcpSocket.disconnectFromHost();
    tcpSocket.close();
    qDebug("exiting");
    return 0;
  }
  QApplication app(argc, argv);
  QSplashScreen *splash = new QSplashScreen(QPixmap(":/Icons/splash.jpg"));
  splash->show();
  bool isLocalized = false;
  // Open options file to read locale
  QString optionsPath = misc::qBittorrentPath()+"options.xml";
  FILE *f = fopen(optionsPath.toStdString().c_str(), "r");
  if(f){
    if (file.open(f, QIODevice::ReadOnly | QIODevice::Text)){
      if (doc.setContent(&file)) {
        root = doc.firstChildElement("options");
        tag = root.firstChildElement("locale");
        if(!tag.isNull()){
          locale = tag.text();
          isLocalized = true;
        }
      }
    }
    file.close();
    fclose(f);
  }

  QTranslator translator;
  if(!isLocalized){
    locale = QLocale::system().name();
  }
  if(translator.load(QString(":/lang/qbittorrent_") + locale)){
    qDebug("%s locale recognized, using translation.", locale.toStdString().c_str());
  }else{
    qDebug("%s locale unrecognized, using default (en_GB).", locale.toStdString().c_str());
  }
  app.installTranslator(&translator);
  // Read torrents given on command line
  QStringList torrentCmdLine = app.arguments();
  // Remove first argument (program name)
  torrentCmdLine.removeFirst();
  GUI window(0, torrentCmdLine);
  // Set locale
  if(!isLocalized){
    window.setLocale(locale);
  }
  // Show main window
  window.show();
  splash->finish(&window);
  delete splash;
  return app.exec();
}


