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

#include "torrentparamrules.h"

#include <utility>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QString>
#include <QtAssert>
#include <QVector>

#include "base/bittorrent/session.h"
#include "base/bittorrent/trackerentry.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/path.h"
#include "base/tag.h"

using namespace BitTorrent;

namespace
{
    using TorrentParamConditions = QList<TorrentParamCondition *>;
    using TorrentParamModifiers = QList<TorrentParamModifier *>;

    // Keys used in JSON. Changing existing strings may break compatibility.
    const QString KEY_CATEGORY = u"Category"_s;
    const QString KEY_CONDITION = u"Condition"_s;
    const QString KEY_CONDITIONS = u"Conditions"_s;
    const QString KEY_MODIFIER = u"Modifier"_s;
    const QString KEY_MODIFIERS = u"Modifiers"_s;
    const QString KEY_PATH = u"Path"_s;
    const QString KEY_REGEX_PATTERN = u"RegexPattern"_s;
    const QString KEY_RULES = u"Rules"_s;
    const QString KEY_TAG = u"Tag"_s;
    const QString KEY_TYPE = u"Type"_s;

    // Supported conditions.
    enum class ConditionType
    {
        // Combine multiple subconditions as a logical AND.
        AllOf,
        // Combine multiple subconditions as a logical OR.
        AnyOf,
        // Negates a condition
        Not,
        // Require at least one file's path matches a regex.
        AnyFilePathRegex,
        // Require at least one tracker's URL matches a regex.
        AnyTrackerUrlRegex,
    };

    // Supported modifiers.
    enum class ModifierType
    {
        // Combine multiple modifiers.
        Compound,
        // Insert a tag in the torrent's set of tags.
        AddTag,
        // Set a torrent's category. Adds it to the category repository if it
        // did not already exist.
        SetCategory,
        // Set the save path for a torrent. Disables AutoTMM if enabled.
        // Path must already exist.
        SetSavePath,
    };

    // Values used in JSON. Changing existing strings may break compatibility.
    QString conditionTypeStr(const ConditionType t)
    {
        switch (t)
        {
        case ConditionType::AllOf:
            return u"AllOf"_s;
        case ConditionType::AnyOf:
            return u"AnyOf"_s;
        case ConditionType::Not:
            return u"Not"_s;
        case ConditionType::AnyFilePathRegex:
            return u"AnyFilePathRegex"_s;
        case ConditionType::AnyTrackerUrlRegex:
            return u"AnyTrackerUrlRegex"_s;
        }
        Q_ASSERT(false);
        return u""_s;
    }

    // Values used in JSON. Changing existing strings may break compatibility.
    QString ModifierTypeStr(const ModifierType t)
    {
        switch (t)
        {
        case ModifierType::Compound:
            return u"Compound"_s;
        case ModifierType::AddTag:
            return u"AddTag"_s;
        case ModifierType::SetCategory:
            return u"SetCategory"_s;
        case ModifierType::SetSavePath:
            return u"SetSavePath"_s;
        }
        Q_ASSERT(false);
        return u""_s;
    }

    std::optional<ConditionType> decodeConditionType(const QString &type)
    {
        static const QMap<QString, ConditionType> map
        {
            {conditionTypeStr(ConditionType::AllOf), ConditionType::AllOf},
            {conditionTypeStr(ConditionType::AnyOf), ConditionType::AnyOf},
            {conditionTypeStr(ConditionType::Not), ConditionType::Not},
            {conditionTypeStr(ConditionType::AnyFilePathRegex), ConditionType::AnyFilePathRegex},
            {conditionTypeStr(ConditionType::AnyTrackerUrlRegex), ConditionType::AnyTrackerUrlRegex},
        };
        if (auto it = map.find(type); it != map.end())
            return *it;
        return std::nullopt;
    }

    std::optional<ModifierType> decodeModifierType(const QString &type)
    {
        static const QMap<QString, ModifierType> map
        {
            {ModifierTypeStr(ModifierType::Compound), ModifierType::Compound},
            {ModifierTypeStr(ModifierType::AddTag), ModifierType::AddTag},
            {ModifierTypeStr(ModifierType::SetCategory), ModifierType::SetCategory},
            {ModifierTypeStr(ModifierType::SetSavePath), ModifierType::SetSavePath},
        };
        if (auto it = map.find(type); it != map.end())
            return *it;
        return std::nullopt;
    }

