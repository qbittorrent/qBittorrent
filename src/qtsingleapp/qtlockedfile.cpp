/****************************************************************************
** 
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
** 
** This file is part of a Qt Solutions component.
**
** Commercial Usage  
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Solutions Commercial License Agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and Nokia.
** 
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
** 
** In addition, as a special exception, Nokia gives you certain
** additional rights. These rights are described in the Nokia Qt LGPL
** Exception version 1.1, included in the file LGPL_EXCEPTION.txt in this
** package.
** 
** GNU General Public License Usage 
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
** 
** Please note Third Party Software included with Qt Solutions may impose
** additional restrictions and it is the user's responsibility to ensure
** that they have met the licensing requirements of the GPL, LGPL, or Qt
** Solutions Commercial license and the relevant license of the Third
** Party Software they are using.
** 
** If you are unsure which license is appropriate for your use, please
** contact Nokia at qt-info@nokia.com.
** 
****************************************************************************/

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
QtLockedFile::QtLockedFile()
    : QFile()
{
#ifdef Q_OS_WIN
    wmutex = 0;
    rmutex = 0;
#endif
    m_lock_mode = NoLock;
}

/*!
    Constructs an unlocked QtLockedFile object with file \a name. This
    constructor behaves in the same way as \e QFile::QFile(const
    QString&).

    \sa QFile::QFile()
*/
QtLockedFile::QtLockedFile(const QString &name)
    : QFile(name)
{
#ifdef Q_OS_WIN
    wmutex = 0;
    rmutex = 0;
#endif
    m_lock_mode = NoLock;
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
bool QtLockedFile::open(OpenMode mode)
{
    if (mode & QIODevice::Truncate) {
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
    return m_lock_mode != NoLock;
}

/*!
    Returns the type of lock currently held by this object, or \e
    QtLockedFile::NoLock.

    \sa isLocked()
*/
QtLockedFile::LockMode QtLockedFile::lockMode() const
{
    return m_lock_mode;
}

/*!
    \fn bool QtLockedFile::lock(LockMode mode, bool block = true)

    Obtains a lock of type \a mode. The file must be opened before it
    can be locked.

    If \a block is true, this function will block until the lock is
    aquired. If \a block is false, this function returns \e false
    immediately if the lock cannot be aquired.

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
