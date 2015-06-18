/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#ifndef UPGRADE_H
#define UPGRADE_H

#include <libtorrent/lazy_entry.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>

#include <QString>
#include <QDir>
#include <QFile>
#ifndef DISABLE_GUI
#include <QMessageBox>
#endif

#include "core/logger.h"
#include "core/utils/fs.h"
#include "core/utils/misc.h"

bool userAcceptsUpgrade()
{
#ifdef DISABLE_GUI
    std::cout << std::endl << "*** " << qPrintable(QObject::tr("Upgrade")) << " ***" << std::endl;
    char ret = '\0';
    do {
        std::cout << qPrintable(QObject::tr("You updated from an older version that saved things differently. You must migrate to the new saving system. You will not be able to use an older version than v3.3.0 again. Continue? [y/n]")) << std::endl;
        ret = getchar(); // Read pressed key
    }
    while ((ret != 'y') && (ret != 'Y') && (ret != 'n') && (ret != 'N'));

    if ((ret == 'y') || (ret == 'Y'))
        return true;
#else
    QMessageBox msgBox;
    msgBox.setText(QObject::tr("You updated from an older version that saved things differently. You must migrate to the new saving system. If you continue, you will not be able to use an older version than v3.3.0 again."));
    msgBox.setWindowTitle(QObject::tr("Upgrade"));
    msgBox.addButton(QMessageBox::Abort);
    msgBox.addButton(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Abort);
    msgBox.show(); // Need to be shown or to moveToCenter does not work
    msgBox.move(Utils::Misc::screenCenter(&msgBox));
    if (msgBox.exec() == QMessageBox::Ok)
        return true;
#endif

    return false;
}

bool upgradeResumeFile(const QString &filepath)
{
    QFile file1(filepath);
    if (!file1.open(QIODevice::ReadOnly))
        return false;

    QByteArray data = file1.readAll();
    file1.close();

    libtorrent::lazy_entry fastOld;
    libtorrent::error_code ec;
    libtorrent::lazy_bdecode(data.constData(), data.constData() + data.size(), fastOld, ec);
    if ((fastOld.type() != libtorrent::lazy_entry::dict_t) && !ec) return false;

    libtorrent::entry fastNew;
    fastNew = fastOld;
    fastNew["qBt-addedTime"] = fastOld.dict_find_int_value("qBt-added_time");

    int priority = fastOld.dict_find_int_value("qBt-queuePosition");
    QFile file2(QString("%1.%2").arg(filepath).arg(priority > 0 ? priority : 0));
    QVector<char> out;
    libtorrent::bencode(std::back_inserter(out), fastNew);
    if (file2.open(QIODevice::WriteOnly)) {
        if (file2.write(&out[0], out.size()) == out.size()) {
            Utils::Fs::forceRemove(filepath);
            return true;
        }
    }

    return false;
}

bool upgrade()
{
    QString backupFolderPath = Utils::Fs::expandPathAbs(Utils::Fs::QDesktopServicesDataLocation() + "BT_backup");
    QDir backupFolderDir(backupFolderPath);
    if (!backupFolderDir.exists()) return true;

    QStringList backupFiles = backupFolderDir.entryList(QStringList() << QLatin1String("*.fastresume"), QDir::Files, QDir::Unsorted);
    if (!backupFiles.isEmpty()) {
        if (!userAcceptsUpgrade()) return false;

        QRegExp rx(QLatin1String("^([A-Fa-f0-9]{40})\\.fastresume$"));
        foreach (QString backupFile, backupFiles) {
            if (rx.indexIn(backupFile) != -1) {
                if (!upgradeResumeFile(backupFolderDir.absoluteFilePath(backupFile)))
                    Logger::instance()->addMessage(QObject::tr("Couldn't migrate torrent with hash: %1").arg(rx.cap(1)), Log::WARNING);
            }
            else {
                Logger::instance()->addMessage(QObject::tr("Couldn't migrate torrent. Invalid fastresume file name: %1").arg(backupFile), Log::WARNING);
            }
        }
    }

    return true;
}

#endif // UPGRADE_H
