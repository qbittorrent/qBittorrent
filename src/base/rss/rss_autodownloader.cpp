/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Vladimir Golovnev <glassez@yandex.ru>
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

#include "rss_autodownloader.h"

#include <QDataStream>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVector>

#include "../bittorrent/magneturi.h"
#include "../bittorrent/session.h"
#include "../asyncfilestorage.h"
#include "../global.h"
#include "../logger.h"
#include "../profile.h"
#include "../utils/fs.h"
#include "rss_article.h"
#include "rss_autodownloadrule.h"
#include "rss_feed.h"
#include "rss_folder.h"
#include "rss_session.h"

struct ProcessingJob
{
    QString feedURL;
    QVariantHash articleData;
};

const QString CONF_FOLDER_NAME = u"rss"_qs;
const QString RULES_FILE_NAME = u"download_rules.json"_qs;

namespace
{
    QVector<RSS::AutoDownloadRule> rulesFromJSON(const QByteArray &jsonData)
    {
        QJsonParseError jsonError;
        const QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &jsonError);
        if (jsonError.error != QJsonParseError::NoError)
            throw RSS::ParsingError(jsonError.errorString());

        if (!jsonDoc.isObject())
            throw RSS::ParsingError(RSS::AutoDownloader::tr("Invalid data format."));

        const QJsonObject jsonObj {jsonDoc.object()};
        QVector<RSS::AutoDownloadRule> rules;
        for (auto it = jsonObj.begin(); it != jsonObj.end(); ++it)
        {
            const QJsonValue jsonVal {it.value()};
            if (!jsonVal.isObject())
                throw RSS::ParsingError(RSS::AutoDownloader::tr("Invalid data format."));

            rules.append(RSS::AutoDownloadRule::fromJsonObject(jsonVal.toObject(), it.key()));
        }

        return rules;
    }
}

using namespace RSS;

QPointer<AutoDownloader> AutoDownloader::m_instance = nullptr;

QString computeSmartFilterRegex(const QStringList &filters)
{
    return u"(?:_|\\b)(?:%1)(?:_|\\b)"_qs.arg(filters.join(u")|(?:"));
}

AutoDownloader::AutoDownloader()
    : m_storeProcessingEnabled(u"RSS/AutoDownloader/EnableProcessing"_qs, false)
    , m_storeSmartEpisodeFilter(u"RSS/AutoDownloader/SmartEpisodeFilter"_qs)
    , m_storeDownloadRepacks(u"RSS/AutoDownloader/DownloadRepacks"_qs)
    , m_processingTimer(new QTimer(this))
    , m_ioThread(new QThread(this))
{
    Q_ASSERT(!m_instance); // only one instance is allowed
    m_instance = this;

    m_fileStorage = new AsyncFileStorage(specialFolderLocation(SpecialFolder::Config) / Path(CONF_FOLDER_NAME));

    m_fileStorage->moveToThread(m_ioThread);
    connect(m_ioThread, &QThread::finished, m_fileStorage, &AsyncFileStorage::deleteLater);
    connect(m_fileStorage, &AsyncFileStorage::failed, [](const Path &fileName, const QString &errorString)
    {
        LogMsg(tr("Couldn't save RSS AutoDownloader data in %1. Error: %2")
               .arg(fileName.toString(), errorString), Log::CRITICAL);
    });

    m_ioThread->start();

    connect(BitTorrent::Session::instance(), &BitTorrent::Session::downloadFromUrlFinished
            , this, &AutoDownloader::handleTorrentDownloadFinished);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::downloadFromUrlFailed
            , this, &AutoDownloader::handleTorrentDownloadFailed);

    // initialise the smart episode regex
    const QString regex = computeSmartFilterRegex(smartEpisodeFilters());
    m_smartEpisodeRegex = QRegularExpression(regex,
                                             QRegularExpression::CaseInsensitiveOption
                                             | QRegularExpression::ExtendedPatternSyntaxOption
                                             | QRegularExpression::UseUnicodePropertiesOption);

    load();

    m_processingTimer->setSingleShot(true);
    connect(m_processingTimer, &QTimer::timeout, this, &AutoDownloader::process);

    const auto *btSession = BitTorrent::Session::instance();
    if (btSession->isRestored())
    {
        if (isProcessingEnabled())
            startProcessing();
    }
    else
    {
        connect(btSession, &BitTorrent::Session::restored, this, [this]()
        {
            if (isProcessingEnabled())
                startProcessing();
        });
    }
}

