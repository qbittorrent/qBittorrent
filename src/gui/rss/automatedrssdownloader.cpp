/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017, 2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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

#include "automatedrssdownloader.h"

#include <QtVersionChecks>
#include <QCursor>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QShortcut>
#include <QSignalBlocker>
#include <QString>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/path.h"
#include "base/rss/rss_article.h"
#include "base/rss/rss_autodownloader.h"
#include "base/rss/rss_feed.h"
#include "base/rss/rss_folder.h"
#include "base/rss/rss_session.h"
#include "base/utils/compare.h"
#include "base/utils/io.h"
#include "base/utils/string.h"
#include "gui/addtorrentparamswidget.h"
#include "gui/autoexpandabledialog.h"
#include "gui/torrentcategorydialog.h"
#include "gui/uithememanager.h"
#include "gui/utils.h"
#include "ui_automatedrssdownloader.h"

const QString EXT_JSON = u".json"_s;
const QString EXT_LEGACY = u".rssrules"_s;

AutomatedRssDownloader::AutomatedRssDownloader(QWidget *parent)
    : QDialog(parent)
    , m_formatFilterJSON {u"%1 (*%2)"_s.arg(tr("Rules"), EXT_JSON)}
    , m_formatFilterLegacy {u"%1 (*%2)"_s.arg(tr("Rules (legacy)"), EXT_LEGACY)}
    , m_ui {new Ui::AutomatedRssDownloader}
    , m_addTorrentParamsWidget {new AddTorrentParamsWidget}
    , m_storeDialogSize {u"RssFeedDownloader/geometrySize"_s}
    , m_storeMainSplitterState {u"GUI/Qt6/RSSFeedDownloader/HSplitterSizes"_s}
    , m_storeRuleDefSplitterState {u"GUI/Qt6/RSSFeedDownloader/RuleDefSplitterState"_s}
{
    m_ui->setupUi(this);

    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_ui->torrentParametersGroupBox->layout()->addWidget(m_addTorrentParamsWidget);

    m_ui->prioritySpinBox->setMinimum(std::numeric_limits<int>::min());
    m_ui->prioritySpinBox->setMaximum(std::numeric_limits<int>::max());

    connect(m_ui->addRuleBtn, &QPushButton::clicked, this, &AutomatedRssDownloader::onAddRuleBtnClicked);
    connect(m_ui->removeRuleBtn, &QPushButton::clicked, this, &AutomatedRssDownloader::onRemoveRuleBtnClicked);
    connect(m_ui->exportBtn, &QPushButton::clicked, this, &AutomatedRssDownloader::onExportBtnClicked);
    connect(m_ui->importBtn, &QPushButton::clicked, this, &AutomatedRssDownloader::onImportBtnClicked);
    connect(m_ui->renameRuleBtn, &QPushButton::clicked, this, &AutomatedRssDownloader::onRenameRuleBtnClicked);

    // Icons
    m_ui->renameRuleBtn->setIcon(UIThemeManager::instance()->getIcon(u"edit-rename"_s));
    m_ui->removeRuleBtn->setIcon(UIThemeManager::instance()->getIcon(u"edit-clear"_s, u"list-remove"_s));
    m_ui->addRuleBtn->setIcon(UIThemeManager::instance()->getIcon(u"list-add"_s));

    // Ui Settings
    m_ui->ruleList->setSortingEnabled(true);
    m_ui->ruleList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_ui->matchingArticlesTree->setSortingEnabled(true);
    m_ui->matchingArticlesTree->sortByColumn(0, Qt::AscendingOrder);
    m_ui->mainSplitter->setCollapsible(0, false);
    m_ui->mainSplitter->setCollapsible(1, false);
    m_ui->mainSplitter->setCollapsible(2, true); // Only the preview list is collapsible

    connect(m_ui->checkRegex, &QAbstractButton::toggled, this, &AutomatedRssDownloader::updateFieldsToolTips);
    connect(m_ui->ruleList, &QWidget::customContextMenuRequested, this, &AutomatedRssDownloader::displayRulesListMenu);

    m_episodeRegex = new QRegularExpression(u"^(^\\d{1,4}x(\\d{1,4}(-(\\d{1,4})?)?;){1,}){1,1}"_s
                                            , QRegularExpression::CaseInsensitiveOption);
    const QString tip = u"<p>" + tr("Matches articles based on episode filter.") + u"</p><p><b>" + tr("Example: ")
        + u"1x2;8-15;5;30-;</b>" + tr(" will match 2, 5, 8 through 15, 30 and onward episodes of season one", "example X will match") + u"</p>"
        + u"<p>" + tr("Episode filter rules: ") + u"</p><ul><li>" + tr("Season number is a mandatory non-zero value") + u"</li>"
        + u"<li>" + tr("Episode number is a mandatory positive value") + u"</li>"
        + u"<li>" + tr("Filter must end with semicolon") + u"</li>"
        + u"<li>" + tr("Three range types for episodes are supported: ") + u"</li>" + u"<li><ul>"
        + u"<li>" + tr("Single number: <b>1x25;</b> matches episode 25 of season one") + u"</li>"
        + u"<li>" + tr("Normal range: <b>1x25-40;</b> matches episodes 25 through 40 of season one") + u"</li>"
        + u"<li>" + tr("Infinite range: <b>1x25-;</b> matches episodes 25 and upward of season one, and all episodes of later seasons") + u"</li>" + u"</ul></li></ul>";
    m_ui->lineEFilter->setToolTip(tip);

    loadSettings();

    connect(RSS::AutoDownloader::instance(), &RSS::AutoDownloader::ruleAdded, this, &AutomatedRssDownloader::handleRuleAdded);
    connect(RSS::AutoDownloader::instance(), &RSS::AutoDownloader::ruleRenamed, this, &AutomatedRssDownloader::handleRuleRenamed);
    connect(RSS::AutoDownloader::instance(), &RSS::AutoDownloader::ruleChanged, this, &AutomatedRssDownloader::handleRuleChanged);
    connect(RSS::AutoDownloader::instance(), &RSS::AutoDownloader::ruleAboutToBeRemoved, this, &AutomatedRssDownloader::handleRuleAboutToBeRemoved);

    // Update matching articles when necessary
    connect(m_ui->lineContains, &QLineEdit::textEdited, this, &AutomatedRssDownloader::handleRuleDefinitionChanged);
    connect(m_ui->lineContains, &QLineEdit::textEdited, this, &AutomatedRssDownloader::updateMustLineValidity);
    connect(m_ui->lineNotContains, &QLineEdit::textEdited, this, &AutomatedRssDownloader::handleRuleDefinitionChanged);
    connect(m_ui->lineNotContains, &QLineEdit::textEdited, this, &AutomatedRssDownloader::updateMustNotLineValidity);
    connect(m_ui->lineEFilter, &QLineEdit::textEdited, this, &AutomatedRssDownloader::handleRuleDefinitionChanged);
    connect(m_ui->lineEFilter, &QLineEdit::textEdited, this, &AutomatedRssDownloader::updateEpisodeFilterValidity);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(m_ui->checkRegex, &QCheckBox::checkStateChanged, this, &AutomatedRssDownloader::handleRuleDefinitionChanged);
    connect(m_ui->checkRegex, &QCheckBox::checkStateChanged, this, &AutomatedRssDownloader::updateMustLineValidity);
    connect(m_ui->checkRegex, &QCheckBox::checkStateChanged, this, &AutomatedRssDownloader::updateMustNotLineValidity);
    connect(m_ui->checkSmart, &QCheckBox::checkStateChanged, this, &AutomatedRssDownloader::handleRuleDefinitionChanged);
#else
    connect(m_ui->checkRegex, &QCheckBox::stateChanged, this, &AutomatedRssDownloader::handleRuleDefinitionChanged);
    connect(m_ui->checkRegex, &QCheckBox::stateChanged, this, &AutomatedRssDownloader::updateMustLineValidity);
    connect(m_ui->checkRegex, &QCheckBox::stateChanged, this, &AutomatedRssDownloader::updateMustNotLineValidity);
    connect(m_ui->checkSmart, &QCheckBox::stateChanged, this, &AutomatedRssDownloader::handleRuleDefinitionChanged);
#endif
    connect(m_ui->spinIgnorePeriod, qOverload<int>(&QSpinBox::valueChanged)
            , this, &AutomatedRssDownloader::handleRuleDefinitionChanged);

    connect(m_ui->listFeeds, &QListWidget::itemChanged, this, &AutomatedRssDownloader::handleFeedCheckStateChange);

    connect(m_ui->ruleList, &QListWidget::itemSelectionChanged, this, &AutomatedRssDownloader::updateRuleDefinitionBox);
    connect(m_ui->ruleList, &QListWidget::itemChanged, this, &AutomatedRssDownloader::handleRuleCheckStateChange);

    const auto *editHotkey = new QShortcut(Qt::Key_F2, m_ui->ruleList, nullptr, nullptr, Qt::WidgetShortcut);
    connect(editHotkey, &QShortcut::activated, this, &AutomatedRssDownloader::renameSelectedRule);
    const auto *deleteHotkey = new QShortcut(QKeySequence::Delete, m_ui->ruleList, nullptr, nullptr, Qt::WidgetShortcut);
    connect(deleteHotkey, &QShortcut::activated, this, &AutomatedRssDownloader::onRemoveRuleBtnClicked);

    connect(m_ui->ruleList, &QAbstractItemView::doubleClicked, this, &AutomatedRssDownloader::renameSelectedRule);

    loadFeedList();

    m_ui->ruleList->blockSignals(true);
    for (const RSS::AutoDownloadRule &rule : asConst(RSS::AutoDownloader::instance()->rules()))
        createRuleItem(rule);
    m_ui->ruleList->blockSignals(false);

    updateRuleDefinitionBox();

    if (RSS::AutoDownloader::instance()->isProcessingEnabled())
        m_ui->labelWarn->hide();

    connect(RSS::AutoDownloader::instance(), &RSS::AutoDownloader::processingStateChanged
            , this, &AutomatedRssDownloader::handleProcessingStateChanged);
}

