/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2019  Mike Tzou (Chocobo1)
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

#include "upgrade.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#ifndef DISABLE_GUI
#include <QMessageBox>
#endif

#include <libtorrent/bdecode.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/version.hpp>

#include "base/bittorrent/db.h"
#include "base/bittorrent/magneturi.h"
#include "base/bittorrent/session.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"

namespace
{
    void exportWebUIHttpsFiles()
    {
        const auto migrate = [](const QString &oldKey, const QString &newKey, const QString &savePath) {
            SettingsStorage *settingsStorage {SettingsStorage::instance()};
            const QByteArray oldData {settingsStorage->loadValue(oldKey).toByteArray()};
            const QString newData {settingsStorage->loadValue(newKey).toString()};
            const QString errorMsgFormat {QObject::tr("Migrate preferences failed: WebUI https, file: \"%1\", error: \"%2\"")};

            if (!newData.isEmpty() || oldData.isEmpty())
                return;

            QFile file(savePath);
            if (!file.open(QIODevice::WriteOnly)) {
                LogMsg(errorMsgFormat.arg(savePath, file.errorString()) , Log::WARNING);
                return;
            }
            if (file.write(oldData) != oldData.size()) {
                file.close();
                Utils::Fs::forceRemove(savePath);
                LogMsg(errorMsgFormat.arg(savePath, QLatin1String("Write incomplete.")) , Log::WARNING);
                return;
            }

            settingsStorage->storeValue(newKey, savePath);
            settingsStorage->removeValue(oldKey);

            LogMsg(QObject::tr("Migrated preferences: WebUI https, exported data to file: \"%1\"").arg(savePath)
                , Log::INFO);
        };

        const QString configPath {specialFolderLocation(SpecialFolder::Config)};
        migrate(QLatin1String("Preferences/WebUI/HTTPS/Certificate")
            , QLatin1String("Preferences/WebUI/HTTPS/CertificatePath")
            , Utils::Fs::toNativePath(configPath + QLatin1String("WebUICertificate.crt")));
        migrate(QLatin1String("Preferences/WebUI/HTTPS/Key")
            , QLatin1String("Preferences/WebUI/HTTPS/KeyPath")
            , Utils::Fs::toNativePath(configPath + QLatin1String("WebUIPrivateKey.pem")));
    }
    
    bool readFile(const QString &path, QByteArray &buf)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug("Cannot read file %s: %s", qUtf8Printable(path), qUtf8Printable(file.errorString()));
            return false;
        }

        buf = file.readAll();
        return true;
    }

    bool loadTorrentParams(const QByteArray &data, int &prio, QByteArray &magnetUri)
    {
        namespace libt = libtorrent;

        libt::error_code ec;
        libt::bdecode_node fast;
        libt::bdecode(data.constData(), data.constData() + data.size(), fast, ec);
        if (ec || (fast.type() != libt::bdecode_node::dict_t)) return false;

        magnetUri = QByteArray::fromStdString(fast.dict_find_string_value("qBt-magnetUri"));
        prio = fast.dict_find_int_value("qBt-queuePosition");

        return true;
    }

    void prepareTorrentData(const QSqlQuery &query, int &queuePos, QByteArray &metadata,
                            QByteArray &fastresume, bool &isMagnetUri)
    {
        metadata = query.value(1).toByteArray();
        QByteArray tmpFastresume = query.value(2).toByteArray();

        namespace libt = libtorrent;

        libt::error_code ec;
        libt::bdecode_node fast;
        libt::bdecode(tmpFastresume.constData(), tmpFastresume.constData() + tmpFastresume.size(), fast, ec);
        if (ec || (fast.type() != libt::bdecode_node::dict_t)) return;

        libt::entry resumeData(libt::entry::dictionary_t);
        resumeData = fast;

        resumeData["qBt-savePath"] = query.value(4).toString().toStdString();
        resumeData["qBt-ratioLimit"] = query.value(5).toInt();
        resumeData["qBt-seedingTimeLimit"] = query.value(6).toInt();
        resumeData["qBt-category"] = query.value(7).toString().toStdString();
        /*???*/ resumeData["qBt-tags"] = query.value(8).toString().toStdString();
        resumeData["qBt-name"] = query.value(9).toString().toStdString();
        resumeData["qBt-seedStatus"] = query.value(10).toBool();
        resumeData["qBt-tempPathDisabled"] = query.value(11).toBool();
        queuePos = query.value(3).toInt();
        resumeData["qBt-queuePosition"] = (queuePos + 1); // qBt starts queue at 1. Compatibility with pre 4.1.5 versions
        resumeData["qBt-hasRootFolder"] = query.value(12).toBool();

        // The 'metadata' var might be a magnet URI saved into the COL_METADATA column before the metadata
        // were received from the network. Internally addTorrent_impl() checks if the constructed below
        // MagnetUri is valid.
        BitTorrent::MagnetUri magnetUri{metadata};
        if (magnetUri.isValid()) {
            isMagnetUri = true;
            resumeData["qBt-magnetUri"] = metadata.toStdString();
            resumeData["qBt-paused"] = query.value(13).toBool();
            resumeData["qBt-forced"] = query.value(14).toBool();
            resumeData["qBt-firstLastPiecePriority"] = query.value(15).toBool();
            resumeData["qBt-sequential"] = query.value(16).toBool();
        }

        libt::bencode(std::back_inserter(fastresume), resumeData);
    }

    bool userAcceptsUpgrade()
    {
#ifdef DISABLE_GUI
        printf("\n*** %s ***\n", qUtf8Printable(QObject::tr("Upgrade")));
        char ret = '\0';
        do {
            printf("%s\n"
                   , qUtf8Printable(QObject::tr("You updated from an older version that saved things differently. You must migrate to the new saving system. You will not be able to use an older version than v4.2.0 again. Continue? [y/n]")));
            ret = getchar(); // Read pressed key
        }
        while ((ret != 'y') && (ret != 'Y') && (ret != 'n') && (ret != 'N'));

        if ((ret == 'y') || (ret == 'Y'))
            return true;
#else
        QMessageBox msgBox;
        msgBox.setText(QObject::tr("You updated from an older version that saved things differently. You must migrate to the new saving system. If you continue, you will not be able to use an older version than v4.2.0 again."));
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
}

