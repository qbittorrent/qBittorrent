/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez, Arnaud Demaiziere
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
 * Contact : chris@qbittorrent.org, arnaud@qbittorrent.org
 */

#include "realprogressbar.h"
#include <QPainter>
#include <QtDebug>
#include <QResizeEvent>

RealProgressBar::RealProgressBar(QWidget *parent)
	: QWidget(parent), array(1)
{
	background = Qt::white;
	foreground = Qt::black;
	active = false;
	array[0] = 0.;
	drawPixmap();
}

RealProgressBar::~RealProgressBar()
{
	qDebug("RealProgressBar destruction");
}

void RealProgressBar::setBackgroundColor(const QColor &newColor)
{
	background = newColor;
	drawPixmap();
}

void RealProgressBar::setForegroundColor(const QColor &newColor)
{
	foreground = newColor;
	drawPixmap();
}
/*
void RealProgressBar::setThread(const RealProgressBarThread *newThread)
{
	thread = newThread;
}
*/
void RealProgressBar::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	painter.drawPixmap(rect(), pixmap);
}

void RealProgressBar::resizeEvent(QResizeEvent *event)
{
	if(width() != event->oldSize().width())
		emit widthChanged(width());
}

void RealProgressBar::setProgress(QRealArray progress)
{
	qDebug("setProgress called");
	array = progress;
	drawPixmap();
}

void RealProgressBar::drawPixmap()
{
	if(pixmap.width() != array.size())
		pixmap = QPixmap(array.size(), 1);
	QPainter painter(&pixmap);
	for(int i=0; i<array.size(); i++)
	{
		painter.setPen(penColor(array[i]));
		painter.drawPoint(i,0);
	}
	update();
}

QColor RealProgressBar::penColor(qreal x)
{
	qreal y = 1 - x;
	Q_ASSERT(x >= 0.);
	Q_ASSERT(y >= 0.);
	qreal r1, g1, b1, a1, r2, g2, b2, a2;
	foreground.getRgbF(&r1, &g1, &b1, &a1);
	background.getRgbF(&r2, &g2, &b2, &a2);
	QColor color;
	color.setRgbF(x*r1+y*r2, x*g1+y*g2, x*b1+y*b2, x*a1+y*a2);
	return color;
}