AutomatedRssDownloader::~AutomatedRssDownloader()
{
    // Save current item on exit
    saveEditedRule();
    saveSettings();

    delete m_ui;
    delete m_episodeRegex;
}

void AutomatedRssDownloader::loadSettings()
{
    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);

    if (const QByteArray mainSplitterSize = m_storeMainSplitterState; !mainSplitterSize.isEmpty())
        m_ui->mainSplitter->restoreState(mainSplitterSize);

    if (const QByteArray ruleDefSplitterSize = m_storeRuleDefSplitterState; !ruleDefSplitterSize.isEmpty())
        m_ui->ruleDefSplitter->restoreState(ruleDefSplitterSize);
}

void AutomatedRssDownloader::saveSettings()
{
    m_storeDialogSize = size();
    m_storeMainSplitterState = m_ui->mainSplitter->saveState();
    m_storeRuleDefSplitterState = m_ui->ruleDefSplitter->saveState();
}

void AutomatedRssDownloader::createRuleItem(const RSS::AutoDownloadRule &rule)
{
    QListWidgetItem *item = new QListWidgetItem(rule.name(), m_ui->ruleList);
    m_itemsByRuleName.insert(rule.name(), item);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(rule.isEnabled() ? Qt::Checked : Qt::Unchecked);
}

