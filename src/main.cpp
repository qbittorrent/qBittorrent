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

#include <QLocale>
#include <QTranslator>
#include <QFile>

#ifndef DISABLE_GUI
#include <QApplication>
#include <QMessageBox>
#include <QStyleFactory>
#include <QStyle>
#include <QSplashScreen>
#include <QPushButton>
#include "GUI.h"
#include "ico.h"
#else
#include <QCoreApplication>
#include <iostream>
#include <stdio.h>
#include "headlessloader.h"
#endif

#include <QSettings>
#include <QLocalSocket>
#include <sys/types.h>

#if defined(Q_WS_X11) || defined(Q_WS_MAC)
#include <signal.h>
#include <execinfo.h>
#include "stacktrace.h"
#endif

#ifdef Q_WS_WIN
#include <windows.h>
const int UNLEN = 256;
#endif

#include <stdlib.h>
#include "misc.h"
#include "preferences.h"

#ifdef DISABLE_GUI
QCoreApplication *app;
#else
QApplication *app;
#endif

class UsageDisplay: public QObject {
  Q_OBJECT

public:
  static void displayUsage(char* prg_name) {
    std::cout << qPrintable(tr("Usage:")) << std::endl;
    std::cout << '\t' << prg_name << " --version: " << qPrintable(tr("displays program version")) << std::endl;
#ifndef DISABLE_GUI
    std::cout << '\t' << prg_name << " --no-splash: " << qPrintable(tr("disable splash screen")) << std::endl;
#endif
    std::cout << '\t' << prg_name << " --help: " << qPrintable(tr("displays this help message")) << std::endl;
    std::cout << '\t' << prg_name << " --webui-port=x: " << qPrintable(tr("changes the webui port (current: %1)").arg(QString::number(Preferences::getWebUiPort()))) << std::endl;
    std::cout << '\t' << prg_name << " " << qPrintable(tr("[files or urls]: downloads the torrents passed by the user (optional)")) << std::endl;
  }
};

class LegalNotice: public QObject {
  Q_OBJECT

public:
  static bool userAgreesWithNotice() {
    QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    if(settings.value(QString::fromUtf8("LegalNotice/Accepted"), false).toBool()) // Already accepted once
      return true;
#ifdef DISABLE_GUI
    std::cout << std::endl << "*** " << qPrintable(tr("Legal Notice")) << " ***" << std::endl;
    std::cout << qPrintable(tr("qBittorrent is a file sharing program. When you run a torrent, its data will be made available to others by means of upload. Any content you share is your sole responsibility.\n\nNo further notices will be issued.")) << std::endl << std::endl;
    std::cout << qPrintable(tr("Press %1 key to accept and continue...").arg("'y'")) << std::endl;
    char ret = getchar(); // Read pressed key
    if(ret == 'y' || ret == 'Y') {
      // Save the answer
      settings.setValue(QString::fromUtf8("LegalNotice/Accepted"), true);
      return true;
    }
    return false;
#else
    QMessageBox msgBox;
    msgBox.setText(tr("qBittorrent is a file sharing program. When you run a torrent, its data will be made available to others by means of upload. Any content you share is your sole responsibility.\n\nNo further notices will be issued."));
    msgBox.setWindowTitle(tr("Legal notice"));
    msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    QAbstractButton *agree_button = msgBox.addButton(tr("I Agree"), QMessageBox::AcceptRole);
    msgBox.exec();
    if(msgBox.clickedButton() == agree_button) {
      // Save the answer
      settings.setValue(QString::fromUtf8("LegalNotice/Accepted"), true);
      return true;
    }
    return false;
#endif
  }
};

#include "main.moc"

#if defined(Q_WS_X11) || defined(Q_WS_MAC)
void sigintHandler(int) {
  signal(SIGINT, 0);
  qDebug("Catching SIGINT, exiting cleanly");
  app->exit();
}

void sigtermHandler(int) {
  signal(SIGTERM, 0);
  qDebug("Catching SIGTERM, exiting cleanly");
  app->exit();
}
void sigsegvHandler(int) {
  signal(SIGABRT, 0);
  signal(SIGSEGV, 0);
  std::cerr << "\n\n*************************************************************\n";
  std::cerr << "Catching SIGSEGV, please report a bug at http://bug.qbittorrent.org\nand provide the following backtrace:\n";
  print_stacktrace();
  std::raise(SIGSEGV);
}
void sigabrtHandler(int) {
  signal(SIGABRT, 0);
  signal(SIGSEGV, 0);
  std::cerr << "\n\n*************************************************************\n";
  std::cerr << "Catching SIGABRT, please report a bug at http://bug.qbittorrent.org\nand provide the following backtrace:\n";
  print_stacktrace();
  std::raise(SIGABRT);
}
#endif

#ifndef DISABLE_GUI
void useStyle(QApplication *app, QString style){
  if(!style.isEmpty()) {
    QApplication::setStyle(QStyleFactory::create(style));
  }
  Preferences::setStyle(app->style()->objectName());
}
#endif

