/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
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
 */

#include <QDebug>
#include <QFileInfo>
#include <QLocale>
#include <QLibraryInfo>
#include <QSysInfo>
#ifdef Q_OS_WIN
#include <Windows.h>
#include <QSharedMemory>
#endif

#if (!defined(DISABLE_GUI) && defined(Q_OS_MAC))
#include <QFont>
#endif

#include "application.h"
#include "preferences.h"

Application::Application(const QString &id, int &argc, char **argv)
    : BaseApplication(id, argc, argv)
{
#if defined(Q_OS_MACX) && !defined(DISABLE_GUI)
    if (QSysInfo::MacintoshVersion > QSysInfo::MV_10_8) {
        // fix Mac OS X 10.9 (mavericks) font issue
        // https://bugreports.qt-project.org/browse/QTBUG-32789
        QFont::insertSubstitution(".Lucida Grande UI", "Lucida Grande");
    }
#endif
    setApplicationName("qBittorrent");
    initializeTranslation();
#ifndef DISABLE_GUI
    setStyleSheet("QStatusBar::item { border-width: 0; }");
    setQuitOnLastWindowClosed(false);
#endif
}

#ifdef Q_OS_WIN
bool Application::isRunning()
{
    bool running = BaseApplication::isRunning();
    QSharedMemory *sharedMem = new QSharedMemory(id() + QLatin1String("-shared-memory-key"), this);
    if (!running) {
        // First instance creates shared memory and store PID
        if (sharedMem->create(sizeof(DWORD)) && sharedMem->lock()) {
            *(static_cast<DWORD*>(sharedMem->data())) = ::GetCurrentProcessId();
            sharedMem->unlock();
        }
    }
    else {
        // Later instances attach to shared memory and retrieve PID
        if (sharedMem->attach() && sharedMem->lock()) {
            ::AllowSetForegroundWindow(*(static_cast<DWORD*>(sharedMem->data())));
            sharedMem->unlock();
        }
    }

    if (!sharedMem->isAttached())
        qWarning() << "Failed to initialize shared memory: " << sharedMem->errorString();

    return running;
}
#endif

void Application::initializeTranslation()
{
    Preferences* const pref = Preferences::instance();
    // Load translation
    QString locale = pref->getLocale();
    if (locale.isEmpty()) {
        locale = QLocale::system().name();
        pref->setLocale(locale);
    }

    if (m_qtTranslator.load(
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
            QString::fromUtf8("qtbase_") + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath)) ||
        m_qtTranslator.load(
#endif
            QString::fromUtf8("qt_") + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
            qDebug("Qt %s locale recognized, using translation.", qPrintable(locale));
    }
    else {
        qDebug("Qt %s locale unrecognized, using default (en).", qPrintable(locale));
    }
    installTranslator(&m_qtTranslator);

    if (m_translator.load(QString::fromUtf8(":/lang/qbittorrent_") + locale)) {
        qDebug("%s locale recognized, using translation.", qPrintable(locale));
    }
    else {
        qDebug("%s locale unrecognized, using default (en).", qPrintable(locale));
    }
    installTranslator(&m_translator);

#ifndef DISABLE_GUI
    if (locale.startsWith("ar") || locale.startsWith("he")) {
        qDebug("Right to Left mode");
        setLayoutDirection(Qt::RightToLeft);
    }
    else {
        setLayoutDirection(Qt::LeftToRight);
    }
#endif
}
