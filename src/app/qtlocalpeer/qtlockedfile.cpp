/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Mike Tzou (Chocobo1)
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
****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Solutions component.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************
*/

#include "qtlockedfile.h"

/*!
    \class QtLockedFile

    \brief The QtLockedFile class extends QFile with advisory locking
    functions.

    A file may be locked in read or write mode. Multiple instances of
    \e QtLockedFile, created in multiple processes running on the same
    machine, may have a file locked in read mode. Exactly one instance
    may have it locked in write mode. A read and a write lock cannot
    exist simultaneously on the same file.

    The file locks are advisory. This means that nothing prevents
    another process from manipulating a locked file using QFile or
    file system functions offered by the OS. Serialization is only
    guaranteed if all processes that access the file use
    QLockedFile. Also, while holding a lock on a file, a process
    must not open the same file again (through any API), or locks
    can be unexpectedly lost.

    The lock provided by an instance of \e QtLockedFile is released
    whenever the program terminates. This is true even when the
    program crashes and no destructors are called.
*/

/*! \enum QtLockedFile::LockMode

    This enum describes the available lock modes.

    \value ReadLock A read lock.
    \value WriteLock A write lock.
    \value NoLock Neither a read lock nor a write lock.
*/

/*!
    Constructs an unlocked \e QtLockedFile object. This constructor
    behaves in the same way as \e QFile::QFile().

    \sa QFile::QFile()
*/
QtLockedFile::QtLockedFile() = default;

/*!
    Constructs an unlocked QtLockedFile object with file \a name. This
    constructor behaves in the same way as \e QFile::QFile(const
    QString&).

    \sa QFile::QFile()
*/
QtLockedFile::QtLockedFile(const QString &name)
    : QFile(name)
{
}

/*!
  Opens the file in OpenMode \a mode.

  This is identical to QFile::open(), with the one exception that the
  Truncate mode flag is disallowed. Truncation would conflict with the
  advisory file locking, since the file would be modified before the
  write lock is obtained. If truncation is required, use resize(0)
  after obtaining the write lock.

  Returns true if successful; otherwise false.

  \sa QFile::open(), QFile::resize()
*/
bool QtLockedFile::open(const OpenMode mode)
{
    if (mode & QIODevice::Truncate)
    {
        qWarning("QtLockedFile::open(): Truncate mode not allowed.");
        return false;
    }
    return QFile::open(mode);
}

/*!
    Returns \e true if this object has a in read or write lock;
    otherwise returns \e false.

    \sa lockMode()
*/
bool QtLockedFile::isLocked() const
{
    return m_lockMode != NoLock;
}

/*!
    Returns the type of lock currently held by this object, or \e
    QtLockedFile::NoLock.

    \sa isLocked()
*/
QtLockedFile::LockMode QtLockedFile::lockMode() const
{
    return m_lockMode;
}

/*!
    \fn bool QtLockedFile::lock(LockMode mode, bool block = true)

    Obtains a lock of type \a mode. The file must be opened before it
    can be locked.

    If \a block is true, this function will block until the lock is
    acquired. If \a block is false, this function returns \e false
    immediately if the lock cannot be acquired.

    If this object already has a lock of type \a mode, this function
    returns \e true immediately. If this object has a lock of a
    different type than \a mode, the lock is first released and then a
    new lock is obtained.

    This function returns \e true if, after it executes, the file is
    locked by this object, and \e false otherwise.

    \sa unlock(), isLocked(), lockMode()
*/

/*!
    \fn bool QtLockedFile::unlock()

    Releases a lock.

    If the object has no lock, this function returns immediately.

    This function returns \e true if, after it executes, the file is
    not locked by this object, and \e false otherwise.

    \sa lock(), isLocked(), lockMode()
*/

/*!
    \fn QtLockedFile::~QtLockedFile()

    Destroys the \e QtLockedFile object. If any locks were held, they
    are released.
*/
