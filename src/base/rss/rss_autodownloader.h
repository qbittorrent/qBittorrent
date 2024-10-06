/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017-2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QBasicTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QRegularExpression>
#include <QSharedPointer>

#include "base/applicationcomponent.h"
#include "base/exceptions.h"
#include "base/settingvalue.h"
#include "base/utils/thread.h"

class QTimer;

class Application;
class AsyncFileStorage;
struct ProcessingJob;

namespace RSS
{
    class Article;
    class Feed;
    class Item;

    class AutoDownloadRule;

    class ParsingError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
    };

    class AutoDownloader final : public ApplicationComponent<QObject>
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(AutoDownloader)

        friend class ::Application;

        explicit AutoDownloader(IApplication *app);
        ~AutoDownloader() override;

    public:
        enum class RulesFileFormat
        {
            Legacy,
            JSON
        };

        static AutoDownloader *instance();

        bool isProcessingEnabled() const;
        void setProcessingEnabled(bool enabled);

        QStringList smartEpisodeFilters() const;
        void setSmartEpisodeFilters(const QStringList &filters);
        QRegularExpression smartEpisodeRegex() const;

        bool downloadRepacks() const;
        void setDownloadRepacks(bool enabled);

        bool hasRule(const QString &ruleName) const;
        AutoDownloadRule ruleByName(const QString &ruleName) const;
        QList<AutoDownloadRule> rules() const;

        void setRule(const AutoDownloadRule &rule);
        bool renameRule(const QString &ruleName, const QString &newRuleName);
        void removeRule(const QString &ruleName);

        QByteArray exportRules(RulesFileFormat format = RulesFileFormat::JSON) const;
        void importRules(const QByteArray &data, RulesFileFormat format = RulesFileFormat::JSON);

    signals:
        void processingStateChanged(bool enabled);
        void ruleAdded(const QString &ruleName);
        void ruleChanged(const QString &ruleName);
        void ruleRenamed(const QString &ruleName, const QString &oldRuleName);
        void ruleAboutToBeRemoved(const QString &ruleName);

    private slots:
        void process();
        void handleTorrentAdded(const QString &source);
        void handleAddTorrentFailed(const QString &url);
        void handleNewArticle(const Article *article);
        void handleFeedURLChanged(Feed *feed, const QString &oldURL);

    private:
        void timerEvent(QTimerEvent *event) override;
        void setRule_impl(const AutoDownloadRule &rule);
        void sortRules();
        void resetProcessingQueue();
        void startProcessing();
        void addJobForArticle(const Article *article);
        void processJob(const QSharedPointer<ProcessingJob> &job);
        void load();
        void loadRules(const QByteArray &data);
        void loadRulesLegacy();
        void store();
        void storeDeferred();
        QByteArray exportRulesToJSONFormat() const;
        void importRulesFromJSONFormat(const QByteArray &data);
        QByteArray exportRulesToLegacyFormat() const;
        void importRulesFromLegacyFormat(const QByteArray &data);

        static QPointer<AutoDownloader> m_instance;

        CachedSettingValue<bool> m_storeProcessingEnabled;
        SettingValue<QVariant> m_storeSmartEpisodeFilter;
        SettingValue<bool> m_storeDownloadRepacks;

        QTimer *m_processingTimer = nullptr;
        Utils::Thread::UniquePtr m_ioThread;
        AsyncFileStorage *m_fileStorage = nullptr;
        QList<AutoDownloadRule> m_rules;
        QHash<QString, qsizetype> m_rulesByName;
        QList<QSharedPointer<ProcessingJob>> m_processingQueue;
        QHash<QString, QSharedPointer<ProcessingJob>> m_waitingJobs;
        bool m_dirty = false;
        QBasicTimer m_savingTimer;
        QRegularExpression m_smartEpisodeRegex;
    };
}
