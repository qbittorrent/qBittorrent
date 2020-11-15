#pragma once

#include <libtorrent/peer_info.hpp>

#include <QSqlDatabase>
#include <QSqlField>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QSqlQuery>


class db_connection
{
public:
  static db_connection& instance()
  {
    static db_connection c;
    return c;
  }

  void init(const QString& db_path)
  {
    m_db.setDatabaseName(db_path);
    m_db.open();
  }

  QSqlDatabase connection() const
  {
    return m_db;
  }

protected:
  db_connection()
    : m_db(QSqlDatabase::addDatabase("QSQLITE"))
  {}

  ~db_connection()
  {
    m_db.close();
  }

private:
  QSqlDatabase m_db;
};


class peer_logger
{
public:
  explicit peer_logger(QSqlDatabase db, QString table)
    : m_model(nullptr, db)
  {
    if (!db.tables().contains(table)) {
      db.exec(QString(
                "CREATE TABLE '%1' ("
                "    'id'      INTEGER NOT NULL UNIQUE,"
                "    'ip'      TEXT NOT NULL UNIQUE,"
                "    'client'  TEXT,"
                "    'pid'     TEXT,"
                "    'tag'     TEXT,"
                "    PRIMARY KEY('id' AUTOINCREMENT)"
                ");").arg(table));
      db.commit();
    }

    m_model.setTable(table);
  }

  bool log_peer(const lt::peer_info& info, const std::string& tag = {})
  {
    QSqlRecord r;
    r.append(QSqlField(QStringLiteral("ip"), QVariant::String));
    r.append(QSqlField(QStringLiteral("client"), QVariant::String));
    r.append(QSqlField(QStringLiteral("pid"), QVariant::String));
    r.append(QSqlField(QStringLiteral("tag"), QVariant::String));

    r.setValue(QStringLiteral("ip"), QString::fromStdString(info.ip.address().to_string()));
    r.setValue(QStringLiteral("client"), QString::fromStdString(info.client));
    r.setValue(QStringLiteral("pid"), QString::fromStdString(info.pid.to_string().substr(0, 8)));
    r.setValue(QStringLiteral("tag"), QString::fromStdString(tag));

    return m_model.insertRecord(-1, r);
  }

private:
  QSqlTableModel m_model;
};
