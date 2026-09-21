/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Tim Sylvester <t.j.sylvester@gmail.com>
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
#include <QObject>

#include "base/path.h"

class QJsonArray;

struct DiscoveryRootOptions
{
    bool recursive = false;

    bool operator==(const DiscoveryRootOptions &other) const = default;
};

struct DiscoveryRoot
{
    Path path;
    DiscoveryRootOptions options;

    bool operator==(const DiscoveryRoot &other) const = default;
};

class DiscoveryRoots final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DiscoveryRoots)

public:
    static void initInstance();
    static void freeInstance();
    static DiscoveryRoots *instance();

    QList<DiscoveryRoot> roots() const;
    void setRoots(const QList<DiscoveryRoot> &roots);

signals:
    void rootsChanged();

private:
    explicit DiscoveryRoots(QObject *parent = nullptr);

    void load();
    void store() const;

    static DiscoveryRoots *m_instance;

    QList<DiscoveryRoot> m_roots;
};

QList<DiscoveryRoot> parseDiscoveryRoots(const QJsonArray &jsonArray);
QJsonArray serializeDiscoveryRoots(const QList<DiscoveryRoot> &roots);
