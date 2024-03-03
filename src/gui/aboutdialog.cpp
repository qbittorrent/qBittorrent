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

#include "aboutdialog.h"

#include <QClipboard>

#include "base/global.h"
#include "base/path.h"
#include "base/unicodestrings.h"
#include "base/utils/io.h"
#include "base/utils/misc.h"
#include "base/version.h"
#include "ui_aboutdialog.h"
#include "uithememanager.h"
#include "utils.h"

#define SETTINGS_KEY(name) u"AboutDialog/" name

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::AboutDialog)
    , m_storeDialogSize(SETTINGS_KEY(u"Size"_s))
{
    m_ui->setupUi(this);

    // Title
    m_ui->labelName->setText(QStringLiteral("<b><h2>qBittorrent " QBT_VERSION " (%1-bit)</h2></b>").arg(QT_POINTER_SIZE * 8));

    m_ui->logo->setPixmap(UIThemeManager::instance()->getScaledPixmap(u"qbittorrent-tray"_s, 32));

    // About
    const QString aboutText =
        u"<p style=\"white-space: pre-wrap;\">"
        u"%1\n\n"
        u"%2\n\n"
        u"<table>"
        u"<tr><td>%3</td><td><a href=\"https://www.qbittorrent.org\">https://www.qbittorrent.org</a></td></tr>"
        u"<tr><td>%4</td><td><a href=\"https://forum.qbittorrent.org\">https://forum.qbittorrent.org</a></td></tr>"
        u"<tr><td>%5</td><td><a href=\"https://bugs.qbittorrent.org\">https://bugs.qbittorrent.org</a></td></tr>"
        u"</table>"
        u"</p>"_s
        .arg(tr("An advanced BitTorrent client programmed in C++, based on Qt toolkit and libtorrent-rasterbar.")
                .replace(u"C++"_s, u"C\u2060+\u2060+"_s) // make C++ non-breaking
            , tr("Copyright %1 2006-2024 The qBittorrent project").arg(C_COPYRIGHT)
            , tr("Home Page:")
            , tr("Forum:")
            , tr("Bug Tracker:"));
    m_ui->labelAbout->setText(aboutText);

    m_ui->labelMascot->setPixmap(Utils::Gui::scaledPixmap(Path(u":/icons/mascot.png"_s)));

    // Thanks
    if (const auto readResult = Utils::IO::readFile(Path(u":/thanks.html"_s), -1, QIODevice::Text))
    {
        m_ui->textBrowserThanks->setHtml(QString::fromUtf8(readResult.value()));
    }

    // Translation
    if (const auto readResult = Utils::IO::readFile(Path(u":/translators.html"_s), -1, QIODevice::Text))
    {
        m_ui->textBrowserTranslation->setHtml(QString::fromUtf8(readResult.value()));
    }

    // License
    if (const auto readResult = Utils::IO::readFile(Path(u":/gpl.html"_s), -1, QIODevice::Text))
    {
        m_ui->textBrowserLicense->setHtml(QString::fromUtf8(readResult.value()));
    }

    // Software Used
    m_ui->labelQtVer->setText(QStringLiteral(QT_VERSION_STR));
    m_ui->labelLibtVer->setText(Utils::Misc::libtorrentVersionString());
    m_ui->labelBoostVer->setText(Utils::Misc::boostVersionString());
    m_ui->labelOpensslVer->setText(Utils::Misc::opensslVersionString());
    m_ui->labelZlibVer->setText(Utils::Misc::zlibVersionString());

    connect(m_ui->btnCopyToClipboard, &QAbstractButton::clicked, this, &AboutDialog::copyVersionsToClipboard);

    const QString DBIPText = u"<html><head/><body><p>"
                             u"%1 (<a href=\"https://db-ip.com/\">https://db-ip.com/</a>)"
                             u"</p></body></html>"_s
                             .arg(tr("The free IP to Country Lite database by DB-IP is used for resolving the countries of peers. "
                                     "The database is licensed under the Creative Commons Attribution 4.0 International License"));
    m_ui->labelDBIP->setText(DBIPText);

    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);
}

AboutDialog::~AboutDialog()
{
    m_storeDialogSize = size();
    delete m_ui;
}

void AboutDialog::copyVersionsToClipboard() const
{
    const QString versions = u"%1 %2\n%3 %4\n%5 %6\n%7 %8\n%9 %10\n"_s
        .arg(m_ui->labelQt->text(), m_ui->labelQtVer->text()
            , m_ui->labelLibt->text(), m_ui->labelLibtVer->text()
            , m_ui->labelBoost->text(), m_ui->labelBoostVer->text()
            , m_ui->labelOpenssl->text(), m_ui->labelOpensslVer->text()
            , m_ui->labelZlib->text(), m_ui->labelZlibVer->text());
    qApp->clipboard()->setText(versions);
}