void AutomatedRssDownloader::loadFeedList()
{
    const QSignalBlocker feedListSignalBlocker(m_ui->listFeeds);

    for (const auto *feed : asConst(RSS::Session::instance()->feeds()))
    {
        QListWidgetItem *item = new QListWidgetItem(feed->name(), m_ui->listFeeds);
        item->setData(Qt::UserRole, feed->url());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
    }

    updateFeedList();
}

void AutomatedRssDownloader::updateFeedList()
{
    const QSignalBlocker feedListSignalBlocker(m_ui->listFeeds);

    QList<QListWidgetItem *> selection;

    if (m_currentRuleItem)
        selection << m_currentRuleItem;
    else
        selection = m_ui->ruleList->selectedItems();

    bool enable = !selection.isEmpty();

    for (int i = 0; i < m_ui->listFeeds->count(); ++i)
    {
        QListWidgetItem *item = m_ui->listFeeds->item(i);
        const QString feedURL = item->data(Qt::UserRole).toString();
        item->setHidden(!enable);

        bool allEnabled = true;
        bool anyEnabled = false;

        for (const QListWidgetItem *ruleItem : asConst(selection))
        {
            const auto rule = RSS::AutoDownloader::instance()->ruleByName(ruleItem->text());
            if (rule.feedURLs().contains(feedURL))
                anyEnabled = true;
            else
                allEnabled = false;
        }

        if (anyEnabled && allEnabled)
            item->setCheckState(Qt::Checked);
        else if (anyEnabled)
            item->setCheckState(Qt::PartiallyChecked);
        else
            item->setCheckState(Qt::Unchecked);
    }

    m_ui->listFeeds->sortItems();
    m_ui->lblListFeeds->setEnabled(enable);
    m_ui->listFeeds->setEnabled(enable);
}