bool upgrade(const bool ask)
{
    exportWebUIHttpsFiles();
    
    using namespace BitTorrent::DB;
    const QString dbFolderPath = Utils::Fs::expandPathAbs(specialFolderLocation(SpecialFolder::Data));
    const QDir dbFolderDir(dbFolderPath);
    const QString dbPath = dbFolderDir.absoluteFilePath(QLatin1String{FILENAME});
    // If db exists don't try to upgrade
    if (QFile::exists(dbPath))
        return true;

    const QString backupFolderPath = Utils::Fs::expandPathAbs(specialFolderLocation(SpecialFolder::Data) + "BT_backup");
    const QDir backupFolderDir(backupFolderPath);
    QStringList fastresumes = backupFolderDir.entryList(QStringList(QLatin1String("*.fastresume")), QDir::Files, QDir::Unsorted);
    // This is a brand new install
    if (fastresumes.isEmpty())
        return true;

    if (ask && !userAcceptsUpgrade()) return false;

    const QRegularExpression rx(QLatin1String("^([A-Fa-f0-9]{40})\\.fastresume$"));
    BitTorrent::Session::initiliazeDB();

    QSqlDatabase db = QSqlDatabase::database(MAIN_CONNECTION_NAME);
    db.transaction();
    QSqlQuery query(db);
    query.prepare(QString("INSERT INTO %1 (%2, %3, %4, %5, %6) "
                          "VALUES (:hash, :metadata, :fastresume, :queue)")
                  .arg(TABLE_NAME, COL_HASH, COL_METADATA, COL_FASTRESUME, COL_QUEUE));

    const auto insertDBRecord = [&backupFolderDir, &query](const QString &hash, bool customQueue = false, const int queue = -1) -> bool
    {
        const QString metadataPath = backupFolderDir.absoluteFilePath(QString("%1.torrent").arg(hash));
        const QString fastresumePath = backupFolderDir.absoluteFilePath(QString("%1.fastresume").arg(hash));
        QByteArray fastResumeData;
        QByteArray metadata;
        int queuePosition = 0;

        if (!readFile(fastresumePath, fastResumeData)
            || !loadTorrentParams(fastResumeData, queuePosition, metadata)) return false;

        // At the 1st check if 'metadata' isn't empty it means it contains a magnet URI
        if (metadata.isEmpty() && !readFile(metadataPath, metadata)) return false;

        query.bindValue(":hash", hash);
        query.bindValue(":metadata", metadata);
        query.bindValue(":fastresume", fastResumeData);
        query.bindValue(":queue", customQueue ? queue : queuePosition);
        if (!query.exec()) {
            LogMsg(QCoreApplication::translate("BitTorrent::Session", "Couldn't save torrent %1 into the database. Error: %2")
                   .arg(hash, query.lastError().text()), Log::CRITICAL);
            return false;
        }

        Utils::Fs::forceRemove(metadataPath);
        Utils::Fs::forceRemove(fastresumePath);
        return true;
    };

    bool isQueueingSystemEnabled = SettingsStorage::instance()->loadValue("BitTorrent/Session/QueueingSystemEnabled", true).toBool();

    if (isQueueingSystemEnabled) {
        const QString queueFilePath {backupFolderDir.absoluteFilePath(QLatin1String {"queue"})};
        QFile queueFile {queueFilePath};

        // TODO: The following code is deprecated in 4.1.5. Remove after several releases in 4.2.x.
        // === BEGIN DEPRECATED CODE === //
        if (!queueFile.exists()) {
            // Resume downloads in a legacy manner
            for (const QString &fastresumeName : asConst(fastresumes)) {
                const QRegularExpressionMatch rxMatch = rx.match(fastresumeName);
                if (!rxMatch.hasMatch()) continue;

                insertDBRecord(rxMatch.captured(1));
            }

            db.commit();
            return true;
        }
        // === END DEPRECATED CODE === //

        if (queueFile.open(QFile::ReadOnly)) {
            QByteArray line;
            int queuePosition = 0;
            while (!(line = queueFile.readLine()).isEmpty()) {
                const QString fastresumeName = QString::fromLatin1(line.trimmed()) + QLatin1String {".fastresume"};
                const QRegularExpressionMatch rxMatch = rx.match(fastresumeName);
                if (!rxMatch.hasMatch()) continue;

                fastresumes.removeAll(fastresumeName);
                if (insertDBRecord(rxMatch.captured(1), true, queuePosition))
                    ++queuePosition;
            }
        }
        else {
            LogMsg(QCoreApplication::translate("BitTorrent::Session", "Couldn't load torrents queue from '%1'. Error: %2")
                   .arg(queueFile.fileName(), queueFile.errorString()), Log::WARNING);
        }

        queueFile.close();
        Utils::Fs::forceRemove(queueFilePath);
    }

    for (const QString &fastresumeName : asConst(fastresumes)) {
        const QRegularExpressionMatch rxMatch = rx.match(fastresumeName);
        if (!rxMatch.hasMatch()) continue;

        insertDBRecord(rxMatch.captured(1), true, -1); // -1 because either queueing is disabled or these are seeds
    }

    db.commit();
    return true;
}

