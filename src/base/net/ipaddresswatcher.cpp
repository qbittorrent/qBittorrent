/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  WordlessEcho <wordless@echo.moe>
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
 */

#include "ipaddresswatcher.h"

#include <chrono>

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <array>
#include <cerrno>
#include <cstring>
#endif

#if defined(Q_OS_LINUX)
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <unistd.h>
#elif defined(Q_OS_MACOS)
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/kern_event.h>
#include <sys/socket.h>
#include <sys/sys_domain.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#endif

#include <QMetaObject>
#include <QTimer>

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <QSocketNotifier>
#endif

#include "base/logger.h"

using namespace Net;

namespace
{
    // OS events usually come in bursts (e.g. an address is added and an old one is deprecated)
    const std::chrono::seconds DEBOUNCE_INTERVAL {1};

#if defined(Q_OS_LINUX)
    // Returns -1 and sets `errno` on failure
    int openEventSocket()
    {
        const int fd = ::socket(AF_NETLINK, (SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK), NETLINK_ROUTE);
        if (fd < 0)
            return -1;

        sockaddr_nl addr {};
        addr.nl_family = AF_NETLINK;
        addr.nl_groups = (RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR);
        if (::bind(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) < 0)
        {
            const int errorCode = errno;
            ::close(fd);
            errno = errorCode;
            return -1;
        }

        return fd;
    }
#elif defined(Q_OS_MACOS)
    // Returns -1 and sets `errno` on failure
    int openEventSocket()
    {
        // Kernel events of the network class include the address changes (KEV_INET_* and KEV_INET6_*)
        const int fd = ::socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT);
        if (fd < 0)
            return -1;

        kev_request request {};
        request.vendor_code = KEV_VENDOR_APPLE;
        request.kev_class = KEV_NETWORK_CLASS;
        request.kev_subclass = KEV_ANY_SUBCLASS;
        const int statusFlags = ::fcntl(fd, F_GETFL);
        if ((::ioctl(fd, SIOCSKEVFILT, &request) < 0)
            || (statusFlags < 0) || (::fcntl(fd, F_SETFL, (statusFlags | O_NONBLOCK)) < 0)
            || (::fcntl(fd, F_SETFD, FD_CLOEXEC) < 0))
        {
            const int errorCode = errno;
            ::close(fd);
            errno = errorCode;
            return -1;
        }

        return fd;
    }
#elif defined(Q_OS_WIN)
    // Runs on a thread pool thread of Windows. Do no work here besides notifying the main thread.
    void WINAPI onUnicastIPAddressChanged(void *context, MIB_UNICASTIPADDRESS_ROW *, MIB_NOTIFICATION_TYPE)
    {
        auto *timer = static_cast<QTimer *>(context);
        QMetaObject::invokeMethod(timer, [timer] { timer->start(); }, Qt::QueuedConnection);
    }
#endif
}

IPAddressWatcher::IPAddressWatcher(QObject *parent)
    : QObject(parent)
    , m_debounceTimer {new QTimer(this)}
{
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(DEBOUNCE_INTERVAL);
    connect(m_debounceTimer, &QTimer::timeout, this, &IPAddressWatcher::addressesChanged);

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    m_eventSocket = openEventSocket();
    if (m_eventSocket < 0)
    {
        const int errorCode = errno;
        LogMsg(tr("Failed to watch IP address changes. Reason: \"%1\". Error code: %2.")
            .arg(QString::fromLocal8Bit(std::strerror(errorCode)), QString::number(errorCode)), Log::WARNING);
        return;
    }

    m_socketNotifier = new QSocketNotifier(m_eventSocket, QSocketNotifier::Read, this);
    connect(m_socketNotifier, &QSocketNotifier::activated, this, &IPAddressWatcher::readEventSocket);
#elif defined(Q_OS_WIN)
    HANDLE handle = nullptr;
    const DWORD result = ::NotifyUnicastIpAddressChange(AF_UNSPEC, onUnicastIPAddressChanged
        , m_debounceTimer, FALSE, &handle);
    if (result != NO_ERROR)
    {
        LogMsg(tr("Failed to watch IP address changes. Error code: %1.").arg(QString::number(result)), Log::WARNING);
        return;
    }

    m_notificationHandle = handle;
#endif
}

IPAddressWatcher::~IPAddressWatcher()
{
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    delete m_socketNotifier;
    if (m_eventSocket >= 0)
        ::close(m_eventSocket);
#elif defined(Q_OS_WIN)
    // Must be done before `m_debounceTimer` is destroyed since the callback uses it.
    // It waits for the running callbacks to complete.
    if (m_notificationHandle)
        ::CancelMibChangeNotify2(static_cast<HANDLE>(m_notificationHandle));
#endif
}

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
void IPAddressWatcher::readEventSocket()
{
    // The content of the messages is not needed, only the fact that something changed.
    // A failure such as ENOBUFS (some messages were dropped) also means that something changed.
    std::array<char, 8192> buffer {};
    while (true)
    {
        const ssize_t received = ::recv(m_eventSocket, buffer.data(), buffer.size(), 0);
        if (received <= 0)
            break;
    }

    m_debounceTimer->start();
}
#endif
