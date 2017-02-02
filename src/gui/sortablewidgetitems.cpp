/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2017  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#include "base/utils/string.h"
#include "sortablewidgetitems.h"

SortableListWidgetItem::SortableListWidgetItem(QListWidget *parent, Utils::String::CaseSensitivity cs, int type)
    : QListWidgetItem(parent, type)
    , m_caseSensitivity(cs)
{
}

SortableListWidgetItem::SortableListWidgetItem(const QString &text, QListWidget *parent, Utils::String::CaseSensitivity cs, int type)
    : QListWidgetItem(text, parent, type)
    , m_caseSensitivity(cs)
{
}

SortableListWidgetItem::SortableListWidgetItem(const QIcon &icon, const QString &text, QListWidget *parent, Utils::String::CaseSensitivity cs, int type)
    : QListWidgetItem(icon, text, parent, type)
    , m_caseSensitivity(cs)
{
}

SortableListWidgetItem::SortableListWidgetItem(const SortableListWidgetItem &other)
    : QListWidgetItem(other)
    , m_caseSensitivity(other.m_caseSensitivity)
{
}

SortableListWidgetItem::~SortableListWidgetItem()
{
}

bool SortableListWidgetItem::operator<(const QListWidgetItem &other) const
{
    return Utils::String::naturalCompare(text(), other.text(), m_caseSensitivity);
}

SortableTreeWidgetItem::SortableTreeWidgetItem(Utils::String::CaseSensitivity cs, int type)
    : QTreeWidgetItem(type)
    , m_caseSensitivity(cs)
{
}

SortableTreeWidgetItem::SortableTreeWidgetItem(const QStringList &strings, Utils::String::CaseSensitivity cs, int type)
    : QTreeWidgetItem(strings, type)
    , m_caseSensitivity(cs)
{
}

SortableTreeWidgetItem::SortableTreeWidgetItem(QTreeWidget *parent, Utils::String::CaseSensitivity cs, int type)
    : QTreeWidgetItem(parent, type)
    , m_caseSensitivity(cs)
{
}

SortableTreeWidgetItem::SortableTreeWidgetItem(QTreeWidget *parent, const QStringList &strings, Utils::String::CaseSensitivity cs, int type)
    : QTreeWidgetItem(parent, strings, type)
    , m_caseSensitivity(cs)
{
}

SortableTreeWidgetItem::SortableTreeWidgetItem(QTreeWidget *parent, QTreeWidgetItem *preceding, Utils::String::CaseSensitivity cs, int type)
    : QTreeWidgetItem(parent, preceding, type)
    , m_caseSensitivity(cs)
{
}

SortableTreeWidgetItem::SortableTreeWidgetItem(QTreeWidgetItem *parent, Utils::String::CaseSensitivity cs, int type)
    : QTreeWidgetItem(parent, type)
    , m_caseSensitivity(cs)
{
}

SortableTreeWidgetItem::SortableTreeWidgetItem(QTreeWidgetItem *parent, const QStringList &strings, Utils::String::CaseSensitivity cs, int type)
    : QTreeWidgetItem(parent, strings, type)
    , m_caseSensitivity(cs)
{
}

SortableTreeWidgetItem::SortableTreeWidgetItem(QTreeWidgetItem *parent, QTreeWidgetItem *preceding, Utils::String::CaseSensitivity cs, int type)
    : QTreeWidgetItem(parent, preceding, type)
    , m_caseSensitivity(cs)
{
}

SortableTreeWidgetItem::SortableTreeWidgetItem(const SortableTreeWidgetItem &other)
    : QTreeWidgetItem(other)
    , m_caseSensitivity(other.m_caseSensitivity)
{
}

SortableTreeWidgetItem::~SortableTreeWidgetItem()
{
}

bool SortableTreeWidgetItem::operator<(const QTreeWidgetItem &other) const
{
    int column = treeWidget()->sortColumn();
    return Utils::String::naturalCompare(text(column), other.text(column), m_caseSensitivity);
}
