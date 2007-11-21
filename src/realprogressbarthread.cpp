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

//this is called inside run()
#define CHECK_INTERRUPT \
	switch(ifInterrupted())\
	{\
		case stop:\
			qDebug("RealProgressBarThread stop");\
			return;\
		case restart:\
			qDebug("RealProgressBarThread restart");\
			goto start;\
		case ignore:\
			break;\
	}

void RealProgressBarThread::run(){
	//wait for refresh or rezise slot
wait:
	QMutexLocker locker(&mutex);
	condition.wait(&mutex);
	locker.unlock();
	
	//start checking the torrent information
start:
	CHECK_INTERRUPT
	size_type total_size = thandle.total_size();
	size_type piece_length = thandle.piece_length();
	int num_pieces = thandle.num_pieces();
	const std::vector<bool>* pieces = thandle.pieces();
	//no vector returned
	if (pieces == 0)
		return;
	//empty the array
	locker.relock();
	for(int i=0; i<size; i++)
		array[i] = 0.;
	locker.unlock();
	qreal subfraction = size / (qreal) total_size;
	qreal fraction = subfraction * piece_length;
	//fill the array with complete pieces
	for(int i=0; i<num_pieces; i++)
	{
		CHECK_INTERRUPT
		qreal start = i * fraction;
		qreal end = start + fraction;
		if((*pieces)[i])
			mark(start, end);
	}
/*
	//fill the array with incomplete pieces (from download queue)
	std::vector<partial_piece_info> queue;
	thandle.get_download_queue(queue);
	for(unsigned int i=0; i<queue.size(); i++)
	{
		partial_piece_info ppi = queue[i];
		qreal start = ppi.piece_index * fraction;
		qreal end = start;
		for(int j=0; j<ppi.blocks_in_piece; j++)
		{
			CHECK_INTERRUPT
			block_info bi = ppi.blocks[j];
			end += bi.block_size * subfraction;
			qreal progress = bi.bytes_progress / (qreal) bi.block_size;
			mark(start, end, progress);
			start = end;
		}
	}
 	qreal sum = 0.;
 	locker.relock();
	 	for(int i=0; i<size; i++)
 	sum += array[i];
 	qDebug()<<"progress:"<<sum*100./size;
 	locker.unlock();*/
	qDebug("refreshed emmitted");
	emit refreshed(array);
	goto wait;
}

//this is called by CHECK_INTERRUPT
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
	if (end > size)
		end = size;
	int start_int, end_int;
	qreal temp, start_frac, end_frac;
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