    bool checkRequiredField(const QJsonObject &obj, const QString &key, const QJsonValue::Type type)
    {
        auto field = obj.find(key);
        if ((field == obj.end()) || (field.value().type() != type))
        {
            QString objStr = QString::fromUtf8(QJsonDocument(obj).toJson());
            static constexpr char kMsg[] = "\"%1\" field missing or invalid type when parsing "
                                           "auto torrent customizer rules. Parent JSON object:\n%2";
            LogMsg(QCoreApplication::translate("TorrentParamRules", kMsg).arg(key, objStr), Log::WARNING);
            return false;
        }
        return true;
    }

    template <typename T>
    T *parseCompoundCondition(const QJsonObject &condition, QObject *parent)
    {
        if (!checkRequiredField(condition, KEY_CONDITIONS, QJsonValue::Array))
            return nullptr;
        QJsonArray conditions = condition.value(KEY_CONDITIONS).toArray();
        TorrentParamConditions subConditions;
        for (const auto &subCondition : conditions)
            if (auto *parsed = TorrentParamCondition::fromJson(subCondition.toObject(), parent))
                subConditions.push_back(parsed);
        return new T(std::move(subConditions), parent);
    }

    template <typename T>
    T *parseRegexCondition(const QJsonObject &condition, QObject *parent)
    {
        if (!checkRequiredField(condition, KEY_REGEX_PATTERN, QJsonValue::String))
            return nullptr;
        QString pattern = condition.value(KEY_REGEX_PATTERN).toString();
        return new T(QRegularExpression(std::move(pattern)), parent);
    }

    class AllOfCondition : public TorrentParamCondition
    {
    public:
        static AllOfCondition *fromJson(const QJsonObject &json, QObject *parent)
        {
            return parseCompoundCondition<AllOfCondition>(json, parent);
        }

        AllOfCondition(TorrentParamConditions conditions, QObject *parent)
            : TorrentParamCondition(parent)
            , m_conditions(std::move(conditions)) {}

        bool isSatisfied(const TorrentDescriptor &descriptor) const override
        {
            return satisfiesAll(m_conditions, descriptor);
        }

        bool isSatisfied(const Torrent &torrent) const override
        {
            return satisfiesAll(m_conditions, torrent);
        }

        QJsonValue toJson() const override
        {
            QJsonObject json;
            QJsonArray conditions;
            json[KEY_TYPE] = conditionTypeStr(ConditionType::AllOf);
            for (const auto &condition : m_conditions)
                conditions.push_back(condition->toJson());
            json[KEY_CONDITIONS] = std::move(conditions);
            return json;
        }

    private:
        template <typename T>
        bool satisfiesAll(const TorrentParamConditions &conditions, const T &t) const
        {
            if (conditions.empty())
                return false;
            for (const auto &condition : conditions)
                if (!condition->isSatisfied(t))
                    return false;
            return true;
        }

        TorrentParamConditions m_conditions;
    };

    class AnyOfCondition : public TorrentParamCondition
    {
    public:
        static AnyOfCondition *fromJson(const QJsonObject &json, QObject *parent)
        {
            return parseCompoundCondition<AnyOfCondition>(json, parent);
        }

        AnyOfCondition(TorrentParamConditions conditions, QObject *parent)
            : TorrentParamCondition(parent)
            , m_conditions(std::move(conditions)) {}

        bool isSatisfied(const TorrentDescriptor &descriptor) const override
        {
            return satisfiesAny(m_conditions, descriptor);
        }

        bool isSatisfied(const Torrent &torrent) const override
        {
            return satisfiesAny(m_conditions, torrent);
        }

        QJsonValue toJson() const override
        {
            QJsonObject json;
            QJsonArray conditions;
            json[KEY_TYPE] = conditionTypeStr(ConditionType::AnyOf);
            for (const auto &condition : m_conditions)
                conditions.push_back(condition->toJson());
            json[KEY_CONDITIONS] = std::move(conditions);
            return json;
        }

    private:
        template <typename T>
        bool satisfiesAny(const TorrentParamConditions &conditions, const T &t) const
        {
            for (const auto &condition : conditions)
                if (condition->isSatisfied(t))
                    return true;
            return false;
        }

        TorrentParamConditions m_conditions;
    };

    class NotCondition : public TorrentParamCondition
    {
    public:
        static NotCondition *fromJson(const QJsonObject &json, QObject *parent)
        {
            if (checkRequiredField(json, KEY_CONDITION, QJsonValue::Object))
                if (auto *condition = TorrentParamCondition::fromJson(json.value(KEY_CONDITION).toObject(), parent))
                    return new NotCondition(condition, parent);
            return nullptr;
        }

