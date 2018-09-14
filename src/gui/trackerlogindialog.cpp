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

#include "trackerlogindialog.h"

#include <libtorrent/version.hpp>

#include <QPushButton>

#include "base/bittorrent/torrenthandle.h"
#include "guiiconprovider.h"
#include "utils.h"

TrackerLoginDialog::TrackerLoginDialog(QWidget *parent, BitTorrent::TorrentHandle *const torrent)
    : QDialog(parent)
    , m_torrent(torrent)
{
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Log in"));

    labelLoginLogo->setPixmap(Utils::Gui::scaledPixmap(GuiIconProvider::instance()->getIcon("document-encrypt"), this, 32));

    labelTrackerURL->setText(torrent->currentTracker());

    connect(buttonBox, &QDialogButtonBox::accepted, this, &TrackerLoginDialog::loginButtonClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &TrackerLoginDialog::cancelButtonClicked);
    connect(linePasswd, &QLineEdit::returnPressed, this, &TrackerLoginDialog::loginButtonClicked);
    connect(this, SIGNAL(trackerLoginCancelled(QPair<BitTorrent::TorrentHandle*, QString>)),  // TODO: use Qt5 connect syntax
        parent, SLOT(addUnauthenticatedTracker(QPair<BitTorrent::TorrentHandle*, QString>)));

    Utils::Gui::resize(this);
    show();
}

TrackerLoginDialog::~TrackerLoginDialog() {}

void TrackerLoginDialog::loginButtonClicked()
{
     // login
#if LIBTORRENT_VERSION_NUM < 10100
     m_torrent->setTrackerLogin(lineUsername->text(), linePasswd->text());
#endif
    accept();
}

void TrackerLoginDialog::cancelButtonClicked()
{
    // Emit a signal to GUI to stop asking for authentication
    emit trackerLoginCancelled(qMakePair(m_torrent, m_torrent->currentTracker()));
    reject();
}
