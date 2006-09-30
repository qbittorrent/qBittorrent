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

#include <QPixmap>
#include "OSD.h"

OSD::OSD(QWidget *parent) : QDialog(parent){
  // Hide OSD Timer
  hideOSDTimer = new QTimer(this);
  hideOSDTimer->setSingleShot(true);
  connect(hideOSDTimer, SIGNAL(timeout()), this, SLOT(hide()));
  // Window settings
  setWindowFlags(Qt::SplashScreen);
  setPalette(QPalette(QColor("darkBlue")));
  hboxLayout = new QHBoxLayout(this);
  icon = new QLabel(this);
  icon->setPixmap(QPixmap(":/Icons/qbittorrent16.png"));
  icon->adjustSize();
  msg = new QLabel(this);
  msg->setPalette(QPalette(QColor(88, 75, 255, 200)));
  icon->setPalette(QPalette(QColor(88, 75, 255, 200)));
  msg->setAutoFillBackground(true);
  icon->setAutoFillBackground(true);
  hboxLayout->addWidget(icon);
  hboxLayout->addWidget(msg);
  hboxLayout->setSpacing(0);
  hboxLayout->setMargin(1);
}

OSD::~OSD(){
  delete hideOSDTimer;
  delete icon;
  delete msg;
  delete hboxLayout;
}

void OSD::display(const QString& message){
  if(hideOSDTimer->isActive()){
    hideOSDTimer->stop();
    hide();
  }
  msg->setText("<font color='white'><b>"+message+"</b></font>");
  msg->adjustSize();
  adjustSize();
  show();
  hideOSDTimer->start(3000);
}