AutoDownloader::~AutoDownloader()
{
    store();

    m_ioThread->quit();
    m_ioThread->wait();
}

AutoDownloader *AutoDownloader::instance()
{
    return m_instance;
}

bool AutoDownloader::hasRule(const QString &ruleName) const
{
    return m_rules.contains(ruleName);
}

AutoDownloadRule AutoDownloader::ruleByName(const QString &ruleName) const
{
    return m_rules.value(ruleName, AutoDownloadRule(u"Unknown Rule"_qs));
}

QList<AutoDownloadRule> AutoDownloader::rules() const
{
    return m_rules.values();
}

void AutoDownloader::insertRule(const AutoDownloadRule &rule)
{
    if (!hasRule(rule.name()))
    {
        // Insert new rule
        setRule_impl(rule);
        m_dirty = true;
        store();
        emit ruleAdded(rule.name());
        resetProcessingQueue();
    }
    else if (ruleByName(rule.name()) != rule)
    {
        // Update existing rule
        setRule_impl(rule);
        m_dirty = true;
        storeDeferred();
        emit ruleChanged(rule.name());
        resetProcessingQueue();
    }
}

bool AutoDownloader::renameRule(const QString &ruleName, const QString &newRuleName)
{
    if (!hasRule(ruleName)) return false;
    if (hasRule(newRuleName)) return false;

    AutoDownloadRule rule = m_rules.take(ruleName);
    rule.setName(newRuleName);
    m_rules.insert(newRuleName, rule);
    m_dirty = true;
    store();
    emit ruleRenamed(newRuleName, ruleName);
    return true;
}

void AutoDownloader::removeRule(const QString &ruleName)
{
    if (m_rules.contains(ruleName))
    {
        emit ruleAboutToBeRemoved(ruleName);
        m_rules.remove(ruleName);
        m_dirty = true;
        store();
    }
}

QByteArray AutoDownloader::exportRules(AutoDownloader::RulesFileFormat format) const
{
    switch (format)
    {
    case RulesFileFormat::Legacy:
        return exportRulesToLegacyFormat();
    default:
        return exportRulesToJSONFormat();
    }
}

void AutoDownloader::importRules(const QByteArray &data, const AutoDownloader::RulesFileFormat format)
{
    switch (format)
    {
    case RulesFileFormat::Legacy:
        importRulesFromLegacyFormat(data);
        break;
    default:
        importRulesFromJSONFormat(data);
    }
}

QByteArray AutoDownloader::exportRulesToJSONFormat() const
{
    QJsonObject jsonObj;
    for (const auto &rule : asConst(rules()))
        jsonObj.insert(rule.name(), rule.toJsonObject());

    return QJsonDocument(jsonObj).toJson();
}

void AutoDownloader::importRulesFromJSONFormat(const QByteArray &data)
{
    for (const auto &rule : asConst(rulesFromJSON(data)))
        insertRule(rule);
}

QByteArray AutoDownloader::exportRulesToLegacyFormat() const
{
    QVariantHash dict;
    for (const auto &rule : asConst(rules()))
        dict[rule.name()] = rule.toLegacyDict();

    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_5);
    out << dict;

    return data;
}