void AutomatedRssDownloader::updateRuleDefinitionBox()
{
    const QList<QListWidgetItem *> selection = m_ui->ruleList->selectedItems();
    QListWidgetItem *currentRuleItem = ((selection.count() == 1) ? selection.first() : nullptr);

    // Enable the edit rule button but only if we have 1 rule selected
    if (selection.count() == 1)
        m_ui->renameRuleBtn->setEnabled(true);
    else
        m_ui->renameRuleBtn->setEnabled(false);

    if (m_currentRuleItem != currentRuleItem)
    {
        saveEditedRule(); // Save previous rule first
        m_currentRuleItem = currentRuleItem;
        //m_ui->ruleList->setCurrentItem(m_currentRuleItem);
    }

    // Update rule definition box
    if (m_currentRuleItem)
    {
        m_currentRule = RSS::AutoDownloader::instance()->ruleByName(m_currentRuleItem->text());

        m_ui->prioritySpinBox->setValue(m_currentRule.priority());

        m_addTorrentParamsWidget->setAddTorrentParams(m_currentRule.addTorrentParams());

        m_ui->lineContains->setText(m_currentRule.mustContain());
        m_ui->lineNotContains->setText(m_currentRule.mustNotContain());
        if (!m_currentRule.episodeFilter().isEmpty())
            m_ui->lineEFilter->setText(m_currentRule.episodeFilter());
        else
            m_ui->lineEFilter->clear();
        m_ui->checkRegex->blockSignals(true);
        m_ui->checkRegex->setChecked(m_currentRule.useRegex());
        m_ui->checkRegex->blockSignals(false);
        m_ui->checkSmart->blockSignals(true);
        m_ui->checkSmart->setChecked(m_currentRule.useSmartFilter());
        m_ui->checkSmart->blockSignals(false);
        m_ui->spinIgnorePeriod->setValue(m_currentRule.ignoreDays());
        QDateTime dateTime = m_currentRule.lastMatch();
        QString lMatch;
        if (dateTime.isValid())
            lMatch = tr("Last Match: %1 days ago").arg(dateTime.daysTo(QDateTime::currentDateTime()));
        else
            lMatch = tr("Last Match: Unknown");
        m_ui->lblLastMatch->setText(lMatch);
        updateMustLineValidity();
        updateMustNotLineValidity();
        updateEpisodeFilterValidity();

        updateFieldsToolTips(m_ui->checkRegex->isChecked());
        m_ui->ruleScrollArea->setEnabled(true);
    }
    else
    {
        m_currentRule = RSS::AutoDownloadRule();
        clearRuleDefinitionBox();
        m_ui->ruleScrollArea->setEnabled(false);
    }

    updateFeedList();
    updateMatchingArticles();
}

void AutomatedRssDownloader::clearRuleDefinitionBox()
{
    m_addTorrentParamsWidget->setAddTorrentParams({});
    m_ui->prioritySpinBox->setValue(0);
    m_ui->lineContains->clear();
    m_ui->lineNotContains->clear();
    m_ui->lineEFilter->clear();
    m_ui->checkRegex->setChecked(false);
    m_ui->checkSmart->setChecked(false);
    m_ui->spinIgnorePeriod->setValue(0);
    updateFieldsToolTips(m_ui->checkRegex->isChecked());
    updateMustLineValidity();
    updateMustNotLineValidity();
    updateEpisodeFilterValidity();
}

void AutomatedRssDownloader::updateEditedRule()
{
    if (!m_currentRuleItem || !m_ui->ruleScrollArea->isEnabled())
        return;

    m_currentRule.setEnabled(m_currentRuleItem->checkState() != Qt::Unchecked);
    m_currentRule.setPriority(m_ui->prioritySpinBox->value());
    m_currentRule.setUseRegex(m_ui->checkRegex->isChecked());
    m_currentRule.setUseSmartFilter(m_ui->checkSmart->isChecked());
    m_currentRule.setMustContain(m_ui->lineContains->text());
    m_currentRule.setMustNotContain(m_ui->lineNotContains->text());
    m_currentRule.setEpisodeFilter(m_ui->lineEFilter->text());
    m_currentRule.setIgnoreDays(m_ui->spinIgnorePeriod->value());

    m_currentRule.setAddTorrentParams(m_addTorrentParamsWidget->addTorrentParams());
}

