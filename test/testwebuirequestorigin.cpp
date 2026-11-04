/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Thomas Piccirello <thomas@piccirello.com>
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
#include <QString>
#include <QTest>
#include <QUrl>

#include "base/http/constants.h"
#include "base/http/request.h"
#include "webui/requestorigin.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    using RequestOrigin::Header;
    using RequestOrigin::Type;

    const QString TARGET_ORIGIN = u"qbt.example:8080"_s;

    RequestOrigin::Result classify(const QString &secFetchSite, const QString &origin, const QString &referer
            , const QString &targetOrigin = TARGET_ORIGIN)
    {
        Http::Request request;
        if (!secFetchSite.isEmpty())
            request.headers[Http::HEADER_SEC_FETCH_SITE] = secFetchSite;
        if (!origin.isEmpty())
            request.headers[Http::HEADER_ORIGIN] = origin;
        if (!referer.isEmpty())
            request.headers[Http::HEADER_REFERER] = referer;
        return RequestOrigin::classify(request, targetOrigin);
    }
}

class TestWebUIRequestOrigin final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestWebUIRequestOrigin)

public:
    TestWebUIRequestOrigin() = default;

private slots:
    void testNoHeaders() const
    {
        const RequestOrigin::Result result = classify({}, {}, {});
        QCOMPARE(result.type, Type::Unknown);
        QCOMPARE(result.header, Header::None);
        QCOMPARE(result.headerValue, QString());
    }

    void testSecFetchSite() const
    {
        const RequestOrigin::Result sameOrigin = classify(u"same-origin"_s, {}, {});
        QCOMPARE(sameOrigin.type, Type::SameOrigin);
        QCOMPARE(sameOrigin.header, Header::SecFetchSite);
        QCOMPARE(sameOrigin.headerValue, u"same-origin"_s);

        QCOMPARE(classify(u"Same-Origin"_s, {}, {}).type, Type::SameOrigin);
        QCOMPARE(classify(u"none"_s, {}, {}).type, Type::UserInitiated);
        QCOMPARE(classify(u"same-site"_s, {}, {}).type, Type::CrossOrigin);
        QCOMPARE(classify(u"cross-site"_s, {}, {}).type, Type::CrossOrigin);
        QCOMPARE(classify(u"bogus"_s, {}, {}).type, Type::CrossOrigin);

        // takes precedence over 'Origin' and 'Referer'
        const RequestOrigin::Result crossSite = classify(u"cross-site"_s, u"http://qbt.example:8080"_s, u"http://qbt.example:8080/"_s);
        QCOMPARE(crossSite.type, Type::CrossOrigin);
        QCOMPARE(crossSite.header, Header::SecFetchSite);
        QCOMPARE(classify(u"same-origin"_s, u"http://evil.example"_s, u"http://evil.example/"_s).type, Type::SameOrigin);
    }

    void testOrigin() const
    {
        const RequestOrigin::Result sameOrigin = classify({}, u"http://qbt.example:8080"_s, {});
        QCOMPARE(sameOrigin.type, Type::SameOrigin);
        QCOMPARE(sameOrigin.header, Header::Origin);
        QCOMPARE(sameOrigin.headerValue, u"http://qbt.example:8080"_s);

        // scheme isn't part of the target origin
        QCOMPARE(classify({}, u"https://qbt.example:8080"_s, {}).type, Type::SameOrigin);

        QCOMPARE(classify({}, u"http://evil.example:8080"_s, {}).type, Type::CrossOrigin);
        QCOMPARE(classify({}, u"http://qbt.example.evil.example:8080"_s, {}).type, Type::CrossOrigin);
        QCOMPARE(classify({}, u"http://qbt.example:9090"_s, {}).type, Type::CrossOrigin);
        QCOMPARE(classify({}, u"http://qbt.example"_s, {}).type, Type::CrossOrigin);
        QCOMPARE(classify({}, u"null"_s, {}).type, Type::CrossOrigin);

        // takes precedence over 'Referer'
        const RequestOrigin::Result crossOrigin = classify({}, u"http://evil.example"_s, u"http://qbt.example:8080/"_s);
        QCOMPARE(crossOrigin.type, Type::CrossOrigin);
        QCOMPARE(crossOrigin.header, Header::Origin);
        QCOMPARE(classify({}, u"http://qbt.example:8080"_s, u"http://evil.example/"_s).type, Type::SameOrigin);
    }

    void testReferer() const
    {
        const RequestOrigin::Result sameOrigin = classify({}, {}, u"http://qbt.example:8080/index.html"_s);
        QCOMPARE(sameOrigin.type, Type::SameOrigin);
        QCOMPARE(sameOrigin.header, Header::Referer);
        QCOMPARE(sameOrigin.headerValue, u"http://qbt.example:8080/index.html"_s);

        QCOMPARE(classify({}, {}, u"http://evil.example:8080/"_s).type, Type::CrossOrigin);
        QCOMPARE(classify({}, {}, u"http://qbt.example:9090/"_s).type, Type::CrossOrigin);
    }

    void testTargetOrigin() const
    {
        QCOMPARE(classify({}, u"http://qbt.example"_s, {}, u"qbt.example"_s).type, Type::SameOrigin);
        QCOMPARE(classify({}, u"https://qbt.example:8080"_s, {}, u"https://qbt.example:8080"_s).type, Type::SameOrigin);
        QCOMPARE(classify({}, u"http://[::1]:8080"_s, {}, u"[::1]:8080"_s).type, Type::SameOrigin);
        QCOMPARE(classify({}, u"http://qbt.example:8080"_s, {}, {}).type, Type::CrossOrigin);
    }

    void testUrlFromHostHeader() const
    {
        const QUrl hostOnly = RequestOrigin::urlFromHostHeader(u"qbt.example:8080"_s);
        QCOMPARE(hostOnly.scheme(), u"http"_s);
        QCOMPARE(hostOnly.host(), u"qbt.example"_s);
        QCOMPARE(hostOnly.port(), 8080);

        const QUrl withScheme = RequestOrigin::urlFromHostHeader(u"https://qbt.example"_s);
        QCOMPARE(withScheme.scheme(), u"https"_s);
        QCOMPARE(withScheme.host(), u"qbt.example"_s);
        QCOMPARE(withScheme.port(), -1);

        QCOMPARE(RequestOrigin::urlFromHostHeader(u"[::1]:8080"_s).host(), u"::1"_s);
    }
};

QTEST_APPLESS_MAIN(TestWebUIRequestOrigin)
#include "testwebuirequestorigin.moc"
