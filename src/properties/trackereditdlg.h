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

#ifndef TRACKEREDITDLG_H
#define TRACKEREDITDLG_H

#include <QDialog>
#include <Qstring>
#include "ui_trackereditdlg.h"
#include "qtorrenthandle.h"
#include "misc.h"

using namespace libtorrent;

class TrackerEditDlg : public QDialog, private Ui::TrackerEditDlg {
  Q_OBJECT

private:
  QTorrentHandle h;
  QString m_tracker_URI;

public:
  TrackerEditDlg(QTorrentHandle h, QString tracker_URI, QWidget *parent=0) : QDialog(parent), h(h) {
    setupUi(this);
    this->m_tracker_URI = tracker_URI.trimmed();
    this->tracker_URI_edit->setText(m_tracker_URI);
    connect(this->button_OK,SIGNAL(clicked()),this,SLOT(TrackerEditDlg::on_button_OK_clicked()));
    connect(this->button_Cancel,SIGNAL(clicked()),this,SLOT(TrackerEditDlg::on_button_Cancel_clicked()));
  }

  ~TrackerEditDlg() {}

public slots:
  void on_button_OK_clicked() {
    if (replaceTracker())
      emit TrackerEditDlg::accept();
    else
      emit TrackerEditDlg::reject();
  }

  void on_button_Cancel_clicked() {
    emit TrackerEditDlg::reject();
  }

private:
  bool replaceTracker() {
    if (!h.is_valid()) return false;

    QString new_URI = this->tracker_URI_edit->text().trimmed();
    if (new_URI.isEmpty()
        || new_URI == this->m_tracker_URI) return false;

    std::vector<announce_entry> trackers = h.trackers();
    if (trackers.empty()) return false;

    std::vector<announce_entry> new_trackers;

    std::vector<announce_entry>::iterator it = trackers.begin();
    std::vector<announce_entry>::iterator itend = trackers.end();
    bool match = false;

    for ( ; it != itend; ++it) {
      if (new_URI == misc::toQString((*it).url))
        return false; // Tracker already exists; silently ignoring

      announce_entry URI(*it);

      if (this->m_tracker_URI == misc::toQString((*it).url) && !match) {
        announce_entry new_URI(new_URI.toStdString());
        URI = new_URI;
        URI.tier = (*it).tier;
        match = true;
      }

      new_trackers.push_back(URI);
    }
    if (match == false) return false; // Found no tracker to replace

    h.replace_trackers(new_trackers);
    return true;
  }

public:
  static bool editSelectedTracker(QTorrentHandle h, QString tracker_URI) {
    TrackerEditDlg dlg(h,tracker_URI);
    if (dlg.exec() != QDialog::Accepted) {
        return false;
    }
    return true;
  }

};

#endif
