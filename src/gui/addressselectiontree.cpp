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

#include "addressselectiontree.h"

#include <algorithm>

#include <QHeaderView>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>

#include "base/global.h"
#include "base/net/ipaddresswatcher.h"
#include "base/utils/net.h"

namespace
{
    enum DataRole
    {
        KindRole = Qt::UserRole,
        IfaceRole,
        IfaceNameRole,
        AddressRole
    };

    enum class NodeKind
    {
        Interface,
        Family,
        Temporary,
        Address,
        TemporaryAddress
    };

    NodeKind nodeKind(const QTreeWidgetItem *item)
    {
        return static_cast<NodeKind>(item->data(0, KindRole).toInt());
    }

    bool isIPv6(const QHostAddress &addr)
    {
        return (addr.protocol() == QAbstractSocket::IPv6Protocol);
    }

    bool containsAddress(const QList<QHostAddress> &addresses, const QHostAddress &addr)
    {
        if (isIPv6(addr))
            return Utils::Net::containsIPv6Address(addresses, addr);

        return addresses.contains(addr);
    }

    QTreeWidgetItem *newItem(QTreeWidgetItem *parent, const QString &text, const NodeKind kind)
    {
        auto *item = new QTreeWidgetItem(parent, {text});
        item->setData(0, KindRole, static_cast<int>(kind));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(0, Qt::Unchecked);
        return item;
    }

    void markMissing(QTreeWidgetItem *item, const QString &toolTip)
    {
        item->setText(0, AddressSelectionTree::tr("%1 (not found)", "e.g. 10.8.0.2 (not found)").arg(item->text(0)));
        item->setToolTip(0, toolTip);
        QFont font = item->font(0);
        font.setItalic(true);
        item->setFont(0, font);
    }
}

AddressSelectionTree::AddressSelectionTree(QWidget *parent)
    : QTreeWidget(parent)
    , m_watcher {new Net::IPAddressWatcher(this)}
{
    setHeaderHidden(true);
    setColumnCount(1);
    setSelectionMode(QAbstractItemView::NoSelection);
    setUniformRowHeights(true);

    connect(this, &QTreeWidget::itemChanged, this, &AddressSelectionTree::handleItemChanged);
    connect(m_watcher, &Net::IPAddressWatcher::addressesChanged, this, [this]
    {
        rebuild(selection());
    });
}

void AddressSelectionTree::setSelection(const QList<BitTorrent::InterfaceSelection> &interfaces)
{
    rebuild(interfaces);
}

QList<BitTorrent::InterfaceSelection> AddressSelectionTree::selection() const
{
    QList<BitTorrent::InterfaceSelection> result;
    for (int i = 0; i < topLevelItemCount(); ++i)
    {
        const QTreeWidgetItem *ifaceItem = topLevelItem(i);

        BitTorrent::InterfaceSelection selection;
        selection.iface = ifaceItem->data(0, IfaceRole).toString();
        if (!selection.iface.isEmpty())
            selection.ifaceName = ifaceItem->data(0, IfaceNameRole).toString();

        for (int j = 0; j < ifaceItem->childCount(); ++j)
        {
            const QTreeWidgetItem *familyItem = ifaceItem->child(j);
            BitTorrent::AddressSelection &family = (j == 0) ? selection.ipv4 : selection.ipv6;
            if (familyItem->checkState(0) == Qt::Checked)
            {
                family.allAddresses = true;
                continue;
            }

            for (int k = 0; k < familyItem->childCount(); ++k)
            {
                const QTreeWidgetItem *child = familyItem->child(k);
                if (child->checkState(0) != Qt::Checked)
                    continue;

                if (nodeKind(child) == NodeKind::Temporary)
                    family.temporaryAddresses = true;
                else if (nodeKind(child) == NodeKind::Address)
                    family.fixedAddresses.append(QHostAddress(child->data(0, AddressRole).toString()));
            }
        }

        if (!selection.isEmpty())
            result.append(selection);
    }
    return result;
}

QString AddressSelectionTree::interfaceText(const BitTorrent::InterfaceSelection &selection)
{
    if (selection.iface.isEmpty())
        return tr("Any interface", "i.e. Any network interface");

    if (const QNetworkInterface iface = QNetworkInterface::interfaceFromName(selection.iface); iface.isValid())
        return iface.humanReadableName();

    return selection.ifaceName.isEmpty() ? selection.iface : selection.ifaceName;
}

QString AddressSelectionTree::addressSelectionText(const BitTorrent::AddressSelection &selection)
{
    if (selection.allAddresses)
        return tr("All", "i.e. All addresses");

    QStringList parts;
    if (selection.temporaryAddresses)
        parts.append(tr("Temporary", "i.e. Temporary addresses"));
    for (const QHostAddress &addr : selection.fixedAddresses)
        parts.append(addr.toString());
    return parts.join(u", ");
}

