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
 * Contact : chris@qbittorrent.org
 */

#ifndef OSD_H
#define OSD_H

#include <QDialog>
#include <QLabel>
#include <QHBoxLayout>
#include <QTimer>

class OSD : public QDialog {
  Q_OBJECT

private:
  QTimer *hideOSDTimer;
  QHBoxLayout *hboxLayout;
  QLabel *icon;
  QLabel *msg;

public:
  OSD(QWidget *parent=0);
  ~OSD();

public slots:
  void display(const QString& message);
};

#endif
