#include "htmlbrowser.h"

#include <QDebug>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkDiskCache>
#include <QStyle>
#include <QApplication>
#include <QDir>
#include <QDateTime>
#include <QScrollBar>

#include "core/utils/fs.h"

HtmlBrowser::HtmlBrowser(QWidget* parent)
    : QTextBrowser(parent)
{
    m_netManager = new QNetworkAccessManager(this);
    m_diskCache = new QNetworkDiskCache(this);
    m_diskCache->setCacheDirectory(QDir::cleanPath(Utils::Fs::cacheLocation() + "/rss"));
    m_diskCache->setMaximumCacheSize(50 * 1024 * 1024);
    qDebug() << "HtmlBrowser  cache path:" << m_diskCache->cacheDirectory() << " max size:" << m_diskCache->maximumCacheSize() / 1024 / 1024 << "MB";
    m_netManager->setCache(m_diskCache);

    connect(m_netManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(resourceLoaded(QNetworkReply*)));
}

HtmlBrowser::~HtmlBrowser()
{
}

QVariant HtmlBrowser::loadResource(int type, const QUrl &name)
{
    if(type == QTextDocument::ImageResource) {
        QUrl url(name);
        if(url.scheme().isEmpty())
            url.setScheme("http");

        QIODevice *dev = m_diskCache->data(url);
        if(dev != 0) {
            qDebug() << "HtmlBrowser::loadResource() cache " << url.toString();
            QByteArray res = dev->readAll();
            delete dev;
            return res;
        }

        if(!m_activeRequests.contains(url)) {
            m_activeRequests.insert(url, true);
            qDebug() << "HtmlBrowser::loadResource() get " << url.toString();
            QNetworkRequest req(url);
            req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
            m_netManager->get(req);
        }

        return QVariant();
    }

    return QTextBrowser::loadResource(type, name);
}

void HtmlBrowser::resourceLoaded(QNetworkReply *reply)
{
    m_activeRequests.remove(reply->request().url());

    if(reply->error() == QNetworkReply::NoError && reply->size() > 0) {
        qDebug() << "HtmlBrowser::resourceLoaded() save " << reply->request().url().toString();
    }
    else {
        // If resource failed to load, replace it with warning icon and store it in cache for 1 day.
        // Otherwise HTMLBrowser will keep trying to download it every time article is displayed,
        // since it's not possible to cache error responses.
        QNetworkCacheMetaData metaData;
        QNetworkCacheMetaData::AttributesMap atts;
        metaData.setUrl(reply->request().url());
        metaData.setSaveToDisk(true);
        atts[QNetworkRequest::HttpStatusCodeAttribute] = 200;
        atts[QNetworkRequest::HttpReasonPhraseAttribute] = "Ok";
        metaData.setAttributes(atts);
        metaData.setLastModified(QDateTime::currentDateTime());
        metaData.setExpirationDate(QDateTime::currentDateTime().addDays(1));
        QIODevice *dev = m_diskCache->prepare(metaData);
        if(!dev)
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