void AutomatedRssDownloader::saveEditedRule()
{
    if (!m_currentRuleItem || !m_ui->ruleScrollArea->isEnabled()) return;

    updateEditedRule();
    RSS::AutoDownloader::instance()->setRule(m_currentRule);
}

void AutomatedRssDownloader::onAddRuleBtnClicked()
{
//    saveEditedRule();

    // Ask for a rule name
    const QString ruleName = AutoExpandableDialog::getText(
                this, tr("New rule name"), tr("Please type the name of the new download rule."));
    if (ruleName.isEmpty()) return;

    // Check if this rule name already exists
    if (RSS::AutoDownloader::instance()->hasRule(ruleName))
    {
        QMessageBox::warning(this, tr("Rule name conflict")
                             , tr("A rule with this name already exists, please choose another name."));
        return;
    }

    RSS::AutoDownloader::instance()->setRule(RSS::AutoDownloadRule(ruleName));
}

void AutomatedRssDownloader::onRemoveRuleBtnClicked()
{
    const QList<QListWidgetItem *> selection = m_ui->ruleList->selectedItems();
    if (selection.isEmpty()) return;

    // Ask for confirmation
    const QString confirmText = ((selection.count() == 1)
                                 ? tr("Are you sure you want to remove the download rule named '%1'?")
                                   .arg(selection.first()->text())
                                 : tr("Are you sure you want to remove the selected download rules?"));
    if (QMessageBox::question(this, tr("Rule deletion confirmation"), confirmText, QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
        return;

    for (const QListWidgetItem *item : selection)
        RSS::AutoDownloader::instance()->removeRule(item->text());
}

void AutomatedRssDownloader::onRenameRuleBtnClicked()
{
    renameSelectedRule();
}

void AutomatedRssDownloader::onExportBtnClicked()
{
    if (RSS::AutoDownloader::instance()->rules().isEmpty())
    {
        QMessageBox::warning(this, tr("Invalid action")
                             , tr("The list is empty, there is nothing to export."));
        return;
    }

    QString selectedFilter {m_formatFilterJSON};
    Path path {QFileDialog::getSaveFileName(
                this, tr("Export RSS rules"), QDir::homePath()
                , u"%1;;%2"_s.arg(m_formatFilterJSON, m_formatFilterLegacy), &selectedFilter)};

    if (path.isEmpty()) return;

    const RSS::AutoDownloader::RulesFileFormat format
    {
        (selectedFilter == m_formatFilterJSON)
                ? RSS::AutoDownloader::RulesFileFormat::JSON
                : RSS::AutoDownloader::RulesFileFormat::Legacy
    };

    if (format == RSS::AutoDownloader::RulesFileFormat::JSON)
    {
        if (!path.hasExtension(EXT_JSON))
            path += EXT_JSON;
    }
    else
    {
        if (!path.hasExtension(EXT_LEGACY))
            path += EXT_LEGACY;
    }

    const QByteArray rules = RSS::AutoDownloader::instance()->exportRules(format);
    const nonstd::expected<void, QString> result = Utils::IO::saveToFile(path, rules);
    if (!result)
    {
        QMessageBox::critical(this, tr("I/O Error")
            , tr("Failed to create the destination file. Reason: %1").arg(result.error()));
    }
}

void AutomatedRssDownloader::onImportBtnClicked()
{
    QString selectedFilter {m_formatFilterJSON};
    const Path path {QFileDialog::getOpenFileName(
                    this, tr("Import RSS rules"), QDir::homePath()
                    , u"%1;;%2"_s.arg(m_formatFilterJSON, m_formatFilterLegacy), &selectedFilter)};

    const int fileMaxSize = 10 * 1024 * 1024;
    const auto readResult = Utils::IO::readFile(path, fileMaxSize);
    if (!readResult)
    {
        if (readResult.error().status == Utils::IO::ReadError::NotExist)
            return;

        QMessageBox::critical(this, tr("Import error")
            , tr("Failed to read the file. %1").arg(readResult.error().message));
        return;
    }

    const RSS::AutoDownloader::RulesFileFormat format
    {
        (selectedFilter == m_formatFilterJSON)
                ? RSS::AutoDownloader::RulesFileFormat::JSON
                : RSS::AutoDownloader::RulesFileFormat::Legacy
    };

    try
    {
        RSS::AutoDownloader::instance()->importRules(readResult.value(), format);
    }
    catch (const RSS::ParsingError &error)
    {
        QMessageBox::critical(this, tr("Import error")
            , tr("Failed to import the selected rules file. Reason: %1").arg(error.message()));
    }
}

void AutomatedRssDownloader::displayRulesListMenu()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(UIThemeManager::instance()->getIcon(u"list-add"_s), tr("Add new rule...")
                    , this, &AutomatedRssDownloader::onAddRuleBtnClicked);

    const QList<QListWidgetItem *> selection = m_ui->ruleList->selectedItems();

    if (!selection.isEmpty())
    {
        if (selection.count() == 1)
        {
            menu->addAction(UIThemeManager::instance()->getIcon(u"edit-clear"_s, u"list-remove"_s), tr("Delete rule")
                            , this, &AutomatedRssDownloader::onRemoveRuleBtnClicked);
            menu->addSeparator();
            menu->addAction(UIThemeManager::instance()->getIcon(u"edit-rename"_s), tr("Rename rule...")
                , this, &AutomatedRssDownloader::renameSelectedRule);
        }
        else
        {
            menu->addAction(UIThemeManager::instance()->getIcon(u"edit-clear"_s, u"list-remove"_s), tr("Delete selected rules")
                            , this, &AutomatedRssDownloader::onRemoveRuleBtnClicked);
        }

        menu->addSeparator();
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-clear"_s), tr("Clear downloaded episodes...")
            , this, &AutomatedRssDownloader::clearSelectedRuleDownloadedEpisodeList);
    }

    menu->popup(QCursor::pos());
}

