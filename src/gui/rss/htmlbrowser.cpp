/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2013  Mladen Milinkovic <max@smoothware.net>
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

#include "htmlbrowser.h"

#include <QImage>
#include <QDebug>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "base/path.h"
#include "base/profile.h"

using namespace Qt::Literals::StringLiterals;

HtmlBrowser::HtmlBrowser(QWidget *parent)
    : QTextBrowser(parent)
{
    m_netManager = new QNetworkAccessManager(this);
    m_diskCache = new QNetworkDiskCache(this);
    m_diskCache->setCacheDirectory((specialFolderLocation(SpecialFolder::Cache) / Path(u"rss"_s)).data());
    m_diskCache->setMaximumCacheSize(50 * 1024 * 1024);
    qDebug() << "HtmlBrowser cache path:" << m_diskCache->cacheDirectory() << " max size:" << m_diskCache->maximumCacheSize() / 1024 / 1024 << "MB";
    m_netManager->setCache(m_diskCache);

    connect(m_netManager, &QNetworkAccessManager::finished, this, &HtmlBrowser::resourceLoaded);
}

void HtmlBrowser::setHtml(const QString &html)
{
    clear();
    QTextBrowser::setHtml(html);
}

void HtmlBrowser::clear()
{
    // Remove ownership before abort(): finished() can be emitted synchronously.
    const auto requests = m_activeRequests;
    m_activeRequests.clear();
    m_failedResources.clear();
    for (auto *reply : requests)
        reply->abort();
    QTextBrowser::clear();
}

void HtmlBrowser::startRequest(const QUrl &url)
{
    if (m_activeRequests.contains(url))
        return;
    QNetworkRequest request {url};
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
        QNetworkRequest::PreferCache);
    m_activeRequests.insert(url, m_netManager->get(request));
}

QVariant HtmlBrowser::loadResource(const int type, const QUrl &name)
{
    if (type != QTextDocument::ImageResource)
        return QTextBrowser::loadResource(type, name);

    const QUrl url = document()->baseUrl().resolved(name);
    if ((url.scheme() != u"http") && (url.scheme() != u"https"))
        return QTextBrowser::loadResource(type, name);

    if (m_failedResources.contains(url))
        return QByteArray {};
    startRequest(url);
    // Keep Qt's original resource-missing layout while the request is pending.
    return {};
}

void HtmlBrowser::updateResource(const QUrl &url, const QVariant &resource)
{
    document()->addResource(QTextDocument::ImageResource, url, resource);
    document()->markContentsDirty(0, document()->characterCount());
    viewport()->update();
}

void HtmlBrowser::resourceLoaded(QNetworkReply *reply)
{
    reply->deleteLater();
    const QUrl url = reply->request().url();
    // Ignore aborted responses from an article that is no longer displayed,
    // including old responses for a URL also used by the new article.
    if (m_activeRequests.value(url) != reply)
        return;
    m_activeRequests.remove(url);

    QImage image;
    if (reply->error() == QNetworkReply::NoError)
        image = QImage::fromData(reply->readAll());
    if (image.isNull())
    {
        m_failedResources.insert(url);
        // A valid, empty byte resource lets Qt draw its native missing-image
        // icon without calling our network loader again.
        updateResource(url, QByteArray {});
        return;
    }
    updateResource(url, image);
}
