#ifndef HTMLBROWSER_H
#define HTMLBROWSER_H

#include <QTextBrowser>
#include <QHash>

class QNetworkAccessManager;
class QNetworkDiskCache;
class QNetworkReply;

class HtmlBrowser: public QTextBrowser
{
    Q_OBJECT

public:
    explicit HtmlBrowser(QWidget* parent = 0);
    ~HtmlBrowser();

    virtual QVariant loadResource(int type, const QUrl &name);

protected:
    QNetworkAccessManager *m_netManager;
    QNetworkDiskCache *m_diskCache;
    QHash<QUrl, bool> m_activeRequests;

protected slots:
    void resourceLoaded(QNetworkReply *reply);
};

#endif // HTMLBROWSER_H
