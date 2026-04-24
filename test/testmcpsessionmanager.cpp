/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Ortes <malo.allee@gmail.com>
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

#include <QObject>
#include <QTest>
#include <QHostAddress>

#include "base/global.h"
#include "webui/mcp/mcpsessionmanager.h"

class TestMCPSessionManager final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestMCPSessionManager)

public:
    TestMCPSessionManager() = default;

private slots:
    void create_returnsUniqueId() const
    {
        MCP::SessionManager mgr;
        const QString id1 = mgr.create(QHostAddress::LocalHost, u"2025-06-18"_s);
        const QString id2 = mgr.create(QHostAddress::LocalHost, u"2025-06-18"_s);
        QVERIFY(!id1.isEmpty());
        QVERIFY(!id2.isEmpty());
        QCOMPARE_NE(id1, id2);
    }

    void find_returnsNullOnUnknownId() const
    {
        MCP::SessionManager mgr;
        QVERIFY(!mgr.find(u"nonexistent"_s).has_value());
    }

    void find_returnsSessionOnKnownId() const
    {
        MCP::SessionManager mgr;
        const QString id = mgr.create(QHostAddress::LocalHost, u"2025-06-18"_s);
        const auto session = mgr.find(id);
        QVERIFY(session.has_value());
        QCOMPARE(session->id, id);
    }

    void remove_removesSession() const
    {
        MCP::SessionManager mgr;
        const QString id = mgr.create(QHostAddress::LocalHost, u"2025-06-18"_s);
        mgr.remove(id);
        QVERIFY(!mgr.find(id).has_value());
    }

    void capPerIp_rejectsEleventhSession() const
    {
        MCP::SessionManager mgr;
        for (int i = 0; i < 10; ++i)
            QVERIFY(!mgr.create(QHostAddress::LocalHost, u"2025-06-18"_s).isEmpty());
        QVERIFY(mgr.create(QHostAddress::LocalHost, u"2025-06-18"_s).isEmpty());
    }

    void capPerIp_separateIpsDoNotConflict() const
    {
        MCP::SessionManager mgr;
        for (int i = 0; i < 10; ++i)
            QVERIFY(!mgr.create(QHostAddress::LocalHost, u"2025-06-18"_s).isEmpty());
        QVERIFY(!mgr.create(QHostAddress(u"192.168.1.1"_s), u"2025-06-18"_s).isEmpty());
    }

    void remove_decrementsPerIpCounter() const
    {
        MCP::SessionManager mgr;
        QList<QString> ids;
        for (int i = 0; i < 10; ++i)
            ids.append(mgr.create(QHostAddress::LocalHost, u"2025-06-18"_s));
        // Cap reached, removing one should free a slot
        mgr.remove(ids.first());
        QVERIFY(!mgr.create(QHostAddress::LocalHost, u"2025-06-18"_s).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestMCPSessionManager)
#include "testmcpsessionmanager.moc"