// Main
int main(int argc, char *argv[]){
  QString locale;
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
#ifndef DISABLE_GUI
  bool no_splash = false;
#endif

  //Check if there is another instance running
  QLocalSocket localSocket;
  QString uid;
#ifdef Q_WS_WIN
  char buffer[UNLEN+1] = {0};
  DWORD buffer_len = UNLEN + 1;
  if (!GetUserName(buffer, &buffer_len))
    uid = QString(buffer)
#else
    uid = QString::number(getuid());
#endif
  localSocket.connectToServer("qBittorrent-"+uid, QIODevice::WriteOnly);
  if (localSocket.waitForConnected(1000)){
    std::cout << "Another qBittorrent instance is already running...\n";
    // Send parameters
    if(argc > 1){
      QStringList params;
      for(int i=1;i<argc;++i){
        params << QString::fromLocal8Bit(argv[i]);
        std::cout << argv[i] << '\n';
      }
      QByteArray block = params.join("\n").toLocal8Bit();
      std::cout << "writting: " << block.data() << '\n';
      std::cout << "size: " << block.size() << '\n';
      uint val = localSocket.write(block);
      if(localSocket.waitForBytesWritten(5000)){
        std::cout << "written(" <<val<<"): " << block.data() << '\n';
      }else{
        std::cerr << "Writing to the socket timed out\n";
      }
      localSocket.disconnectFromServer();
      std::cout << "disconnected\n";
    }
    localSocket.close();
    return 0;
  }

  // Create Application
#ifdef DISABLE_GUI
  app = new QCoreApplication(argc, argv);
#else
  app = new QApplication(argc, argv);
#endif

  // Load translation
  locale = settings.value(QString::fromUtf8("Preferences/General/Locale"), QString()).toString();
  QTranslator translator;
  if(locale.isEmpty()){
    locale = QLocale::system().name();
    settings.setValue(QString::fromUtf8("Preferences/General/Locale"), locale);
  }
  if(translator.load(QString::fromUtf8(":/lang/qbittorrent_") + locale)){
    qDebug("%s locale recognized, using translation.", qPrintable(locale));
  }else{
    qDebug("%s locale unrecognized, using default (en_GB).", qPrintable(locale));
  }
  app->installTranslator(&translator);
  app->setApplicationName(QString::fromUtf8("qBittorrent"));

  // Check for executable parameters
  if(argc > 1){
    if(QString::fromLocal8Bit(argv[1]) == QString::fromUtf8("--version")){
      std::cout << "qBittorrent " << VERSION << '\n';
      delete app;
      return 0;
    }
    if(QString::fromLocal8Bit(argv[1]) == QString::fromUtf8("--help")){
      UsageDisplay::displayUsage(argv[0]);
      delete app;
      return 0;
    }

    for(int i=1; i<argc; ++i) {
#ifndef DISABLE_GUI
      if(QString::fromLocal8Bit(argv[i]) == QString::fromUtf8("--no-splash")) {
        no_splash = true;
      } else {
#endif
        if(QString::fromLocal8Bit(argv[i]).startsWith("--webui-port=")) {
          QStringList parts = QString::fromLocal8Bit(argv[i]).split("=");
          if(parts.size() == 2) {
            bool ok = false;
            int new_port = parts.last().toInt(&ok);
            if(ok && new_port > 0 && new_port <= 65535) {
              Preferences::setWebUiPort(new_port);
            }
          }
        }
#ifndef DISABLE_GUI
      }
#endif
    }
  }

#ifndef DISABLE_GUI
  if(settings.value(QString::fromUtf8("Preferences/General/NoSplashScreen"), false).toBool()) {
    no_splash = true;
  }
#endif
  // Set environment variable
  if(putenv((char*)"QBITTORRENT="VERSION)) {
    std::cerr << "Couldn't set environment variable...\n";
  }

#ifndef DISABLE_GUI
  useStyle(app, settings.value("Preferences/General/Style", "").toString());
  app->setStyleSheet("QStatusBar::item { border-width: 0; }");
  QSplashScreen *splash = 0;
  if(!no_splash) {
    splash = new QSplashScreen(QPixmap(QString::fromUtf8(":/Icons/skin/splash.png")));
    splash->show();
  }
#endif

  if(!LegalNotice::userAgreesWithNotice()) {
#ifndef DISABLE_GUI
    delete splash;
#endif
    delete app;
    return 0;
  }
#ifndef DISABLE_GUI
  app->setQuitOnLastWindowClosed(false);
#endif
#if defined(Q_WS_X11) || defined(Q_WS_MAC)
  signal(SIGABRT, sigabrtHandler);
  signal(SIGTERM, sigtermHandler);
  signal(SIGINT, sigintHandler);
  signal(SIGSEGV, sigsegvHandler);
#endif
  // Read torrents given on command line
  QStringList torrentCmdLine = app->arguments();
  // Remove first argument (program name)
  torrentCmdLine.removeFirst();
#ifndef DISABLE_GUI
  GUI *window = new GUI(0, torrentCmdLine);
  if(!no_splash) {
    splash->finish(window);
    delete splash;
  }
#else
  // Load Headless class
  HeadlessLoader *loader = new HeadlessLoader(torrentCmdLine);
#endif
  int ret =  app->exec();

#if defined(Q_WS_X11) || defined(Q_WS_MAC)
  // Application has exited, stop catching SIGINT and SIGTERM
  signal(SIGINT, 0);
  signal(SIGTERM, 0);
#endif

#ifndef DISABLE_GUI
  delete window;
  qDebug("GUI was deleted!");
#else
  delete loader;
#endif
  qDebug("Deleting app...");
  delete app;
  qDebug("App was deleted! All good.");
  return ret;
}