void AutomatedRssDownloader::renameSelectedRule()
{
    const QList<QListWidgetItem *> selection = m_ui->ruleList->selectedItems();
    if (selection.isEmpty()) return;

    QListWidgetItem *item = selection.first();
    forever
    {
        QString newName = AutoExpandableDialog::getText(
                    this, tr("Rule renaming"), tr("Please type the new rule name")
                    , QLineEdit::Normal, item->text());
        newName = newName.trimmed();
        if (newName.isEmpty()) return;

        if (RSS::AutoDownloader::instance()->hasRule(newName))
        {
            QMessageBox::warning(this, tr("Rule name conflict")
                                 , tr("A rule with this name already exists, please choose another name."));
        }
        else
        {
            // Rename the rule
            RSS::AutoDownloader::instance()->renameRule(item->text(), newName);
            return;
        }
    }
}

void AutomatedRssDownloader::handleRuleCheckStateChange(QListWidgetItem *ruleItem)
{
    m_ui->ruleList->setCurrentItem(ruleItem);
}

void AutomatedRssDownloader::clearSelectedRuleDownloadedEpisodeList()
{
    const QMessageBox::StandardButton reply = QMessageBox::question(
                this,
                tr("Clear downloaded episodes"),
                tr("Are you sure you want to clear the list of downloaded episodes for the selected rule?"),
                QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        m_currentRule.setPreviouslyMatchedEpisodes(QStringList());
        handleRuleDefinitionChanged();
    }
}

void AutomatedRssDownloader::handleFeedCheckStateChange(QListWidgetItem *feedItem)
{
    const QString feedURL = feedItem->data(Qt::UserRole).toString();
    for (QListWidgetItem *ruleItem : asConst(m_ui->ruleList->selectedItems()))
    {
        RSS::AutoDownloadRule rule = (ruleItem == m_currentRuleItem
                                       ? m_currentRule
                                       : RSS::AutoDownloader::instance()->ruleByName(ruleItem->text()));
        QStringList affectedFeeds = rule.feedURLs();
        if ((feedItem->checkState() == Qt::Checked) && !affectedFeeds.contains(feedURL))
            affectedFeeds << feedURL;
        else if ((feedItem->checkState() == Qt::Unchecked) && affectedFeeds.contains(feedURL))
            affectedFeeds.removeOne(feedURL);

        rule.setFeedURLs(affectedFeeds);
        if (ruleItem != m_currentRuleItem)
            RSS::AutoDownloader::instance()->setRule(rule);
        else
            m_currentRule = rule;
    }

    handleRuleDefinitionChanged();
}

