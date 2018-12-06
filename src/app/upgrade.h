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

#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/version.hpp>

#if LIBTORRENT_VERSION_NUM >= 10100
#include <libtorrent/bdecode.hpp>
#else
#include <libtorrent/lazy_entry.hpp>
#endif

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QString>

#ifndef DISABLE_GUI
#include <QMessageBox>
#endif
#ifdef Q_OS_MAC
#include <QSettings>
#endif

#include "base/logger.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"

bool userAcceptsUpgrade()
{
#ifdef DISABLE_GUI
    printf("\n*** %s ***\n", qUtf8Printable(QObject::tr("Upgrade")));
    char ret = '\0';
    do {
        printf("%s\n"
            , qUtf8Printable(QObject::tr("You updated from an older version that saved things differently. You must migrate to the new saving system. You will not be able to use an older version than v3.3.0 again. Continue? [y/n]")));
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
#endif // DISABLE_GUI

    return false;
}

bool upgradeResumeFile(const QString &filepath, const QVariantHash &oldTorrent = QVariantHash())
{
    QFile file1(filepath);
    if (!file1.open(QIODevice::ReadOnly))
        return false;

    QByteArray data = file1.readAll();
    file1.close();

    libtorrent::error_code ec;
#if LIBTORRENT_VERSION_NUM < 10100
        libtorrent::lazy_entry fastOld;
        libtorrent::lazy_bdecode(data.constData(), data.constData() + data.size(), fastOld, ec);
        if (ec || (fastOld.type() != libtorrent::lazy_entry::dict_t)) return false;
#else
        libtorrent::bdecode_node fastOld;
        libtorrent::bdecode(data.constData(), data.constData() + data.size(), fastOld, ec);
        if (ec || (fastOld.type() != libtorrent::bdecode_node::dict_t)) return false;
#endif

    libtorrent::entry fastNew;
    fastNew = fastOld;

    bool v3_3 = false;
    int queuePosition = 0;
    QString outFilePath = filepath;
    static const QRegularExpression rx(QLatin1String("([A-Fa-f0-9]{40})\\.fastresume\\.(.+)$"));
    const QRegularExpressionMatch rxMatch = rx.match(filepath);
    if (rxMatch.hasMatch()) {
        // Old v3.3.x format had a number at the end indicating the queue position.
        // The naming scheme was '<infohash>.fastresume.<queueposition>'.
        // However, QSaveFile, which uses QTemporaryFile internally, might leave
        // non-commited files behind eg after a crash. These files have the
        // naming scheme '<infohash>.fastresume.XXXXXX' where each X is a random
        // character. So we detect if the last part is present. Then check if it
        // is 6 chars long. If all the 6 chars are digits we assume it is an old
        // v3.3.x format. Otherwise it is considered a non-commited fastresume
        // and is deleted, because it may be a corrupted/incomplete fastresume.
        // NOTE: When the upgrade code is removed, we must continue to perform
        // cleanup of non-commited QSaveFile/QTemporaryFile fastresumes
        queuePosition = rxMatch.captured(2).toInt();
        if ((rxMatch.captured(2).size() == 6) && (queuePosition <= 99999)) {
            Utils::Fs::forceRemove(filepath);
            return true;
        }

        v3_3 = true;
        outFilePath.replace(QRegularExpression("\\.fastresume\\..+$"), ".fastresume");
    }
    else {
        queuePosition = fastOld.dict_find_int_value("qBt-queuePosition", 0);
        fastNew["qBt-name"] = oldTorrent.value("name").toString().toStdString();
        fastNew["qBt-tempPathDisabled"] = false;
    }

    // in versions < 3.3 we have -1 for seeding torrents, so we convert it to 0
    fastNew["qBt-queuePosition"] = (queuePosition >= 0 ? queuePosition : 0);

    if (v3_3) {
        QFileInfo oldFile(filepath);
        QFileInfo newFile(outFilePath);
        if (newFile.exists()
            && (oldFile.lastModified() < newFile.lastModified())) {
            Utils::Fs::forceRemove(filepath);
            return true;
        }
    }
    QFile file2(outFilePath);
    QVector<char> out;
    libtorrent::bencode(std::back_inserter(out), fastNew);
    if (file2.open(QIODevice::WriteOnly)) {
        if (file2.write(&out[0], out.size()) == out.size()) {
            if (v3_3)
                Utils::Fs::forceRemove(filepath);
            return true;
        }
    }

    return false;
}

bool upgrade(bool ask = true)
{
    // Upgrade preferences
    Preferences::instance()->upgrade();

    QString backupFolderPath = Utils::Fs::expandPathAbs(specialFolderLocation(SpecialFolder::Data) + "BT_backup");
    QDir backupFolderDir(backupFolderPath);

    // ****************************************************************************************
    // Silently converts old v3.3.x .fastresume files
    const QStringList backupFiles_3_3 = backupFolderDir.entryList(
                QStringList(QLatin1String("*.fastresume.*")), QDir::Files, QDir::Unsorted);
    for (const QString &backupFile : backupFiles_3_3)
        upgradeResumeFile(backupFolderDir.absoluteFilePath(backupFile));
    // ****************************************************************************************

    SettingsPtr oldResumeSettings = Profile::instance().applicationSettings(QLatin1String("qBittorrent-resume"));
    QString oldResumeFilename = oldResumeSettings->fileName();
    QVariantHash oldResumeData = oldResumeSettings->value("torrents").toHash();
    oldResumeSettings.reset();

    if (oldResumeData.isEmpty()) {
        Utils::Fs::forceRemove(oldResumeFilename);
        return true;
    }

    if (ask && !userAcceptsUpgrade()) return false;

    const QStringList backupFiles = backupFolderDir.entryList(
                QStringList(QLatin1String("*.fastresume")), QDir::Files, QDir::Unsorted);
    const QRegularExpression rx(QLatin1String("^([A-Fa-f0-9]{40})\\.fastresume$"));
    for (const QString &backupFile : backupFiles) {
        const QRegularExpressionMatch rxMatch = rx.match(backupFile);
        if (rxMatch.hasMatch()) {
            const QString hashStr = rxMatch.captured(1);
            if (upgradeResumeFile(backupFolderDir.absoluteFilePath(backupFile), oldResumeData[hashStr].toHash()))
                oldResumeData.remove(hashStr);
            else
                Logger::instance()->addMessage(QObject::tr("Couldn't migrate torrent with hash: %1").arg(hashStr), Log::WARNING);
        }
        else {
            Logger::instance()->addMessage(QObject::tr("Couldn't migrate torrent. Invalid fastresume file name: %1").arg(backupFile), Log::WARNING);
        }
    }

    for (auto i = oldResumeData.cbegin(); i != oldResumeData.cend(); ++i) {
        const QVariantHash oldTorrent = i.value().toHash();
        if (oldTorrent.value("is_magnet", false).toBool()) {
            const QString &hash = i.key();
            libtorrent::entry resumeData;
            resumeData["qBt-magnetUri"] = oldTorrent.value("magnet_uri").toString().toStdString();
            resumeData["qBt-paused"] = false;
            resumeData["qBt-forced"] = false;

            resumeData["qBt-savePath"] = oldTorrent.value("save_path").toString().toStdString();
            resumeData["qBt-ratioLimit"] = static_cast<int>(oldTorrent.value("max_ratio", -2).toReal() * 1000);
            resumeData["qBt-label"] = oldTorrent.value("label").toString().toStdString();
            resumeData["qBt-name"] = oldTorrent.value("name").toString().toStdString();
            resumeData["qBt-seedStatus"] = oldTorrent.value("seed").toBool();
            resumeData["qBt-tempPathDisabled"] = false;

            int queuePosition = oldTorrent.value("priority", 0).toInt();
            resumeData["qBt-queuePosition"] = (queuePosition >= 0 ? queuePosition : 0);

            QString filename = QString("%1.fastresume").arg(hash);
            QString filepath = backupFolderDir.absoluteFilePath(filename);

            QFile resumeFile(filepath);
            QVector<char> out;
            libtorrent::bencode(std::back_inserter(out), resumeData);
            if (resumeFile.open(QIODevice::WriteOnly))
                resumeFile.write(&out[0], out.size());
        }
    }

    int counter = 0;
    QString backupResumeFilename = oldResumeFilename + ".bak";
    while (QFile::exists(backupResumeFilename)) {
        ++counter;
        backupResumeFilename = oldResumeFilename + ".bak" + QString::number(counter);
    }
    QFile::rename(oldResumeFilename, backupResumeFilename);

    return true;
}

#ifdef Q_OS_MAC
void migratePlistToIni(const QString &application)
{
    QSettings iniFile(QSettings::IniFormat, QSettings::UserScope, "qBittorrent", application);
    if (!iniFile.allKeys().isEmpty()) return; // We copy the contents of plist, only if inifile does not exist(is empty).

    QSettings *plistFile = new QSettings("qBittorrent", application);
    plistFile->setFallbacksEnabled(false);
    const QStringList plist = plistFile->allKeys();
    if (!plist.isEmpty()) {
        for (const QString &key : plist)
            iniFile.setValue(key, plistFile->value(key));
        plistFile->clear();
    }

    QString plistPath = plistFile->fileName();
    delete plistFile;
    Utils::Fs::forceRemove(plistPath);
}

void macMigratePlists()
{
    migratePlistToIni("qBittorrent-data");
    migratePlistToIni("qBittorrent-rss");
    migratePlistToIni("qBittorrent");
}
#endif  // Q_OS_MAC

#ifndef DISABLE_GUI
void migrateRSS()
{
    // Copy old feed items to new file if needed
    SettingsPtr qBTRSS = Profile::instance().applicationSettings(QLatin1String("qBittorrent-rss-feeds"));
    if (!qBTRSS->allKeys().isEmpty()) return; // We move the contents of RSS old_items only if inifile does not exist (is empty).

    SettingsPtr qBTRSSLegacy = Profile::instance().applicationSettings(QLatin1String("qBittorrent-rss"));
    QHash<QString, QVariant> allOldItems = qBTRSSLegacy->value("old_items", QHash<QString, QVariant>()).toHash();

    if (!allOldItems.empty()) {
        qDebug("Moving %d old items for feeds to qBittorrent-rss-feeds", allOldItems.size());
        qBTRSS->setValue("old_items", allOldItems);
        qBTRSSLegacy->remove("old_items");
    }
}
#endif // DISABLE_GUI

#endif // UPGRADE_H