void exportTorrentData()
{
    const QString backupFolderPath = Utils::Fs::expandPathAbs(specialFolderLocation(SpecialFolder::Data) + "BT_backup");
    const QDir backupFolderDir(backupFolderPath);

    using namespace BitTorrent::DB;
    QSqlQuery query;
    query.setForwardOnly(true);
    query.exec(QString("SELECT %1, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13, %14, %15, %16, %17 FROM %18 ORDER BY %4 ASC")
               .arg(COL_HASH, COL_METADATA, COL_FASTRESUME, COL_QUEUE, COL_SAVE_PATH,
                    COL_RATIO_LIMIT, COL_SEEDING_TIME_LIMIT, COL_CATEGORY, COL_TAGS)
               .arg(COL_NAME, COL_SEED_STATUS, COL_TMP_PATH_DISABLED, COL_HAS_ROOT_FOLDER,
                    COL_PAUSED, COL_FORCED, COL_HAS_FIRST_LAST_PIECE_PRIO, COL_SEQUENTIAL, TABLE_NAME));

    QByteArray queueList;
    while (query.next()) {
        const QString hash = query.value(0).toString();
        int queuePosition = -1;
        QByteArray metadata;
        QByteArray fastresume;
        bool isMagnetUri = false;

        prepareTorrentData(query, queuePosition, metadata, fastresume, isMagnetUri);

        if (queuePosition >= 0)
            queueList += hash.toLatin1() + '\n';

        QFile file(backupFolderDir.absoluteFilePath(QString("%1.fastresume").arg(hash)));
        if (file.open(QIODevice::WriteOnly)) {
            file.write(fastresume.constData(), fastresume.size());
            file.close();
            if (!isMagnetUri) {
                file.setFileName(backupFolderDir.absoluteFilePath(QString("%1.torrent").arg(hash)));
                if (file.open(QIODevice::WriteOnly))
                    file.write(metadata.constData(), metadata.size());
            }
        }
    }

    if (queueList.isEmpty()) return;

    QFile file(backupFolderDir.absoluteFilePath(QLatin1String{"queue"}));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(queueList.constData(), queueList.size());
    }
}