void AutoDownloader::importRulesFromLegacyFormat(const QByteArray &data)
{
    QDataStream in(data);
    in.setVersion(QDataStream::Qt_4_5);
    QVariantHash dict;
    in >> dict;
    if (in.status() != QDataStream::Ok)
        throw ParsingError(tr("Invalid data format"));

    for (const QVariant &val : asConst(dict))
        insertRule(AutoDownloadRule::fromLegacyDict(val.toHash()));
}

QStringList AutoDownloader::smartEpisodeFilters() const
{
    const QVariant filter = m_storeSmartEpisodeFilter.get();
    if (filter.isNull())
    {
        const QStringList defaultFilters =
        {
            u"s(\\d+)e(\\d+)"_qs,                       // Format 1: s01e01
            u"(\\d+)x(\\d+)"_qs,                        // Format 2: 01x01
            u"(\\d{4}[.\\-]\\d{1,2}[.\\-]\\d{1,2})"_qs, // Format 3: 2017.01.01
            u"(\\d{1,2}[.\\-]\\d{1,2}[.\\-]\\d{4})"_qs  // Format 4: 01.01.2017
        };
        return defaultFilters;
    }
    return filter.toStringList();
}

QRegularExpression AutoDownloader::smartEpisodeRegex() const
{
    return m_smartEpisodeRegex;
}

void AutoDownloader::setSmartEpisodeFilters(const QStringList &filters)
{
    m_storeSmartEpisodeFilter = filters;

    const QString regex = computeSmartFilterRegex(filters);
    m_smartEpisodeRegex.setPattern(regex);
}

bool AutoDownloader::downloadRepacks() const
{
    return m_storeDownloadRepacks.get(true);
}

void AutoDownloader::setDownloadRepacks(const bool enabled)
{
    m_storeDownloadRepacks = enabled;
}

void AutoDownloader::process()
{
    if (m_processingQueue.isEmpty()) return; // processing was disabled

    processJob(m_processingQueue.takeFirst());
    if (!m_processingQueue.isEmpty())
        // Schedule to process the next torrent (if any)
        m_processingTimer->start();
}

void AutoDownloader::handleTorrentDownloadFinished(const QString &url)
{
    const auto job = m_waitingJobs.take(url);
    if (!job) return;

    if (Feed *feed = Session::instance()->feedByURL(job->feedURL))
        if (Article *article = feed->articleByGUID(job->articleData.value(Article::KeyId).toString()))
            article->markAsRead();
}

void AutoDownloader::handleTorrentDownloadFailed(const QString &url)
{
    m_waitingJobs.remove(url);
    // TODO: Re-schedule job here.
}

void AutoDownloader::handleNewArticle(const Article *article)
{
    if (!article->isRead() && !article->torrentUrl().isEmpty())
        addJobForArticle(article);
}

void AutoDownloader::setRule_impl(const AutoDownloadRule &rule)
{
    m_rules.insert(rule.name(), rule);
}

void AutoDownloader::addJobForArticle(const Article *article)
{
    const QString torrentURL = article->torrentUrl();
    if (m_waitingJobs.contains(torrentURL)) return;

    QSharedPointer<ProcessingJob> job(new ProcessingJob);
    job->feedURL = article->feed()->url();
    job->articleData = article->data();
    m_processingQueue.append(job);
    if (!m_processingTimer->isActive())
        m_processingTimer->start();
}

void AutoDownloader::processJob(const QSharedPointer<ProcessingJob> &job)
{
    for (AutoDownloadRule &rule : m_rules)
    {
        if (!rule.isEnabled()) continue;
        if (!rule.feedURLs().contains(job->feedURL)) continue;
        if (!rule.accepts(job->articleData)) continue;

        m_dirty = true;
        storeDeferred();

        BitTorrent::AddTorrentParams params;
        params.savePath = rule.savePath();
        params.category = rule.assignedCategory();
        params.addPaused = rule.addPaused();
        params.contentLayout = rule.torrentContentLayout();
        if (!rule.savePath().isEmpty())
            params.useAutoTMM = false;
        const auto torrentURL = job->articleData.value(Article::KeyTorrentURL).toString();
        BitTorrent::Session::instance()->addTorrent(torrentURL, params);

        if (BitTorrent::MagnetUri(torrentURL).isValid())
        {
            if (Feed *feed = Session::instance()->feedByURL(job->feedURL))
            {
                if (Article *article = feed->articleByGUID(job->articleData.value(Article::KeyId).toString()))
                    article->markAsRead();
            }
        }
        else
        {
            // waiting for torrent file downloading
            m_waitingJobs.insert(torrentURL, job);
        }

        return;
    }
}

