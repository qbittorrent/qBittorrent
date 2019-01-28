/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2018  Vladimir Golovnev <glassez@yandex.ru>
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

#include "resumedatasavingmanager.h"

#include <QByteArray>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "base/bittorrent/db.h"
#include "base/logger.h"
#include "base/utils/fs.h"

static const char DB_CONNECTION_NAME[] = "saveManager";

using namespace BitTorrent::DB;

ResumeDataSavingManager::ResumeDataSavingManager(const QString &dbFilename)
    : m_dbFilename(dbFilename)
{
}

void ResumeDataSavingManager::save(const QString &hash, const QByteArray &data, int pos) const
{
    static bool setup = false;
    if (!setup) {
        // A connection to the database can be used only from the same thread that created it
        // So here we create another one from the main one
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", DB_CONNECTION_NAME);
        db.setDatabaseName(m_dbFilename);
        setup = true;
    }

    QSqlDatabase db = QSqlDatabase::database(DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare(QString("UPDATE %1 "
                          "SET %2 = :resume, %3 = :queue "
                          "WHERE %4 = :hash")
                  .arg(TABLE_NAME, COL_FASTRESUME, COL_QUEUE, COL_HASH));
    query.bindValue(":resume", data);
    query.bindValue(":queue", pos);
    query.bindValue(":hash", hash);

    if (!query.exec())
        Logger::instance()->addMessage(QString("ResumeDataSavingManager couldn't save data in '%1'. Error: %2")
                                       .arg(hash, query.lastError().text()), Log::WARNING);
}