void AddressSelectionTree::rebuild(const QList<BitTorrent::InterfaceSelection> &interfaces)
{
    m_isUpdating = true;
    clear();

    const auto selectionFor = [&interfaces](const QString &iface) -> BitTorrent::InterfaceSelection
    {
        const auto iter = std::ranges::find(interfaces, iface, &BitTorrent::InterfaceSelection::iface);
        if (iter != interfaces.cend())
            return *iter;

        BitTorrent::InterfaceSelection selection;
        selection.iface = iface;
        return selection;
    };

    // The session does not use deprecated addresses, so they cannot be selected
    const auto usableAddresses = [](const QNetworkInterface &iface)
    {
        QList<QHostAddress> addresses;
        for (const QNetworkAddressEntry &entry : asConst(iface.addressEntries()))
        {
            if (!Utils::Net::isDeprecatedAddressEntry(entry))
                addresses.append(entry.ip());
        }
        std::ranges::sort(addresses, Utils::Net::lessThan);
        return addresses;
    };

    const QList<QNetworkInterface> systemInterfaces = QNetworkInterface::allInterfaces();
    QList<QHostAddress> systemAddresses;
    for (const QNetworkInterface &iface : systemInterfaces)
        systemAddresses.append(usableAddresses(iface));

    const auto addInterface = [this, &systemAddresses](const BitTorrent::InterfaceSelection &selection, const QString &text
        , const QList<QHostAddress> &addresses, const QList<QHostAddress> &temporaryAddresses, const bool isMissing)
    {
        auto *ifaceItem = new QTreeWidgetItem(this, {text});
        ifaceItem->setData(0, KindRole, static_cast<int>(NodeKind::Interface));
        ifaceItem->setData(0, IfaceRole, selection.iface);
        ifaceItem->setData(0, IfaceNameRole, text);
        ifaceItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        if (isMissing)
        {
            ifaceItem->setData(0, IfaceNameRole, interfaceText(selection));
            markMissing(ifaceItem, tr("The system does not have this interface now. It is used when it comes back."));
        }
        else if (selection.iface.isEmpty())
        {
            ifaceItem->setToolTip(0, tr("The addresses of all interfaces"));
        }

        // Any interface lists no addresses, but its fixed addresses can be on any interface
        const QList<QHostAddress> &knownAddresses = selection.iface.isEmpty() ? systemAddresses : addresses;

        const auto addFamily = [ifaceItem, &addresses, &temporaryAddresses, &knownAddresses](const QString &title
            , const BitTorrent::AddressSelection &family, const QAbstractSocket::NetworkLayerProtocol protocol)
        {
            QTreeWidgetItem *familyItem = newItem(ifaceItem, title, NodeKind::Family);
            familyItem->setToolTip(0, tr("Checked: all addresses of the family, also the ones that come later"));

            const bool isFamilyIPv6 = (protocol == QAbstractSocket::IPv6Protocol);
            if (isFamilyIPv6 && (Utils::Net::isTemporaryIPv6Supported() || family.temporaryAddresses))
            {
                QTreeWidgetItem *temporaryItem = newItem(familyItem, tr("Temporary addresses"), NodeKind::Temporary);
                temporaryItem->setToolTip(0, tr("The privacy extension addresses. The system changes them regularly."));
                temporaryItem->setCheckState(0, (family.temporaryAddresses ? Qt::Checked : Qt::Unchecked));
                for (const QHostAddress &addr : temporaryAddresses)
                {
                    QTreeWidgetItem *addrItem = newItem(temporaryItem, addr.toString(), NodeKind::TemporaryAddress);
                    addrItem->setFlags(Qt::NoItemFlags);
                    addrItem->setCheckState(0, temporaryItem->checkState(0));
                }
            }

            for (const QHostAddress &addr : addresses)
            {
                if ((addr.protocol() != protocol) || containsAddress(temporaryAddresses, addr))
                    continue;

                QTreeWidgetItem *addrItem = newItem(familyItem, addr.toString(), NodeKind::Address);
                addrItem->setData(0, AddressRole, addr.toString());
                if (containsAddress(family.fixedAddresses, addr))
                    addrItem->setCheckState(0, Qt::Checked);
            }

            for (const QHostAddress &addr : family.fixedAddresses)
            {
                if (containsAddress(addresses, addr))
                    continue;

                QTreeWidgetItem *addrItem = newItem(familyItem, addr.toString(), NodeKind::Address);
                addrItem->setData(0, AddressRole, addr.toString());
                addrItem->setCheckState(0, Qt::Checked);
                if (!containsAddress(knownAddresses, addr))
                    markMissing(addrItem, tr("The interface does not have this address now. It is used when it comes back."));
            }

            return familyItem;
        };

        QTreeWidgetItem *ipv4Item = addFamily(u"IPv4"_s, selection.ipv4, QAbstractSocket::IPv4Protocol);
        QTreeWidgetItem *ipv6Item = addFamily(u"IPv6"_s, selection.ipv6, QAbstractSocket::IPv6Protocol);
        // Unchecking a family also unchecks its children, which hold the rest of the selection
        if (selection.ipv4.allAddresses)
            setFamilyChecked(ipv4Item, true);
        if (selection.ipv6.allAddresses)
            setFamilyChecked(ipv6Item, true);
        updateParentStates(ipv4Item);
        updateParentStates(ipv6Item);

        if (!selection.isEmpty())
        {
            ifaceItem->setExpanded(true);
            ipv4Item->setExpanded(!selection.ipv4.isEmpty() && !selection.ipv4.allAddresses);
            ipv6Item->setExpanded(!selection.ipv6.isEmpty() && !selection.ipv6.allAddresses);
        }
    };

    // One call, because each call lists all interfaces and probes the addresses
    const QList<QHostAddress> allTemporaryAddresses = Utils::Net::temporaryIPv6Addresses();

    // Any interface lists no addresses: a fixed address belongs to its interface
    addInterface(selectionFor({}), interfaceText({}), {}, allTemporaryAddresses, false);

    for (const QNetworkInterface &iface : systemInterfaces)
    {
        const QList<QHostAddress> addresses = usableAddresses(iface);

        QList<QHostAddress> temporaryAddresses;
        for (const QHostAddress &addr : allTemporaryAddresses)
        {
            if (Utils::Net::containsIPv6Address(addresses, addr))
                temporaryAddresses.append(addr);
        }

        addInterface(selectionFor(iface.name()), iface.humanReadableName(), addresses, temporaryAddresses, false);
    }

    for (const BitTorrent::InterfaceSelection &selection : interfaces)
    {
        if (selection.iface.isEmpty())
            continue;

        const bool isPresent = std::ranges::any_of(systemInterfaces, [&selection](const QNetworkInterface &iface)
        {
            return (iface.name() == selection.iface);
        });
        if (!isPresent)
            addInterface(selection, interfaceText(selection), {}, {}, true);
    }

    m_isUpdating = false;
}

