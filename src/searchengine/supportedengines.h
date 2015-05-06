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

#ifndef SEARCHENGINES_H
#define SEARCHENGINES_H

#include <QHash>
#include <QStringList>
#include <QDomDocument>
#include <QDomNode>
#include <QDomElement>
#include <QProcess>
#include <QDir>
#include <QApplication>
#include <QDebug>

#include "core/utils/fs.h"
#include "core/preferences.h"

class SearchCategories: public QObject, public QHash<QString, QString> {
  Q_OBJECT

public:
  SearchCategories() {
    (*this)["all"] = tr("All categories");
    (*this)["movies"] = tr("Movies");
    (*this)["tv"] = tr("TV shows");
    (*this)["music"] = tr("Music");
    (*this)["games"] = tr("Games");
    (*this)["anime"] = tr("Anime");
    (*this)["software"] = tr("Software");
    (*this)["pictures"] = tr("Pictures");
    (*this)["books"] = tr("Books");
  }
};

class SupportedEngine {
private:
  QString name;
  QString full_name;
  QString url;
  QStringList supported_categories;
  bool enabled;

public:
  SupportedEngine(QDomElement engine_elem) {
    name = engine_elem.tagName();
    full_name = engine_elem.elementsByTagName("name").at(0).toElement().text();
    url = engine_elem.elementsByTagName("url").at(0).toElement().text();
    supported_categories = engine_elem.elementsByTagName("categories").at(0).toElement().text().split(" ");
    QStringList disabled_engines = Preferences::instance()->getSearchEngDisabled();
    enabled = !disabled_engines.contains(name);
  }

  QString getName() const { return name; }
  QString getUrl() const { return url; }
  QString getFullName() const { return full_name; }
  QStringList getSupportedCategories() const { return supported_categories; }
  bool isEnabled() const { return enabled; }
  void setEnabled(bool _enabled) {
    enabled = _enabled;
    // Save to Hard disk
    Preferences* const pref = Preferences::instance();
    QStringList disabled_engines = pref->getSearchEngDisabled();
    if (enabled) {
      disabled_engines.removeAll(name);
    } else {
      disabled_engines.append(name);
    }
    pref->setSearchEngDisabled(disabled_engines);
  }
};

class SupportedEngines: public QObject, public QHash<QString, SupportedEngine*> {
  Q_OBJECT

signals:
  void newSupportedEngine(QString name);

public:
  SupportedEngines() {
    update();
  }

  ~SupportedEngines() {
    qDeleteAll(this->values());
  }

  QStringList enginesAll() const {
    QStringList engines;
    foreach (const SupportedEngine *engine, values()) engines << engine->getName();
    return engines;
  }

  QStringList enginesEnabled() const {
    QStringList engines;
    foreach (const SupportedEngine *engine, values()) {
      if (engine->isEnabled())
        engines << engine->getName();
    }
    return engines;
  }

  QStringList supportedCategories() const {
    QStringList supported_cat;
    foreach (const SupportedEngine *engine, values()) {
      if (engine->isEnabled()) {
        const QStringList &s = engine->getSupportedCategories();
        foreach (QString cat, s) {
          cat = cat.trimmed();
          if (!cat.isEmpty() && !supported_cat.contains(cat))
            supported_cat << cat;
        }
      }
    }
    return supported_cat;
  }

public slots:
  void update() {
    QProcess nova;
    nova.setEnvironment(QProcess::systemEnvironment());
    QStringList params;
    params << Utils::Fs::toNativePath(Utils::Fs::searchEngineLocation()+"/nova2.py");
    params << "--capabilities";
    nova.start("python", params, QIODevice::ReadOnly);
    nova.waitForStarted();
    nova.waitForFinished();
    QString capabilities = QString(nova.readAll());
    QDomDocument xml_doc;
    if (!xml_doc.setContent(capabilities)) {
      qWarning() << "Could not parse Nova search engine capabilities, msg: " << capabilities.toLocal8Bit().data();
      qWarning() << "Error: " << nova.readAllStandardError().constData();
      return;
    }
    QDomElement root = xml_doc.documentElement();
    if (root.tagName() != "capabilities") {
      qWarning() << "Invalid XML file for Nova search engine capabilities, msg: " << capabilities.toLocal8Bit().data();
      return;
    }
    for (QDomNode engine_node = root.firstChild(); !engine_node.isNull(); engine_node = engine_node.nextSibling()) {
      QDomElement engine_elem = engine_node.toElement();
      if (!engine_elem.isNull()) {
        SupportedEngine *s = new SupportedEngine(engine_elem);
        if (this->contains(s->getName())) {
          // Already in the list
          delete s;
        } else {
          qDebug("Supported search engine: %s", s->getFullName().toLocal8Bit().data());
          (*this)[s->getName()] = s;
          emit newSupportedEngine(s->getName());
        }
      }
    }
  }
};

#endif // SEARCHENGINES_H
