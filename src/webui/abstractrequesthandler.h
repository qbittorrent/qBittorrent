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

#ifndef ABSTRACTREQUESTHANDLER_H
#define ABSTRACTREQUESTHANDLER_H

#include <QString>
#include "httptypes.h"

class WebApplication;
struct WebSession;

class AbstractRequestHandler
{
  friend class WebApplication;

public:
  AbstractRequestHandler(
      const HttpRequest& request, const HttpEnvironment& env,
      WebApplication* app);

  HttpResponse run();

protected:
  virtual void processRequest() = 0;

  void status(uint code, const QString& text);
  void header(const QString& name, const QString& value);
  void print(const QString& text, const QString& type = CONTENT_TYPE_HTML);
  void print(const QByteArray& data, const QString& type = CONTENT_TYPE_HTML);
  void printFile(const QString& path);

  // Session management
  bool sessionActive() const { return session_ != 0; }
  void sessionInitialize();
  void sessionStart();
  void sessionEnd();

  // Ban management
  bool isBanned() const;
  int failedAttempts() const;
  void resetFailedAttempts();
  void increaseFailedAttempts();

  bool isAuthNeeded();

  // save data to temporary file on disk and return its name (or empty string if fails)
  static QString saveTmpFile(const QByteArray& data);

  inline WebSession* session() { return session_; }
  inline HttpRequest request() const { return request_; }
  inline HttpEnvironment env() const { return env_; }

private:
  WebApplication* app_;
  WebSession* session_;
  const HttpRequest request_;
  const HttpEnvironment env_;
  HttpResponse response_;

  void print_impl(const QByteArray& data, const QString& type);
};

#endif // ABSTRACTREQUESTHANDLER_H