void AutoDownloader::load()
{
    QFile rulesFile {(m_fileStorage->storageDir() / Path(RULES_FILE_NAME)).data()};

    if (!rulesFile.exists())
        loadRulesLegacy();
    else if (rulesFile.open(QFile::ReadOnly))
        loadRules(rulesFile.readAll());
    else
        LogMsg(tr("Couldn't read RSS AutoDownloader rules from %1. Error: %2")
               .arg(rulesFile.fileName(), rulesFile.errorString()), Log::CRITICAL);
}

void AutoDownloader::loadRules(const QByteArray &data)
{
    try
    {
        const auto rules = rulesFromJSON(data);
        for (const auto &rule : rules)
            setRule_impl(rule);
    }
    catch (const ParsingError &error)
    {
        LogMsg(tr("Couldn't load RSS AutoDownloader rules. Reason: %1")
               .arg(error.message()), Log::CRITICAL);
    }
}

void AutoDownloader::loadRulesLegacy()
{
    const std::unique_ptr<QSettings> settings = Profile::instance()->applicationSettings(u"qBittorrent-rss"_qs);
    const QVariantHash rules = settings->value(u"download_rules"_qs).toHash();
    for (const QVariant &ruleVar : rules)
    {
        const auto rule = AutoDownloadRule::fromLegacyDict(ruleVar.toHash());
        if (!rule.name().isEmpty())
            insertRule(rule);
    }
}

void AutoDownloader::store()
{
    if (!m_dirty) return;

    m_dirty = false;
    m_savingTimer.stop();

    QJsonObject jsonObj;
    for (const auto &rule : asConst(m_rules))
        jsonObj.insert(rule.name(), rule.toJsonObject());

    m_fileStorage->store(Path(RULES_FILE_NAME), QJsonDocument(jsonObj).toJson());
}

void AutoDownloader::storeDeferred()
{
    if (!m_savingTimer.isActive())
        m_savingTimer.start(5 * 1000, this);
}

bool AutoDownloader::isProcessingEnabled() const
{
    return m_storeProcessingEnabled;
}

void AutoDownloader::resetProcessingQueue()
{
    m_processingQueue.clear();
    if (!isProcessingEnabled()) return;

    for (Article *article : asConst(Session::instance()->rootFolder()->articles()))
    {
        if (!article->isRead() && !article->torrentUrl().isEmpty())
            addJobForArticle(article);
    }
}

void AutoDownloader::startProcessing()
{
    resetProcessingQueue();

    const RSS::Folder *rootFolder = Session::instance()->rootFolder();
    for (const Article *article : asConst(rootFolder->articles()))
        handleNewArticle(article);

    connect(rootFolder, &Folder::newArticle, this, &AutoDownloader::handleNewArticle);
}

void AutoDownloader::setProcessingEnabled(const bool enabled)
{
    if (m_storeProcessingEnabled != enabled)
    {
        m_storeProcessingEnabled = enabled;
        if (enabled)
        {
            if (BitTorrent::Session::instance()->isRestored())
                startProcessing();
        }
        else
        {
            m_processingQueue.clear();
            disconnect(Session::instance()->rootFolder(), &Folder::newArticle, this, &AutoDownloader::handleNewArticle);
        }

        emit processingStateChanged(enabled);
    }
}

void AutoDownloader::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);
    store();
}
