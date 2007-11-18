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

#ifndef REALPROGRESSBAR_H
#define REALPROGRESSBAR_H

#include "qrealarray.h"
#include <QWidget>
#include <QPixmap>

class RealProgressBar : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor)
	Q_PROPERTY(QColor foregroundColor READ foregroundColor WRITE setForegroundColor)

	private:
		QColor foreground;
		QColor background;
		bool active;
		QPixmap pixmap;
//		RealProgressBarThread *thread;
		QRealArray array;

	public:
		RealProgressBar(QWidget *parent = 0);
		~RealProgressBar();

		void setBackgroundColor(const QColor &newColor);
		QColor backgroundColor() const {return background;}
		void setForegroundColor(const QColor &newColor);
		QColor foregroundColor() const {return foreground;}
//		void setThread(const RealProgressBarThread *newThread);
//		RealProgressBarThread *thread() const {return thread;}

	public slots:
		void setProgress(QRealArray progress);

	signals:
		void widthChanged(int width);

	protected:
		void paintEvent(QPaintEvent *event);
		void resizeEvent(QResizeEvent *event);

	private:
		void drawPixmap();
		QColor penColor(qreal f);

};

#endif
