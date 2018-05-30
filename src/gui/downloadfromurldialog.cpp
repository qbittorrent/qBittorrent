/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "downloadfromurldialog.h"

#include <QClipboard>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

#include "ui_downloadfromurldialog.h"
#include "utils.h"

namespace
{
    bool isDownloadable(const QString &str)
    {
        return (str.startsWith("http://", Qt::CaseInsensitive)
            || str.startsWith("https://", Qt::CaseInsensitive)
            || str.startsWith("ftp://", Qt::CaseInsensitive)
            || str.startsWith("magnet:", Qt::CaseInsensitive)
            || str.startsWith("bc://bt/", Qt::CaseInsensitive)
            || ((str.size() == 40) && !str.contains(QRegularExpression("[^0-9A-Fa-f]")))
            || ((str.size() == 32) && !str.contains(QRegularExpression("[^2-7A-Za-z]"))));
    }
}

DownloadFromURLDialog::DownloadFromURLDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::DownloadFromURLDialog)
{
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(true);

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Download"));
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &DownloadFromURLDialog::downloadButtonClicked);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_ui->textUrls->setWordWrapMode(QTextOption::NoWrap);

    // Paste clipboard if there is an URL in it
    const QStringList clipboardList = qApp->clipboard()->text().split('\n');

    QSet<QString> uniqueURLs;
    for (QString str : clipboardList) {
        str = str.trimmed();
        if (str.isEmpty()) continue;

        if (isDownloadable(str))
            uniqueURLs << str;
    }
    m_ui->textUrls->setText(uniqueURLs.toList().join('\n'));

    Utils::Gui::resize(this);
    show();
}

DownloadFromURLDialog::~DownloadFromURLDialog()
{
    delete m_ui;
}

void DownloadFromURLDialog::downloadButtonClicked()
{
    const QStringList urls = m_ui->textUrls->toPlainText().split('\n');

    QSet<QString> uniqueURLs;
    for (QString url : urls) {
        url = url.trimmed();
        if (url.isEmpty()) continue;

        uniqueURLs << url;
    }

    if (uniqueURLs.isEmpty()) {
        QMessageBox::warning(this, tr("No URL entered"), tr("Please type at least one URL."));
        return;
    }

    emit urlsReadyToBeDownloaded(uniqueURLs.toList());
    accept();
}
