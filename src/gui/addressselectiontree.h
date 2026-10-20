/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  WordlessEcho <wordless@echo.moe>
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

#pragma once

#include <QList>
#include <QTreeWidget>

#include "base/bittorrent/listenentry.h"

namespace Net
{
    class IPAddressWatcher;
}

// A checkable tree of interface › IPv4 / IPv6 › address.
// A checked IPv4 or IPv6 node selects all addresses of the family, also the ones that come later.
// The tree follows the address changes of the system and keeps the checks.
class AddressSelectionTree final : public QTreeWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AddressSelectionTree)

public:
    explicit AddressSelectionTree(QWidget *parent = nullptr);

    // Interfaces and fixed addresses that the system does not have now stay in the tree
    void setSelection(const QList<BitTorrent::InterfaceSelection> &interfaces);
    // Only the interfaces with a check
    QList<BitTorrent::InterfaceSelection> selection() const;

    static QString interfaceText(const BitTorrent::InterfaceSelection &selection);
    // e.g. "All", "Temporary, 2001:db8::5"
    static QString addressSelectionText(const BitTorrent::AddressSelection &selection);

signals:
    void selectionChanged();

private:
    void rebuild(const QList<BitTorrent::InterfaceSelection> &interfaces);
    void handleItemChanged(QTreeWidgetItem *item);
    void setFamilyChecked(QTreeWidgetItem *familyItem, bool checked);
    void updateParentStates(QTreeWidgetItem *item);

    Net::IPAddressWatcher *m_watcher = nullptr;
    bool m_isUpdating = false;
};
