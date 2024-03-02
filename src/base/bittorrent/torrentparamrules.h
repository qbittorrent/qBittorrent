/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Tony Gregerson <tony.gregerson@gmail.com>
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

#include <QJsonValue>
#include <QList>
#include <QObject>
#include <QRegularExpression>
#include <QString>

#include "addtorrentparams.h"
#include "torrent.h"
#include "torrentdescriptor.h"

// This library defines a framework for modifying torrent parameters (e.g.,
// category, tags) based on a set of rules related to the torrent's metadata.
//
// Rules consist of a set of conditions and a set of modifications. If the
// conditions are satisfied, the modifications are applied. If multiple rules
// match a given torrent, modifications are applied in sequential order.
//
// Metadata may or may not be available when initially adding the torrent, and
// there are two API variants to address each of these cases. The first operates
// on a TorrentDescriptor and AddTorrentParams, and is used when metadata is
// available prior to adding a torrent to the session. The second operates on a
// Torrent object, and is used when metadata arrives after adding the torrent.

namespace BitTorrent
{
    class TorrentParamRuleComponent : public QObject
    {
    public:
        using QObject::QObject;

        virtual QJsonValue toJson() const = 0;
    };

    class TorrentParamCondition : public TorrentParamRuleComponent
    {
    public:
        static TorrentParamCondition *fromJson(const QJsonObject &json, QObject *parent);

        virtual ~TorrentParamCondition() = default;

        virtual bool isSatisfied(const TorrentDescriptor &descriptor) const = 0;
        virtual bool isSatisfied(const Torrent &torrent) const = 0;

    protected:
        using TorrentParamRuleComponent::TorrentParamRuleComponent;
    };

    class TorrentParamModifier : public TorrentParamRuleComponent
    {
    public:
        static TorrentParamModifier *fromJson(const QJsonObject &json, QObject *parent);

        virtual ~TorrentParamModifier() = default;

        virtual void modify(AddTorrentParams *params) const = 0;
        virtual void modify(Torrent *torrent) const = 0;

    protected:
        using TorrentParamRuleComponent::TorrentParamRuleComponent;
    };

    class TorrentParamRule : public TorrentParamRuleComponent
    {
    public:
        using TorrentParamRuleComponent::TorrentParamRuleComponent;

        static TorrentParamRule *fromJson(const QJsonObject &json, QObject *parent);

        TorrentParamRule(TorrentParamCondition *condition, TorrentParamModifier *modifier, QObject *parent);

        void apply(const TorrentDescriptor &descriptor, AddTorrentParams *params) const;
        void apply(Torrent *torrent) const;

        QJsonValue toJson() const override;

    private:
        TorrentParamCondition *m_condition;
        TorrentParamModifier *m_modifier;
    };

    class TorrentParamRules : public QObject
    {
    public:
        using QObject::QObject;

        void addRule(TorrentParamRule *rule);

        void clearRules();

        void apply(const TorrentDescriptor &descriptor, AddTorrentParams *params) const;
        void apply(Torrent *torrent) const;

        QJsonObject toJson() const;
        size_t loadRulesFromJson(const QJsonObject &json);

    private:
        QList<TorrentParamRule *> m_rules;
    };
}
