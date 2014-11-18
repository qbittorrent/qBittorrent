/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
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

#ifdef DISABLE_GUI
#include <QCoreApplication>
#else
#include <QApplication>
#endif
#include <QDateTime>
#include <QTimer>
#include <QFile>
#include <QDebug>
#include "preferences.h"
#include "requesthandler.h"
#include "webapplication.h"

// UnbanTimer

class UnbanTimer: public QTimer
{
public:
  UnbanTimer(const QHostAddress& peer_ip, QObject *parent)
    : QTimer(parent), m_peerIp(peer_ip)
  {
    setSingleShot(true);
    setInterval(BAN_TIME);
  }

  inline const QHostAddress& peerIp() const { return m_peerIp; }

private:
  QHostAddress m_peerIp;
};

// WebApplication

WebApplication::WebApplication(QObject *parent)
  : QObject(parent)
{
}

WebApplication::~WebApplication()
{
  // cleanup sessions data
  foreach (WebSession* session, sessions_.values())
    delete session;
}

WebApplication *WebApplication::instance()
{
  static WebApplication inst;
  return &inst;
}

void WebApplication::UnbanTimerEvent()
{
  UnbanTimer* ubantimer = static_cast<UnbanTimer*>(sender());
  qDebug("Ban period has expired for %s", qPrintable(ubantimer->peerIp().toString()));
  clientFailedAttempts_.remove(ubantimer->peerIp());
  ubantimer->deleteLater();
}

bool WebApplication::sessionInitialize(AbstractRequestHandler* _this)
{
  if (_this->session_ == 0)
  {
    QString cookie = _this->request_.headers.value("cookie");
    //qDebug() << Q_FUNC_INFO << "cookie: " << cookie;

    QString sessionId;
    const QString SID_START = C_SID + "=";
    int pos = cookie.indexOf(SID_START);
    if (pos >= 0)
    {
      pos += SID_START.length();
      int end = cookie.indexOf(QRegExp("[,;]"), pos);
      sessionId = cookie.mid(pos, end >= 0 ? end - pos : end);
    }

    // TODO: Additional session check

    if (!sessionId.isNull())
    {
      if (sessions_.contains(sessionId))
      {
        _this->session_ = sessions_[sessionId];
        return true;
      }
      else
      {
        qDebug() << Q_FUNC_INFO << "session does not exist!";
      }
    }
  }

  return false;
}

bool WebApplication::readFile(const QString& path, QByteArray &data, QString &type)
{
  QString ext = "";
  int index = path.lastIndexOf('.') + 1;
  if (index > 0)
    ext = path.mid(index);

  // find translated file in cache
  if (translatedFiles_.contains(path))
  {
    data = translatedFiles_[path];
  }
  else
  {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
      qDebug("File %s was not found!", qPrintable(path));
      return false;
    }

    data = file.readAll();
    file.close();

    // Translate the file
    if ((ext == "html") || ((ext == "js") && !path.endsWith("excanvas-compressed.js")))
    {
      QString dataStr = QString::fromUtf8(data.constData());
      translateDocument(dataStr);

      if (path.endsWith("about.html"))
      {
        dataStr.replace("${VERSION}", VERSION);
      }

      data = dataStr.toUtf8();
      translatedFiles_[path] = data; // cashing translated file
    }
  }

  type = CONTENT_TYPE_BY_EXT[ext];
  return true;
}

QString WebApplication::generateSid()
{
  QString sid;

  qsrand(QDateTime::currentDateTime().toTime_t());
  do
  {
    const size_t size = 6;
    quint32 tmp[size];

    for (size_t i = 0; i < size; ++i)
      tmp[i] = qrand();

    sid = QByteArray::fromRawData(reinterpret_cast<const char *>(tmp), sizeof(quint32) * size).toBase64();
  }
  while (sessions_.contains(sid));

  return sid;
}