void AutomatedRssDownloader::updateMatchingArticles()
{
    m_ui->matchingArticlesTree->clear();

    for (const QListWidgetItem *ruleItem : asConst(m_ui->ruleList->selectedItems()))
    {
        RSS::AutoDownloadRule rule = (ruleItem == m_currentRuleItem
                                       ? m_currentRule
                                       : RSS::AutoDownloader::instance()->ruleByName(ruleItem->text()));
        for (const QString &feedURL : asConst(rule.feedURLs()))
        {
            auto *feed = RSS::Session::instance()->feedByURL(feedURL);
            if (!feed) continue; // feed doesn't exist

            QStringList matchingArticles;
            for (const auto *article : asConst(feed->articles()))
                if (rule.matches(article->data()))
                    matchingArticles << article->title();
            if (!matchingArticles.isEmpty())
                addFeedArticlesToTree(feed, matchingArticles);
        }
    }

    m_treeListEntries.clear();
}

void AutomatedRssDownloader::addFeedArticlesToTree(RSS::Feed *feed, const QStringList &articles)
{
    // Turn off sorting while inserting
    m_ui->matchingArticlesTree->setSortingEnabled(false);

    // Check if this feed is already in the tree
    QTreeWidgetItem *treeFeedItem = nullptr;
    for (int i = 0; i < m_ui->matchingArticlesTree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = m_ui->matchingArticlesTree->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toString() == feed->url())
        {
            treeFeedItem = item;
            break;
        }
    }

    // If there is none, create it
    if (!treeFeedItem)
    {
        treeFeedItem = new QTreeWidgetItem(QStringList() << feed->name());
        treeFeedItem->setToolTip(0, feed->name());
        QFont f = treeFeedItem->font(0);
        f.setBold(true);
        treeFeedItem->setFont(0, f);
        treeFeedItem->setData(0, Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"directory"_s));
        treeFeedItem->setData(0, Qt::UserRole, feed->url());
        m_ui->matchingArticlesTree->addTopLevelItem(treeFeedItem);
    }

    // Insert the articles
    for (const QString &article : articles)
    {
        const std::pair<QString, QString> key(feed->name(), article);

        if (!m_treeListEntries.contains(key))
        {
            m_treeListEntries << key;
            QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << article);
            item->setToolTip(0, article);
            treeFeedItem->addChild(item);
        }
    }

    m_ui->matchingArticlesTree->expandItem(treeFeedItem);
    m_ui->matchingArticlesTree->sortItems(0, Qt::AscendingOrder);
    m_ui->matchingArticlesTree->setSortingEnabled(true);
}

void AutomatedRssDownloader::updateFieldsToolTips(bool regex)
{
    QString tip;
    if (regex)
    {
        tip = u"<p>" + tr("Regex mode: use Perl-compatible regular expressions") + u"</p>";
    }
    else
    {
        tip = u"<p>" + tr("Wildcard mode: you can use") + u"<ul>"
              + u"<li>" + tr("? to match any single character") + u"</li>"
              + u"<li>" + tr("* to match zero or more of any characters") + u"</li>"
              + u"<li>" + tr("Whitespaces count as AND operators (all words, any order)") + u"</li>"
              + u"<li>" + tr("| is used as OR operator") + u"</li></ul></p>"
              + u"<p>" + tr("If word order is important use * instead of whitespace.") + u"</p>";
    }

    // Whether regex or wildcard, warn about a potential gotcha for users.
    // Explanatory string broken over multiple lines for readability (and multiple
    // statements to prevent uncrustify indenting excessively.
    tip += u"<p>";
    tip += tr("An expression with an empty %1 clause (e.g. %2)",
              "We talk about regex/wildcards in the RSS filters section here."
              " So a valid sentence would be: An expression with an empty | clause (e.g. expr|)"
              ).arg(u"<tt>|</tt>"_s, u"<tt>expr|</tt>"_s);
    m_ui->lineContains->setToolTip(tip + tr(" will match all articles.") + u"</p>");
    m_ui->lineNotContains->setToolTip(tip + tr(" will exclude all articles.") + u"</p>");
}