        NotCondition(TorrentParamCondition *condition, QObject *parent)
            : TorrentParamCondition(parent)
            , m_condition(condition) {}

        bool isSatisfied(const TorrentDescriptor &descriptor) const override
        {
            return !m_condition->isSatisfied(descriptor);
        }

        bool isSatisfied(const Torrent &torrent) const override
        {
            return !m_condition->isSatisfied(torrent);
        }

        QJsonValue toJson() const override
        {
            QJsonObject json;
            json[KEY_TYPE] = conditionTypeStr(ConditionType::Not);
            json[KEY_CONDITION] = m_condition->toJson();
            return json;
        }

    private:
        TorrentParamCondition *m_condition;
    };

    class AnyTrackerUrlRegexCondition : public TorrentParamCondition
    {
    public:
        static AnyTrackerUrlRegexCondition *fromJson(const QJsonObject &json, QObject *parent)
        {
            return parseRegexCondition<AnyTrackerUrlRegexCondition>(json, parent);
        }

        AnyTrackerUrlRegexCondition(QRegularExpression regex, QObject *parent)
            : TorrentParamCondition(parent)
            , m_regex(std::move(regex)) {}

        bool isSatisfied(const TorrentDescriptor &descriptor) const override
        {
            if (matchTracker(descriptor.trackers(), m_regex))
                return true;
            // Trackers in the descriptor and in info() may not be the same.
            else if (descriptor.info() && matchTracker(descriptor.info()->trackers(), m_regex))
                return true;
            return false;
        }

        bool isSatisfied(const Torrent &torrent) const override
        {
            return matchTracker(torrent.trackers(), m_regex);
        }

        QJsonValue toJson() const override
        {
            QJsonObject json;
            json[KEY_TYPE] = conditionTypeStr(ConditionType::AnyTrackerUrlRegex);
            json[KEY_REGEX_PATTERN] = m_regex.pattern();
            return json;
        }

    private:
        bool matchTracker(const QVector<TrackerEntry> &trackers, const QRegularExpression &regex) const
        {
            for (const auto &tracker : trackers)
                if (regex.match(tracker.url).hasMatch())
                    return true;
            return false;
        }

        QRegularExpression m_regex;
    };

    class AnyFilePathRegexCondition : public TorrentParamCondition
    {
    public:
        static AnyFilePathRegexCondition *fromJson(const QJsonObject &json, QObject *parent)
        {
            return parseRegexCondition<AnyFilePathRegexCondition>(json, parent);
        }

        AnyFilePathRegexCondition(QRegularExpression regex, QObject *parent)
            : TorrentParamCondition(parent)
            , m_regex(std::move(regex)) {}

        bool isSatisfied(const TorrentDescriptor &descriptor) const override
        {
            if (!descriptor.info() || !descriptor.info()->isValid())
                return false;
            return matchFilePath(descriptor.info()->filePaths(), m_regex);
        }

        bool isSatisfied(const Torrent &torrent) const override
        {
            return matchFilePath(torrent.filePaths(), m_regex);
        }

        QJsonValue toJson() const override
        {
            QJsonObject json;
            json[KEY_TYPE] = conditionTypeStr(ConditionType::AnyFilePathRegex);
            json[KEY_REGEX_PATTERN] = m_regex.pattern();
            return json;
        }

    private:
        bool matchFilePath(const PathList &filePaths, const QRegularExpression &regex) const
        {
            for (const Path &path : filePaths)
                if (regex.match(path.toString()).hasMatch())
                    return true;
            return false;
        }

        QRegularExpression m_regex;
    };

    class CompoundModifier : public TorrentParamModifier
    {
    public:
        static CompoundModifier *fromJson(const QJsonObject &json, QObject *parent)
        {
            if (!checkRequiredField(json, KEY_MODIFIERS, QJsonValue::Array))
                return nullptr;
            QJsonArray modifiers = json.value(KEY_MODIFIERS).toArray();
            TorrentParamModifiers subModifiers;
            for (const auto &subModifier : modifiers)
                if (auto *parsed = TorrentParamModifier::fromJson(subModifier.toObject(), parent))
                    subModifiers.push_back(parsed);
            return new CompoundModifier(std::move(subModifiers), parent);
        }

        CompoundModifier(TorrentParamModifiers modifiers, QObject *parent)
            : TorrentParamModifier(parent)
            , m_modifiers(std::move(modifiers)) {}

