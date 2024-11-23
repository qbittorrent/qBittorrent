/*
 * Bittorrent Client using Qt and libtorrent.
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

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScrollBar>
#include <QStyle>

#include "base/global.h"
#include "base/path.h"
#include "base/profile.h"

HtmlBrowser::HtmlBrowser(QWidget *parent)
    : QTextBrowser(parent)
{
    m_netManager = new QNetworkAccessManager(this);
    m_diskCache = new QNetworkDiskCache(this);
    m_diskCache->setCacheDirectory((specialFolderLocation(SpecialFolder::Cache) / Path(u"rss"_s)).data());
    m_diskCache->setMaximumCacheSize(50 * 1024 * 1024);
    qDebug() << "HtmlBrowser  cache path:" << m_diskCache->cacheDirectory() << " max size:" << m_diskCache->maximumCacheSize() / 1024 / 1024 << "MB";
    m_netManager->setCache(m_diskCache);

    connect(m_netManager, &QNetworkAccessManager::finished, this, &HtmlBrowser::resourceLoaded);
}

QVariant HtmlBrowser::loadResource(int type, const QUrl &name)
{
    if (type == QTextDocument::ImageResource)
    {
        QUrl url(name);
        if (url.scheme().isEmpty())
            url.setScheme(u"http"_s);

        QIODevice *dev = m_diskCache->data(url);
        if (dev)
        {
            qDebug() << "HtmlBrowser::loadResource() cache " << url.toString();
            QByteArray res = dev->readAll();
            delete dev;
            return res;
        }

        if (!m_activeRequests.contains(url))
        {
            m_activeRequests.insert(url, true);
            qDebug() << "HtmlBrowser::loadResource() get " << url.toString();
            QNetworkRequest req(url);
            req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
            m_netManager->get(req);
        }

        return {};
    }

    return QTextBrowser::loadResource(type, name);
}

void HtmlBrowser::resourceLoaded(QNetworkReply *reply)
{
    m_activeRequests.remove(reply->request().url());

    if ((reply->error() == QNetworkReply::NoError) && (reply->size() > 0))
    {
        qDebug() << "HtmlBrowser::resourceLoaded() save " << reply->request().url().toString();
    }
    else
    {
        // If resource failed to load, replace it with warning icon and store it in cache for 1 day.
        // Otherwise HTMLBrowser will keep trying to download it every time article is displayed,
        // since it's not possible to cache error responses.
        QNetworkCacheMetaData metaData;
        QNetworkCacheMetaData::AttributesMap atts;
        metaData.setUrl(reply->request().url());
        metaData.setSaveToDisk(true);
        atts[QNetworkRequest::HttpStatusCodeAttribute] = 200;
        atts[QNetworkRequest::HttpReasonPhraseAttribute] = u"Ok"_s;
        metaData.setAttributes(atts);
        const auto currentDateTime = QDateTime::currentDateTime();
        metaData.setLastModified(currentDateTime);
        metaData.setExpirationDate(currentDateTime.addDays(1));
        QIODevice *dev = m_diskCache->prepare(metaData);
        if (!dev)
            return;

        QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(32, 32).save(dev, "PNG");
        m_diskCache->insert(dev);
    }
    // Refresh the document display and keep scrollbars where they are
    int sx = horizontalScrollBar()->value();
    int sy = verticalScrollBar()->value();
    document()->setHtml(document()->toHtml());
    horizontalScrollBar()->setValue(sx);
    verticalScrollBar()->setValue(sy);
}