void AddressSelectionTree::handleItemChanged(QTreeWidgetItem *item)
{
    if (m_isUpdating)
        return;

    m_isUpdating = true;

    const bool isChecked = (item->checkState(0) == Qt::Checked);
    switch (nodeKind(item))
    {
    case NodeKind::Interface:
        for (int i = 0; i < item->childCount(); ++i)
            setFamilyChecked(item->child(i), isChecked);
        break;
    case NodeKind::Family:
        setFamilyChecked(item, isChecked);
        updateParentStates(item);
        break;
    case NodeKind::Temporary:
        for (int i = 0; i < item->childCount(); ++i)
            item->child(i)->setCheckState(0, item->checkState(0));
        updateParentStates(item);
        break;
    case NodeKind::Address:
        updateParentStates(item);
        break;
    case NodeKind::TemporaryAddress:
        break;
    }

    m_isUpdating = false;
    emit selectionChanged();
}

void AddressSelectionTree::setFamilyChecked(QTreeWidgetItem *familyItem, const bool checked)
{
    familyItem->setCheckState(0, (checked ? Qt::Checked : Qt::Unchecked));

    // While the family is checked, its children only show what "all" gives now
    for (int i = 0; i < familyItem->childCount(); ++i)
    {
        QTreeWidgetItem *child = familyItem->child(i);
        child->setCheckState(0, (checked ? Qt::Checked : Qt::Unchecked));
        child->setDisabled(checked);
        for (int j = 0; j < child->childCount(); ++j)
            child->child(j)->setCheckState(0, child->checkState(0));
    }
}

void AddressSelectionTree::updateParentStates(QTreeWidgetItem *item)
{
    QTreeWidgetItem *familyItem = item;
    while (familyItem && (nodeKind(familyItem) != NodeKind::Family))
        familyItem = familyItem->parent();
    if (!familyItem)
        return;

    // A checked family means "all", so a family with only some checked children is partial
    if (familyItem->checkState(0) != Qt::Checked)
    {
        bool hasCheckedChild = false;
        for (int i = 0; i < familyItem->childCount(); ++i)
        {
            if (familyItem->child(i)->checkState(0) == Qt::Checked)
                hasCheckedChild = true;
        }
        familyItem->setCheckState(0, (hasCheckedChild ? Qt::PartiallyChecked : Qt::Unchecked));
    }

    QTreeWidgetItem *ifaceItem = familyItem->parent();
    bool isAllChecked = true;
    bool isAllUnchecked = true;
    for (int i = 0; i < ifaceItem->childCount(); ++i)
    {
        const Qt::CheckState state = ifaceItem->child(i)->checkState(0);
        isAllChecked = isAllChecked && (state == Qt::Checked);
        isAllUnchecked = isAllUnchecked && (state == Qt::Unchecked);
    }
    ifaceItem->setCheckState(0, (isAllChecked ? Qt::Checked : (isAllUnchecked ? Qt::Unchecked : Qt::PartiallyChecked)));
}
