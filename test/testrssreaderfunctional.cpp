/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026 The qBittorrent project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later.
 */

#include <algorithm>

#include <QAbstractTextDocumentLayout>
#include <QBuffer>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#include "base/global.h"
#include "base/logger.h"
#include "base/path.h"
#include "base/profile.h"
#include "gui/rss/htmlbrowser.h"

class TestRSSReaderFunctional final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestRSSReaderFunctional)

public:
    TestRSSReaderFunctional() = default;

private slots:
    void initTestCase()
    {
        QVERIFY(m_profileDir.isValid());
        Logger::initInstance();
        Profile::initInstance(Path(m_profileDir.path()), u"rss-reader-test"_s, false);
    }

    void cleanupTestCase()
    {
        Profile::freeInstance();
        Logger::freeInstance();
    }

    void resolvesHtmlImageUrls()
    {
        HtmlBrowser browser;
        const QUrl base {u"https://example.invalid:8443/news/item.html"_s};
        browser.setHtml(u"<IMG SRC='pic/a.jpg'><img src=\"/root.jpg\"><img src=../parent.jpg>"
            u"<img src='//cdn.invalid/a.jpg'><img src='https://other.invalid/b.jpg'>"_s, base);
        browser.resize(600, 400);
        browser.show();
        browser.document()->documentLayout()->documentSize();
        const QList<QUrl> expected {
            QUrl(u"https://example.invalid:8443/news/pic/a.jpg"_s),
            QUrl(u"https://example.invalid:8443/root.jpg"_s),
            QUrl(u"https://example.invalid:8443/parent.jpg"_s),
            QUrl(u"https://cdn.invalid/a.jpg"_s),
            QUrl(u"https://other.invalid/b.jpg"_s)};
        const auto replies = browser.findChildren<QNetworkReply *>();
        QCOMPARE(replies.size(), expected.size());
        for (const auto &url : expected)
        {
            QVERIFY(std::any_of(replies.cbegin(), replies.cend(), [&url](const QNetworkReply *reply)
            {
                return reply->request().url() == url;
            }));
        }
        QCOMPARE(browser.document()->baseUrl(), base);
        browser.setHtml(u"<p>Next article</p>"_s);
        QVERIFY(browser.document()->baseUrl().isEmpty());
    }

    void imageResourceLifecycle_data()
    {
        QTest::addColumn<bool>("validImage");
        QTest::newRow("empty-response") << false;
        QTest::newRow("valid-no-store-image") << true;
    }

    void imageResourceLifecycle()
    {
        QFETCH(bool, validImage);
        QByteArray body;
        QImage expectedImage {16, 16, QImage::Format_ARGB32};
        expectedImage.fill(Qt::red);
        if (validImage)
        {
            QBuffer buffer {&body};
            QVERIFY(buffer.open(QIODevice::WriteOnly));
            QVERIFY(expectedImage.save(&buffer, "PNG"));
        }
        const QByteArray response = "HTTP/1.1 200 OK\r\nCache-Control: no-store\r\nContent-Length: "
            + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        QTcpServer server;
        QVERIFY2(server.listen(QHostAddress::LocalHost), qPrintable(server.errorString()));
        int requests = 0;
        QList<QTcpSocket *> heldSockets;
        connect(&server, &QTcpServer::newConnection, this, [&]
        {
            while (server.hasPendingConnections())
            {
                auto *socket = server.nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, socket, [&, socket]
                {
                    const QByteArray request = socket->readAll();
                    ++requests;
                    if (request.contains("/held"))
                    {
                        heldSockets.append(socket);
                        return;
                    }

                    socket->write(response);
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        HtmlBrowser browser;
        auto *manager = browser.findChild<QNetworkAccessManager *>();
        QVERIFY(manager);
        QSignalSpy finished {manager, &QNetworkAccessManager::finished};
        const QUrl imageUrl {u"http://127.0.0.1:%1/image"_s.arg(server.serverPort())};
        const QString html = u"<img src='%1'>"_s.arg(imageUrl.toString());
        browser.resize(400, 300);
        browser.show();
        browser.setHtml(html);
        QTRY_COMPARE(finished.count(), 1);
        QTRY_COMPARE(manager->findChildren<QNetworkReply *>().size(), 0);
        const QVariant resource = browser.document()->resource(QTextDocument::ImageResource, imageUrl);
        QVERIFY(resource.isValid());
        if (validImage)
        {
            QCOMPARE(resource.value<QImage>().pixelColor(0, 0), QColor(Qt::red));
        }
        else
        {
            QCOMPARE(resource.metaType(), QMetaType::fromType<QByteArray>());
            QVERIFY(resource.toByteArray().isEmpty());
        }
        for (int i = 0; i < 3; ++i)
        {
            browser.document()->markContentsDirty(0, browser.document()->characterCount());
            browser.viewport()->update();
            QTest::qWait(50);
        }
        QCOMPARE(requests, 1);

        browser.setHtml(html);
        QTRY_COMPARE(finished.count(), 2);
        QTRY_COMPARE(manager->findChildren<QNetworkReply *>().size(), 0);
        QCOMPARE(requests, 2);

        // Keep a response pending, switch articles, then attempt late delivery.
        browser.setHtml(u"<img src='http://127.0.0.1:%1/held'>"_s.arg(server.serverPort()));
        QTRY_COMPARE(requests, 3);
        QVERIFY(!heldSockets.isEmpty());
        QSignalSpy destroyed {manager->findChildren<QNetworkReply *>().last(), &QObject::destroyed};
        browser.setHtml(u"<p>Next article</p>"_s);
        QTRY_COMPARE(destroyed.count(), 1);
        for (auto *socket : heldSockets)
        {
            if (server.children().contains(socket) && socket->state() == QAbstractSocket::ConnectedState)
            {
                socket->write(response);
                socket->disconnectFromHost();
            }
        }
        QTest::qWait(50);
        QCOMPARE(browser.toPlainText(), u"Next article"_s);
        QTRY_COMPARE(manager->findChildren<QNetworkReply *>().size(), 0);
        QCOMPARE(requests, 3);
    }

private:
    QTemporaryDir m_profileDir;
};

QTEST_MAIN(TestRSSReaderFunctional)
#include "testrssreaderfunctional.moc"