void WebApplication::translateDocument(QString& data)
{
  const QRegExp regex(QString::fromUtf8("_\\(([\\w\\s?!:\\/\\(\\),%µ&\\-\\.]+)\\)"));
  const QRegExp mnemonic("\\(?&([a-zA-Z]?\\))?");
  const std::string contexts[] = {
    "TransferListFiltersWidget", "TransferListWidget", "PropertiesWidget",
    "HttpServer", "confirmDeletionDlg", "TrackerList", "TorrentFilesModel",
    "options_imp", "Preferences", "TrackersAdditionDlg", "ScanFoldersModel",
    "PropTabBar", "TorrentModel", "downloadFromURL", "MainWindow", "misc"
  };
  const size_t context_count = sizeof(contexts) / sizeof(contexts[0]);
  int i = 0;
  bool found = true;

  const QString locale = Preferences::instance()->getLocale();
  bool isTranslationNeeded = !locale.startsWith("en") || locale.startsWith("en_AU") || locale.startsWith("en_GB");

  while(i < data.size() && found)
  {
    i = regex.indexIn(data, i);
    if (i >= 0)
    {
      //qDebug("Found translatable string: %s", regex.cap(1).toUtf8().data());
      QByteArray word = regex.cap(1).toUtf8();

      QString translation = word;
      if (isTranslationNeeded)
      {
        size_t context_index = 0;
        while ((context_index < context_count) && (translation == word))
        {
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
          translation = qApp->translate(contexts[context_index].c_str(), word.constData(), 0, QCoreApplication::UnicodeUTF8, 1);
#else
          translation = qApp->translate(contexts[context_index].c_str(), word.constData(), 0, 1);
#endif
          ++context_index;
        }
      }
      // Remove keyboard shortcuts
      translation.replace(mnemonic, "");

      data.replace(i, regex.matchedLength(), translation);
      i += translation.length();
    }
    else
    {
        found = false; // no more translatable strings
    }
  }
}

bool WebApplication::isBanned(const AbstractRequestHandler *_this) const
{
  return clientFailedAttempts_.value(_this->env_.clientAddress, 0) >= MAX_AUTH_FAILED_ATTEMPTS;
}

int WebApplication::failedAttempts(const AbstractRequestHandler* _this) const
{
  return clientFailedAttempts_.value(_this->env_.clientAddress, 0);
}

void WebApplication::resetFailedAttempts(AbstractRequestHandler* _this)
{
  clientFailedAttempts_.remove(_this->env_.clientAddress);
}

void WebApplication::increaseFailedAttempts(AbstractRequestHandler* _this)
{
  const int nb_fail = clientFailedAttempts_.value(_this->env_.clientAddress, 0) + 1;

  clientFailedAttempts_[_this->env_.clientAddress] = nb_fail;
  if (nb_fail == MAX_AUTH_FAILED_ATTEMPTS)
  {
    // Max number of failed attempts reached
    // Start ban period
    UnbanTimer* ubantimer = new UnbanTimer(_this->env_.clientAddress, this);
    connect(ubantimer, SIGNAL(timeout()), SLOT(UnbanTimerEvent()));
    ubantimer->start();
  }
}

bool WebApplication::sessionStart(AbstractRequestHandler *_this)
{
  if (_this->session_ == 0)
  {
    _this->session_ = new WebSession(generateSid());
    sessions_[_this->session_->id] = _this->session_;
    return true;
  }

  return false;
}

bool WebApplication::sessionEnd(AbstractRequestHandler *_this)
{
  if ((_this->session_ != 0) && (sessions_.contains(_this->session_->id)))
  {
    sessions_.remove(_this->session_->id);
    delete _this->session_;
    _this->session_ = 0;
    return true;
  }

  return false;
}

QStringMap WebApplication::initializeContentTypeByExtMap()
{
  QStringMap map;

  map["htm"] = CONTENT_TYPE_HTML;
  map["html"] = CONTENT_TYPE_HTML;
  map["css"] = CONTENT_TYPE_CSS;
  map["gif"] = CONTENT_TYPE_GIF;
  map["png"] = CONTENT_TYPE_PNG;
  map["js"] = CONTENT_TYPE_JS;

  return map;
}

const QStringMap WebApplication::CONTENT_TYPE_BY_EXT = WebApplication::initializeContentTypeByExtMap();