        void modify(AddTorrentParams *params) const override
        {
            for (const auto &modifier : m_modifiers)
                modifier->modify(params);
        }

        void modify(Torrent *torrent) const override
        {
            for (const auto &modifier : m_modifiers)
                modifier->modify(torrent);
        }

        QJsonValue toJson() const override
        {
            QJsonObject json;
            QJsonArray modifiers;
            json[KEY_TYPE] = ModifierTypeStr(ModifierType::Compound);
            for (const auto &modifier : m_modifiers)
                modifiers.push_back(modifier->toJson());
            json[KEY_MODIFIERS] = std::move(modifiers);
            return json;
        }

    private:
        TorrentParamModifiers m_modifiers;
    };

    class AddTagModifier : public TorrentParamModifier
    {
    public:
        static AddTagModifier *fromJson(const QJsonObject &json, QObject *parent)
        {
            if (!checkRequiredField(json, KEY_TAG, QJsonValue::String))
                return nullptr;
            QString tag = json.value(KEY_TAG).toString();
            return new AddTagModifier(std::move(tag), parent);
        }

        AddTagModifier(QString tag, QObject *parent)
            : TorrentParamModifier(parent)
            , m_tag(std::move(tag)) {}

        void modify(AddTorrentParams *params) const override
        {
            params->tags.insert(Tag{m_tag});
        }

        void modify(Torrent *torrent) const override
        {
            torrent->addTag(Tag{m_tag});
        }

        QJsonValue toJson() const override
        {
            QJsonObject json;
            json[KEY_TYPE] = ModifierTypeStr(ModifierType::AddTag);
            json[KEY_TAG] = m_tag;
            return json;
        }

    private:
        QString m_tag;
    };

    class SetCategoryModifier : public TorrentParamModifier
    {
    public:
        static SetCategoryModifier *fromJson(const QJsonObject &json, QObject *parent)
        {
            if (!checkRequiredField(json, KEY_CATEGORY, QJsonValue::String))
                return nullptr;
            QString category = json.value(KEY_CATEGORY).toString();
            return new SetCategoryModifier(std::move(category), parent);
        }

        SetCategoryModifier(QString category, QObject *parent)
            : TorrentParamModifier(parent)
            , m_category(std::move(category)) {}

        void modify(AddTorrentParams *params) const override
        {
            params->category = m_category;
        }

        void modify(Torrent *torrent) const override
        {
            // When setting a category after a torrent has been added, it must be
            // be considered valid by the session to be accepted.
            torrent->session()->addCategory(m_category);
            torrent->setCategory(m_category);
        }

        QJsonValue toJson() const override
        {
            QJsonObject json;
            json[KEY_TYPE] = ModifierTypeStr(ModifierType::SetCategory);
            json[KEY_CATEGORY] = m_category;
            return json;
        }

    private:
        QString m_category;
    };

    class SetSavePathModifier : public TorrentParamModifier
    {
    public:
        static SetSavePathModifier *fromJson(const QJsonObject &json, QObject *parent)
        {
            if (!checkRequiredField(json, KEY_PATH, QJsonValue::String))
                return nullptr;
            Path path(json.value(KEY_PATH).toString());
            if (!checkPath(path))
                return nullptr;
            return new SetSavePathModifier(std::move(path), parent);
        }

        SetSavePathModifier(Path path, QObject *parent)
            : TorrentParamModifier(parent)
            , m_path(std::move(path)) {}

        void modify(AddTorrentParams *params) const override
        {
            params->useAutoTMM = false;
            params->savePath = m_path;
        }

        void modify(Torrent *torrent) const override
        {
            torrent->setAutoTMMEnabled(false);
            torrent->setSavePath(m_path);
        }

        QJsonValue toJson() const override
        {
            QJsonObject json;
            json[KEY_TYPE] = ModifierTypeStr(ModifierType::SetSavePath);
            json[KEY_PATH] = m_path.toString();
            return json;
        }

    private:
        static bool checkPath(const Path &path)
        {
            if (path.isValid())
                return true;
            LogMsg(tr("Save path \"%1\" is invalid.").arg(path.toString()), Log::WARNING);
            return false;
        }

        Path m_path;
    };
}

