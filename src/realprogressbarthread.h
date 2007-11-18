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

#ifndef REALPROGRESSBARTHREAD_H
#define REALPROGRESSBARTHREAD_H

#include "qtorrenthandle.h"
#include "qrealarray.h"

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

class RealProgressBar;

enum Interrupt
{
	ignore,
	restart,
	stop
};

class RealProgressBarThread : public QThread {
	Q_OBJECT
	private:
		bool abort;
		int size;
		QTorrentHandle thandle;
		QMutex mutex;
		QWaitCondition condition;
		QRealArray array;

	public:
		RealProgressBarThread(RealProgressBar *pb, QTorrentHandle &handle);
		~RealProgressBarThread();

	public slots:
		void resize(int width);
		void refresh();

	signals:
		void refreshed(QRealArray progress);

	protected:
		void run();

	private:
		Interrupt ifInterrupted();
		void mark(qreal start, qreal end, qreal progress = 1.);
};

#endif
