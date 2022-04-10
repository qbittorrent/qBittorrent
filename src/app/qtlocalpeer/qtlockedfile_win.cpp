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

#include <QFileInfo>

#include "base/global.h"

// Maximum number of concurrent read locks. Must not be greater than MAXIMUM_WAIT_OBJECTS
const int MAX_READERS = MAXIMUM_WAIT_OBJECTS;

Qt::HANDLE QtLockedFile::getMutexHandle(const int idx, const bool doCreate)
{
    if (m_mutexName.isEmpty())
    {
        QFileInfo fi(*this);
        m_mutexName = u"QtLockedFile mutex " + fi.absoluteFilePath().toLower();
    }

    QString mname = m_mutexName;
    if (idx >= 0)
        mname += QString::number(idx);

    const std::wstring mnameWStr = mname.toStdWString();

    if (doCreate)
    {
        const Qt::HANDLE mutex = ::CreateMutexW(NULL, FALSE, mnameWStr.c_str());
        if (!mutex)
        {
            qErrnoWarning("QtLockedFile::lock(): CreateMutex failed");
            return nullptr;
        }

        return mutex;
    }
    else
    {
        const Qt::HANDLE mutex = ::OpenMutexW((SYNCHRONIZE | MUTEX_MODIFY_STATE), FALSE, mnameWStr.c_str());
        if (!mutex)
        {
            if (GetLastError() != ERROR_FILE_NOT_FOUND)
                qErrnoWarning("QtLockedFile::lock(): OpenMutex failed");
            return nullptr;
        }

        return mutex;
    }

    return nullptr;
}

bool QtLockedFile::waitMutex(const Qt::HANDLE mutex, const bool doBlock) const
{
    Q_ASSERT(mutex);

    const DWORD res = ::WaitForSingleObject(mutex, (doBlock ? INFINITE : 0));
    switch (res)
    {
    case WAIT_OBJECT_0:
    case WAIT_ABANDONED:
        return true;
    case WAIT_TIMEOUT:
        return false;
    default:
        qErrnoWarning("QtLockedFile::lock(): WaitForSingleObject failed");
        break;
    }
    return false;
}

bool QtLockedFile::lock(const LockMode mode, const bool block)
{
    if (!isOpen())
    {
        qWarning("QtLockedFile::lock(): file is not opened");
        return false;
    }

    if (mode == NoLock)
        return unlock();

    if (mode == m_lockMode)
        return true;

    if (m_lockMode != NoLock)
        unlock();

    if (!m_writeMutex && !(m_writeMutex = getMutexHandle(-1, true)))
        return false;

    if (!waitMutex(m_writeMutex, block))
        return false;

    if (mode == ReadLock)
    {
        int idx = 0;
        for (; idx < MAX_READERS; ++idx)
        {
            m_readMutex = getMutexHandle(idx, false);
            if (!m_readMutex || waitMutex(m_readMutex, false))
                break;
            ::CloseHandle(m_readMutex);
        }

        bool ok = true;
        if (idx >= MAX_READERS)
        {
            qWarning("QtLockedFile::lock(): too many readers");
            m_readMutex = nullptr;
            ok = false;
        }
        else if (!m_readMutex)
        {
            m_readMutex = getMutexHandle(idx, true);
            if (!m_readMutex || !waitMutex(m_readMutex, false))
                ok = false;
        }

        if (!ok && m_readMutex)
        {
            ::CloseHandle(m_readMutex);
            m_readMutex = nullptr;
        }

        ::ReleaseMutex(m_writeMutex);
        if (!ok)
            return false;
    }
    else
    {
        Q_ASSERT(m_readMutexes.isEmpty());
        for (int i = 0; i < MAX_READERS; ++i)
        {
            const Qt::HANDLE mutex = getMutexHandle(i, false);
            if (mutex)
                m_readMutexes.append(mutex);
        }
        if (m_readMutexes.size())
        {
            const DWORD res = ::WaitForMultipleObjects(m_readMutexes.size(), m_readMutexes.constData(),
                TRUE, (block ? INFINITE : 0));
            if ((res != WAIT_OBJECT_0) && (res != WAIT_ABANDONED))
            {
                if (res != WAIT_TIMEOUT)
                    qErrnoWarning("QtLockedFile::lock(): WaitForMultipleObjects failed");
                m_lockMode = WriteLock;  // trick unlock() to clean up - semiyucky
                unlock();
                return false;
            }
        }
    }

    m_lockMode = mode;
    return true;
}

bool QtLockedFile::unlock()
{
    if (!isOpen())
    {
        qWarning("QtLockedFile::unlock(): file is not opened");
        return false;
    }

    if (!isLocked())
        return true;

    if (m_lockMode == ReadLock)
    {
        ::ReleaseMutex(m_readMutex);
        ::CloseHandle(m_readMutex);
        m_readMutex = nullptr;
    }
    else
    {
        for (const Qt::HANDLE &mutex : asConst(m_readMutexes))
        {
            ::ReleaseMutex(mutex);
            ::CloseHandle(mutex);
        }
        m_readMutexes.clear();
        ::ReleaseMutex(m_writeMutex);
    }

    m_lockMode = QtLockedFile::NoLock;
    return true;
}

QtLockedFile::~QtLockedFile()
{
    if (isOpen())
        unlock();
    if (m_writeMutex)
        ::CloseHandle(m_writeMutex);
}