TorrentParamCondition *TorrentParamCondition::fromJson(const QJsonObject &json, QObject *parent)
{
    if (!checkRequiredField(json, KEY_TYPE, QJsonValue::String))
        return nullptr;
    QString typeStr = json.value(KEY_TYPE).toString();
    std::optional<ConditionType> type = decodeConditionType(typeStr);
    if (type) {
        switch (type.value())
        {
        case ConditionType::AllOf:
            return AllOfCondition::fromJson(json, parent);
        case ConditionType::AnyOf:
            return AnyOfCondition::fromJson(json, parent);
        case ConditionType::Not:
            return NotCondition::fromJson(json, parent);
        case ConditionType::AnyTrackerUrlRegex:
            return AnyTrackerUrlRegexCondition::fromJson(json, parent);
        case ConditionType::AnyFilePathRegex:
            return AnyFilePathRegexCondition::fromJson(json, parent);
        }
    }
    LogMsg(tr("Unsupported \"%1\" value: \"%2\"").arg(KEY_CONDITION, typeStr), Log::WARNING);
    return nullptr;
}

TorrentParamModifier *TorrentParamModifier::fromJson(const QJsonObject &json, QObject *parent)
{
    if (!checkRequiredField(json, KEY_TYPE, QJsonValue::String))
        return nullptr;
    QString typeStr = json.value(KEY_TYPE).toString();
    std::optional<ModifierType> type = decodeModifierType(typeStr);
    if (type) {
        switch (type.value())
        {
        case ModifierType::Compound:
            return CompoundModifier::fromJson(json, parent);
        case ModifierType::AddTag:
            return AddTagModifier::fromJson(json, parent);
        case ModifierType::SetCategory:
            return SetCategoryModifier::fromJson(json, parent);
        case ModifierType::SetSavePath:
            return SetSavePathModifier::fromJson(json, parent);
        }
    }
    LogMsg(tr("Unsupported \"%1\" value: \"%2\"").arg(KEY_MODIFIER, typeStr), Log::WARNING);
    return nullptr;
}

TorrentParamRule::TorrentParamRule(TorrentParamCondition *condition, TorrentParamModifier *modifier, QObject *parent)
    : TorrentParamRuleComponent(parent)
    , m_condition(std::move(condition))
    , m_modifier(std::move(modifier))
{
}

void TorrentParamRule::apply(const TorrentDescriptor &descriptor, AddTorrentParams *params) const
{
    if (m_condition->isSatisfied(descriptor))
        m_modifier->modify(params);
}

void TorrentParamRule::apply(Torrent *torrent) const
{
    if (m_condition->isSatisfied(*torrent))
        m_modifier->modify(torrent);
}

QJsonValue TorrentParamRule::toJson() const
{
    QJsonObject json;
    json[KEY_CONDITION] = m_condition->toJson();
    json[KEY_MODIFIER] = m_modifier->toJson();
    return json;
}

TorrentParamRule *TorrentParamRule::fromJson(const QJsonObject &json, QObject *parent)
{
    if (!checkRequiredField(json, KEY_CONDITION, QJsonValue::Object) ||
        !checkRequiredField(json, KEY_MODIFIER, QJsonValue::Object))
        return nullptr;
    auto *condition = TorrentParamCondition::fromJson(json.value(KEY_CONDITION).toObject(), parent);
    auto *modifier = TorrentParamModifier::fromJson(json.value(KEY_MODIFIER).toObject(), parent);
    if (condition && modifier)
        return new TorrentParamRule(condition, modifier, parent);
    return nullptr;
}

void TorrentParamRules::apply(const TorrentDescriptor &descriptor, AddTorrentParams *params) const
{
    for (const auto &rule : m_rules)
        rule->apply(descriptor, params);
}

void TorrentParamRules::apply(Torrent *torrent) const
{
    for (const auto &rule : m_rules)
        rule->apply(torrent);
}

void TorrentParamRules::addRule(TorrentParamRule *rule)
{
    if (rule)
        m_rules.push_back(rule);
}

void TorrentParamRules::clearRules()
{
    m_rules.clear();
}

QJsonObject TorrentParamRules::toJson() const
{
    QJsonObject json;
    QJsonArray rules;
    for (const auto &rule : m_rules)
        rules.push_back(rule->toJson());
    json[KEY_RULES] = std::move(rules);
    return json;
}

size_t TorrentParamRules::loadRulesFromJson(const QJsonObject &json)
{
    if (checkRequiredField(json, KEY_RULES, QJsonValue::Array))
    {
        const QJsonArray rules = json.value(KEY_RULES).toArray();
        for (const QJsonValue &rule : rules)
            addRule(TorrentParamRule::fromJson(rule.toObject(), this));
    }
    return m_rules.size();
}
