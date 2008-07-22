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

#include "realprogressbarthread.h"
#include "realprogressbar.h"
#include <QtDebug>
#include <QMutexLocker>

RealProgressBarThread::RealProgressBarThread(RealProgressBar *pb, QTorrentHandle &handle) : QThread(pb), thandle(handle),  array(pb->width()){
	size = pb->width();
	abort = false;
	connect(pb, SIGNAL(widthChanged(int)), this, SLOT(resize(int)), Qt::DirectConnection);
	connect(this, SIGNAL(refreshed(QRealArray)), pb, SLOT(setProgress(QRealArray)));
}

RealProgressBarThread::~RealProgressBarThread(){
	qDebug("RealProgressBarThread destruction");
	resize(-1);
	qDebug("RealProgressBarThread waiting");
	wait();
	qDebug("RealProgressBarThread destroyed");
}

void RealProgressBarThread::resize(int width)
{
	QMutexLocker locker(&mutex);
	size = width;
	abort = true;
	condition.wakeOne();
}

void RealProgressBarThread::refresh()
{
	QMutexLocker locker(&mutex);
	condition.wakeOne();
}

void RealProgressBarThread::run(){
	forever
	{
		//start checking the torrent information
		if(ifInterrupted() == stop)
		{
			qDebug("RealProgressBarThread stop");
			return;
		}
		size_type total_size = thandle.total_size();
		size_type piece_length = thandle.piece_length();
		int num_pieces = thandle.num_pieces();
		const std::vector<bool>* pieces = thandle.pieces();
		//pieces not returned
		if (pieces == 0)
		{
			qDebug("pieces vector not returned");
			return;
		}
		//empty the array
		mutex.lock();
		for(int i=0; i<array.size(); i++)
			array[i] = 0.;
		mutex.unlock();
		qreal subfraction = array.size() / (qreal) total_size;
		qreal fraction = subfraction * piece_length;
		bool success = true;
		//fill the array with complete pieces
		for(int i=0; i<num_pieces; i++)
		{
			Interrupt temp = ifInterrupted();
			if (temp == stop)
			{
				qDebug("RealProgressBarThread stop");
				return;
			}
			if (temp == restart)
			{
				qDebug("RealProgressBarThread restart");
				success = false;
				break;
			}
			qreal start = i * fraction;
			qreal end = start + fraction;
			if((*pieces)[i])
				mark(start, end);
		}
		if (success)
		{
			/*
			qreal sum = 0.;
			mutex.lock();
			for(int i=0; i<array.size(); i++)
			sum += array[i];
			qDebug()<<"progress:"<<sum*100./array.size();
			mutex.unlock();*/
			qDebug("refreshed emmitted");
			emit refreshed(array);
			//wait for refresh or rezise slot
			mutex.lock();
			condition.wait(&mutex);
			mutex.unlock();
		}
	}
}

Interrupt RealProgressBarThread::ifInterrupted()
{
	QMutexLocker locker(&mutex);
	if(abort)
	{
		abort = false;
		if(size < 0)
			return stop;
		if(size != array.size())
		{
			array.resize(size);
			return restart;
		}
	}
	return ignore;
}

void RealProgressBarThread::mark(qreal start, qreal end, qreal progress){
	QMutexLocker locker(&mutex);
	if (end > array.size())
		end = array.size();
	int start_int, end_int;
	qreal start_frac, end_frac;
	double temp;
	start_frac = modf(start, &temp);
	start_int = (int) temp;
	end_frac = modf(end, &temp);
	end_int = (int) temp;
	if(start_int == end_int)
	{
		array[start_int] += progress * (end - start);
		return;
	}
	if (start_frac > 0.)
		array[start_int] += progress * (1 - start_frac);
	if (end_frac > 0.)
		array[end_int] += progress * end_frac;
	for(int i=start_int+1; i<end_int; i++)
		array[i] += progress;
}