void AutomatedRssDownloader::updateMustLineValidity()
{
    const QString text = m_ui->lineContains->text();
    bool isRegex = m_ui->checkRegex->isChecked();
    bool valid = true;
    QString error;

    if (!text.isEmpty())
    {
        QStringList tokens;
        if (isRegex)
        {
            tokens << text;
        }
        else
        {
            for (const QString &token : asConst(text.split(u'|')))
                tokens << Utils::String::wildcardToRegexPattern(token);
        }

        for (const QString &token : asConst(tokens))
        {
            QRegularExpression reg(token, QRegularExpression::CaseInsensitiveOption);
            if (!reg.isValid())
            {
                if (isRegex)
                    error = tr("Position %1: %2").arg(reg.patternErrorOffset()).arg(reg.errorString());
                valid = false;
                break;
            }
        }
    }

    if (valid)
    {
        m_ui->lineContains->setStyleSheet({});
        m_ui->labelMustStat->setPixmap(QPixmap());
        m_ui->labelMustStat->setToolTip({});
    }
    else
    {
        m_ui->lineContains->setStyleSheet(u"QLineEdit { color: #ff0000; }"_s);
        m_ui->labelMustStat->setPixmap(UIThemeManager::instance()->getIcon(u"dialog-warning"_s, u"task-attention"_s).pixmap(16, 16));
        m_ui->labelMustStat->setToolTip(error);
    }
}

void AutomatedRssDownloader::updateMustNotLineValidity()
{
    const QString text = m_ui->lineNotContains->text();
    bool isRegex = m_ui->checkRegex->isChecked();
    bool valid = true;
    QString error;

    if (!text.isEmpty())
    {
        QStringList tokens;
        if (isRegex)
        {
            tokens << text;
        }
        else
        {
            for (const QString &token : asConst(text.split(u'|')))
                tokens << Utils::String::wildcardToRegexPattern(token);
        }

        for (const QString &token : asConst(tokens))
        {
            QRegularExpression reg(token, QRegularExpression::CaseInsensitiveOption);
            if (!reg.isValid())
            {
                if (isRegex)
                    error = tr("Position %1: %2").arg(reg.patternErrorOffset()).arg(reg.errorString());
                valid = false;
                break;
            }
        }
    }

    if (valid)
    {
        m_ui->lineNotContains->setStyleSheet({});
        m_ui->labelMustNotStat->setPixmap(QPixmap());
        m_ui->labelMustNotStat->setToolTip({});
    }
    else
    {
        m_ui->lineNotContains->setStyleSheet(u"QLineEdit { color: #ff0000; }"_s);
        m_ui->labelMustNotStat->setPixmap(UIThemeManager::instance()->getIcon(u"dialog-warning"_s, u"task-attention"_s).pixmap(16, 16));
        m_ui->labelMustNotStat->setToolTip(error);
    }
}

void AutomatedRssDownloader::updateEpisodeFilterValidity()
{
    const QString text = m_ui->lineEFilter->text();
    bool valid = text.isEmpty() || m_episodeRegex->match(text).hasMatch();

    if (valid)
    {
        m_ui->lineEFilter->setStyleSheet({});
        m_ui->labelEpFilterStat->setPixmap({});
    }
    else
    {
        m_ui->lineEFilter->setStyleSheet(u"QLineEdit { color: #ff0000; }"_s);
        m_ui->labelEpFilterStat->setPixmap(UIThemeManager::instance()->getIcon(u"dialog-warning"_s, u"task-attention"_s).pixmap(16, 16));
    }
}

void AutomatedRssDownloader::handleRuleDefinitionChanged()
{
    updateEditedRule();
    updateMatchingArticles();
}

void AutomatedRssDownloader::handleRuleAdded(const QString &ruleName)
{
    createRuleItem(RSS::AutoDownloadRule(ruleName));
}

void AutomatedRssDownloader::handleRuleRenamed(const QString &ruleName, const QString &oldRuleName)
{
    auto *item = m_itemsByRuleName.take(oldRuleName);
    m_itemsByRuleName.insert(ruleName, item);
    if (m_currentRule.name() == oldRuleName)
        m_currentRule.setName(ruleName);
    item->setText(ruleName);
}

void AutomatedRssDownloader::handleRuleChanged(const QString &ruleName)
{
    auto *item = m_itemsByRuleName.value(ruleName);
    if (item && (item != m_currentRuleItem))
        item->setCheckState(RSS::AutoDownloader::instance()->ruleByName(ruleName).isEnabled() ? Qt::Checked : Qt::Unchecked);
}

void AutomatedRssDownloader::handleRuleAboutToBeRemoved(const QString &ruleName)
{
    m_currentRuleItem = nullptr;
    delete m_itemsByRuleName.take(ruleName);
}

void AutomatedRssDownloader::handleProcessingStateChanged(bool enabled)
{
    m_ui->labelWarn->setVisible(!enabled);
}
