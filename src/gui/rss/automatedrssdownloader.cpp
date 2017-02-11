/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
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

#include <QCursor>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
#include <QRegularExpression>

#include "base/bittorrent/session.h"
#include "base/preferences.h"
#include "base/rss/rssdownloadrulelist.h"
#include "base/rss/rssmanager.h"
#include "base/rss/rssfolder.h"
#include "base/rss/rssfeed.h"
#include "base/utils/fs.h"
#include "base/utils/string.h"
#include "guiiconprovider.h"
#include "autoexpandabledialog.h"
#include "ui_automatedrssdownloader.h"
#include "automatedrssdownloader.h"

AutomatedRssDownloader::AutomatedRssDownloader(const QWeakPointer<Rss::Manager> &manager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AutomatedRssDownloader)
    , m_manager(manager)
    , m_editedRule(0)
{
    ui->setupUi(this);
    // Icons
    ui->removeRuleBtn->setIcon(GuiIconProvider::instance()->getIcon("list-remove"));
    ui->addRuleBtn->setIcon(GuiIconProvider::instance()->getIcon("list-add"));

    // Ui Settings
    ui->listRules->setSortingEnabled(true);
    ui->listRules->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->treeMatchingArticles->setSortingEnabled(true);
    ui->treeMatchingArticles->sortByColumn(0, Qt::AscendingOrder);
    ui->hsplitter->setCollapsible(0, false);
    ui->hsplitter->setCollapsible(1, false);
    ui->hsplitter->setCollapsible(2, true); // Only the preview list is collapsible
    bool ok; Q_UNUSED(ok);
    ok = connect(ui->checkRegex, SIGNAL(toggled(bool)), SLOT(updateFieldsToolTips(bool)));
    Q_ASSERT(ok);
    ok = connect(ui->listRules, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayRulesListMenu(const QPoint&)));
    Q_ASSERT(ok);
    m_ruleList = manager.toStrongRef()->downloadRules();
    m_editableRuleList = new Rss::DownloadRuleList; // Read rule list from disk
    m_episodeRegex = new QRegularExpression("^(^\\d{1,4}x(\\d{1,4}(-(\\d{1,4})?)?;){1,}){1,1}",
                                 QRegularExpression::CaseInsensitiveOption);
    QString tip = "<p>" + tr("Matches articles based on episode filter.") + "</p><p><b>" + tr("Example: ")
                  + "1x2;8-15;5;30-;</b>" + tr(" will match 2, 5, 8 through 15, 30 and onward episodes of season one", "example X will match") + "</p>";
    tip += "<p>" + tr("Episode filter rules: ") + "</p><ul><li>" + tr("Season number is a mandatory non-zero value") + "</li>"
           + "<li>" + tr("Episode number is a mandatory positive value") + "</li>"
           + "<li>" + tr("Filter must end with semicolon") + "</li>"
           + "<li>" + tr("Three range types for episodes are supported: ") + "</li>" + "<li><ul>"
           "<li>" + tr("Single number: <b>1x25;</b> matches episode 25 of season one") + "</li>"
           + "<li>" + tr("Normal range: <b>1x25-40;</b> matches episodes 25 through 40 of season one") + "</li>"
           + "<li>" + tr("Infinite range: <b>1x25-;</b> matches episodes 25 and upward of season one, and all episodes of later seasons") + "</li>" + "</ul></li></ul>";
    ui->lineEFilter->setToolTip(tip);
    initCategoryCombobox();
    loadSettings();
    // Update matching articles when necessary
    ok = connect(ui->lineContains, SIGNAL(textEdited(QString)), SLOT(updateMatchingArticles()));
    Q_ASSERT(ok);
    ok = connect(ui->lineContains, SIGNAL(textEdited(QString)), SLOT(updateMustLineValidity()));
    Q_ASSERT(ok);
    ok = connect(ui->lineNotContains, SIGNAL(textEdited(QString)), SLOT(updateMatchingArticles()));
    Q_ASSERT(ok);
    ok = connect(ui->lineNotContains, SIGNAL(textEdited(QString)), SLOT(updateMustNotLineValidity()));
    Q_ASSERT(ok);
    ok = connect(ui->lineEFilter, SIGNAL(textEdited(QString)), SLOT(updateEpisodeFilterValidity()));
    Q_ASSERT(ok);
    ok = connect(ui->checkRegex, SIGNAL(stateChanged(int)), SLOT(updateMatchingArticles()));
    Q_ASSERT(ok);
    ok = connect(ui->checkRegex, SIGNAL(stateChanged(int)), SLOT(updateMustLineValidity()));
    Q_ASSERT(ok);
    ok = connect(ui->checkRegex, SIGNAL(stateChanged(int)), SLOT(updateMustNotLineValidity()));
    Q_ASSERT(ok);
    ok = connect(this, SIGNAL(finished(int)), SLOT(onFinished(int)));
    Q_ASSERT(ok);
    ok = connect(ui->lineEFilter, SIGNAL(textEdited(QString)), SLOT(updateMatchingArticles()));
    Q_ASSERT(ok);
    editHotkey = new QShortcut(Qt::Key_F2, ui->listRules, 0, 0, Qt::WidgetShortcut);
    ok = connect(editHotkey, SIGNAL(activated()), SLOT(renameSelectedRule()));
    Q_ASSERT(ok);
    ok = connect(ui->listRules, SIGNAL(doubleClicked(QModelIndex)), SLOT(renameSelectedRule()));
    Q_ASSERT(ok);
    deleteHotkey = new QShortcut(QKeySequence::Delete, ui->listRules, 0, 0, Qt::WidgetShortcut);
    ok = connect(deleteHotkey, SIGNAL(activated()), SLOT(on_removeRuleBtn_clicked()));
    Q_ASSERT(ok);

    updateRuleDefinitionBox();
}

AutomatedRssDownloader::~AutomatedRssDownloader()
{
    qDebug() << Q_FUNC_INFO;
    delete editHotkey;
    delete deleteHotkey;
    delete ui;
    delete m_editableRuleList;
    delete m_episodeRegex;
}

void AutomatedRssDownloader::connectRuleFeedSlots()
{
    qDebug() << Q_FUNC_INFO << "Connecting rule and feed slots";
    connect(ui->listRules, SIGNAL(itemSelectionChanged()), this, SLOT(updateRuleDefinitionBox()), Qt::UniqueConnection);
    connect(ui->listRules, SIGNAL(itemChanged(QListWidgetItem *)), this, SLOT(handleRuleCheckStateChange(QListWidgetItem *)), Qt::UniqueConnection);
    connect(ui->listFeeds, SIGNAL(itemChanged(QListWidgetItem *)), this, SLOT(handleFeedCheckStateChange(QListWidgetItem *)), Qt::UniqueConnection);
}

void AutomatedRssDownloader::disconnectRuleFeedSlots()
{
    qDebug() << Q_FUNC_INFO << "Disconnecting rule and feed slots";
    disconnect(ui->listRules, SIGNAL(itemSelectionChanged()), this, SLOT(updateRuleDefinitionBox()));
    disconnect(ui->listRules, SIGNAL(itemChanged(QListWidgetItem *)), this, SLOT(handleRuleCheckStateChange(QListWidgetItem *)));
    disconnect(ui->listFeeds, SIGNAL(itemChanged(QListWidgetItem *)), this, SLOT(handleFeedCheckStateChange(QListWidgetItem *)));
}

void AutomatedRssDownloader::loadSettings()
{
    // load dialog geometry
    const Preferences *const pref = Preferences::instance();
    restoreGeometry(pref->getRssGeometry());
    ui->checkEnableDownloader->setChecked(pref->isRssDownloadingEnabled());
    ui->hsplitter->restoreState(pref->getRssHSplitterSizes());
    // Display download rules
    loadRulesList();
}

void AutomatedRssDownloader::saveSettings()
{
    Preferences::instance()->setRssDownloadingEnabled(ui->checkEnableDownloader->isChecked());
    // Save dialog geometry
    Preferences *const pref = Preferences::instance();
    pref->setRssGeometry(saveGeometry());
    pref->setRssHSplitterSizes(ui->hsplitter->saveState());
}

void AutomatedRssDownloader::loadRulesList()
{
    // Make sure we save the current item before clearing
    saveEditedRule();
    ui->listRules->clear();

    foreach (const QString &rule_name, m_editableRuleList->ruleNames()) {
        QListWidgetItem *item = new QListWidgetItem(rule_name, ui->listRules);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        if (m_editableRuleList->getRule(rule_name)->isEnabled())
            item->setCheckState(Qt::Checked);
        else
            item->setCheckState(Qt::Unchecked);
    }
}

void AutomatedRssDownloader::loadFeedList()
{
    disconnectRuleFeedSlots();

    const Preferences *const pref = Preferences::instance();
    const QStringList feed_aliases = pref->getRssFeedsAliases();
    const QStringList feed_urls = pref->getRssFeedsUrls();
    ui->listFeeds->clear();

    for (int i = 0; i < feed_aliases.size(); ++i) {
        QString feed_url = feed_urls.at(i);
        feed_url = feed_url.split("\\").last();
        qDebug() << Q_FUNC_INFO << feed_url;

        QListWidgetItem *item = new QListWidgetItem(feed_aliases.at(i), ui->listFeeds);
        item->setData(Qt::UserRole, feed_url);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsTristate);
    }

    // Reconnects slots
    updateFeedList();
}

void AutomatedRssDownloader::updateFeedList(QListWidgetItem *selected)
{
    disconnectRuleFeedSlots();

    QList<QListWidgetItem *> selection;

    if (selected)
        selection << selected;
    else
        selection = ui->listRules->selectedItems();

    bool enable = !selection.isEmpty();

    for (int i = 0; i<ui->listFeeds->count(); ++i) {
        QListWidgetItem *item = ui->listFeeds->item(i);
        const QString feed_url = item->data(Qt::UserRole).toString();
        item->setHidden(!enable);

        bool allEnabled = true;
        bool anyEnabled = false;

        foreach (const QListWidgetItem *ruleItem, selection) {
            Rss::DownloadRulePtr rule = m_editableRuleList->getRule(ruleItem->text());
            if (!rule) continue;
            qDebug() << "Rule" << rule->name() << "affects" << rule->rssFeeds().size() << "feeds.";
            foreach (QString test, rule->rssFeeds())
                qDebug() << "Feed is " << test;
            if (rule->rssFeeds().contains(feed_url)) {
                qDebug() << "Rule " << rule->name() << " affects feed " << feed_url;
                anyEnabled = true;
            }
            else {
                qDebug() << "Rule " << rule->name() << " does NOT affect feed " << feed_url;
                allEnabled = false;
            }
        }

        if (anyEnabled && allEnabled)
            item->setCheckState(Qt::Checked);
        else if (anyEnabled)
            item->setCheckState(Qt::PartiallyChecked);
        else
            item->setCheckState(Qt::Unchecked);
    }

    ui->listFeeds->sortItems();
    ui->lblListFeeds->setEnabled(enable);
    ui->listFeeds->setEnabled(enable);

    if (selected) {
        m_editedRule = selected;
        ui->listRules->clearSelection();
        ui->listRules->setCurrentItem(selected);
    }

    connectRuleFeedSlots();
    updateMatchingArticles();
}

bool AutomatedRssDownloader::isRssDownloaderEnabled() const
{
    return ui->checkEnableDownloader->isChecked();
}

void AutomatedRssDownloader::updateRuleDefinitionBox(QListWidgetItem *selected)
{
    disconnectRuleFeedSlots();

    qDebug() << Q_FUNC_INFO;
    // Save previous rule first
    saveEditedRule();
    // Update rule definition box
    const QList<QListWidgetItem *> selection = ui->listRules->selectedItems();

    if (!selected && (selection.count() == 1))
        selected = selection.first();

    if (selected) {
        m_editedRule = selected;

        // Cannot call getCurrentRule() here as the current item hasn't been updated yet
        // and we could get the details from the wrong rule.
        // Also can't set the current item here or the selected items gets messed up.
        Rss::DownloadRulePtr rule = m_editableRuleList->getRule(m_editedRule->text());

        if (rule) {
            ui->lineContains->setText(rule->mustContain());
            ui->lineNotContains->setText(rule->mustNotContain());
            QString ep = rule->episodeFilter();
            if (!ep.isEmpty())
                ui->lineEFilter->setText(ep);
            else
                ui->lineEFilter->clear();
            ui->saveDiffDir_check->setChecked(!rule->savePath().isEmpty());
            ui->lineSavePath->setText(Utils::Fs::toNativePath(rule->savePath()));
            ui->checkRegex->blockSignals(true);
            ui->checkRegex->setChecked(rule->useRegex());
            ui->checkRegex->blockSignals(false);
            ui->comboCategory->setCurrentIndex(ui->comboCategory->findText(rule->category()));
            if (rule->category().isEmpty())
                ui->comboCategory->clearEditText();
            ui->comboAddPaused->setCurrentIndex(rule->addPaused());
            ui->spinIgnorePeriod->setValue(rule->ignoreDays());
            QDateTime dateTime = rule->lastMatch();
            QString lMatch;
            if (dateTime.isValid())
                lMatch = tr("Last Match: %1 days ago").arg(dateTime.daysTo(QDateTime::currentDateTime()));
            else
                lMatch = tr("Last Match: Unknown");
            ui->lblLastMatch->setText(lMatch);
            updateMustLineValidity();
            updateMustNotLineValidity();
            updateEpisodeFilterValidity();
        }
        else {
            // New rule
            clearRuleDefinitionBox();
            ui->comboAddPaused->setCurrentIndex(0);
            ui->comboCategory->setCurrentIndex(0);
            ui->spinIgnorePeriod->setValue(0);
        }

        updateFieldsToolTips(ui->checkRegex->isChecked());
        ui->ruleDefBox->setEnabled(true);
    }
    else {
        m_editedRule = 0;
        clearRuleDefinitionBox();
        ui->ruleDefBox->setEnabled(false);
    }

    // Reconnects slots
    updateFeedList(selected);
}

void AutomatedRssDownloader::clearRuleDefinitionBox()
{
    ui->lineContains->clear();
    ui->lineNotContains->clear();
    ui->lineEFilter->clear();
    ui->saveDiffDir_check->setChecked(false);
    ui->lineSavePath->clear();
    ui->comboCategory->clearEditText();
    ui->comboCategory->setCurrentIndex(-1);
    ui->checkRegex->setChecked(false);
    ui->spinIgnorePeriod->setValue(0);
    ui->comboAddPaused->clearEditText();
    ui->comboAddPaused->setCurrentIndex(-1);
    updateFieldsToolTips(ui->checkRegex->isChecked());
    updateMustLineValidity();
    updateMustNotLineValidity();
    updateEpisodeFilterValidity();
}

Rss::DownloadRulePtr AutomatedRssDownloader::getCurrentRule() const
{
    QListWidgetItem *current_item = ui->listRules->currentItem();
    if (current_item)
        return m_editableRuleList->getRule(current_item->text());
    return Rss::DownloadRulePtr();
}

void AutomatedRssDownloader::initCategoryCombobox()
{
    // Load torrent categories
    QStringList categories = BitTorrent::Session::instance()->categories();
    std::sort(categories.begin(), categories.end(), Utils::String::naturalCompareCaseInsensitive);
    ui->comboCategory->addItem(QString(""));
    ui->comboCategory->addItems(categories);
}

void AutomatedRssDownloader::saveEditedRule()
{
    if (!m_editedRule || !ui->ruleDefBox->isEnabled()) return;

    qDebug() << Q_FUNC_INFO << m_editedRule;
    if (ui->listRules->findItems(m_editedRule->text(), Qt::MatchExactly).isEmpty()) {
        qDebug() << "Could not find rule" << m_editedRule->text() << "in the UI list";
        qDebug() << "Probably removed the item, no need to save it";
        return;
    }
    Rss::DownloadRulePtr rule = m_editableRuleList->getRule(m_editedRule->text());
    if (!rule) {
        rule = Rss::DownloadRulePtr(new Rss::DownloadRule);
        rule->setName(m_editedRule->text());
    }
    if (m_editedRule->checkState() == Qt::Unchecked)
        rule->setEnabled(false);
    else
        rule->setEnabled(true);
    rule->setUseRegex(ui->checkRegex->isChecked());
    rule->setMustContain(ui->lineContains->text());
    rule->setMustNotContain(ui->lineNotContains->text());
    rule->setEpisodeFilter(ui->lineEFilter->text());
    if (ui->saveDiffDir_check->isChecked())
        rule->setSavePath(ui->lineSavePath->text());
    else
        rule->setSavePath("");
    rule->setCategory(ui->comboCategory->currentText());

    rule->setAddPaused(Rss::DownloadRule::AddPausedState(ui->comboAddPaused->currentIndex()));
    rule->setIgnoreDays(ui->spinIgnorePeriod->value());
    // rule->setRssFeeds(getSelectedFeeds());
    // Save it
    m_editableRuleList->saveRule(rule);
}

void AutomatedRssDownloader::on_addRuleBtn_clicked()
{
    saveEditedRule();

    // Ask for a rule name
    const QString rule_name = AutoExpandableDialog::getText(this, tr("New rule name"), tr("Please type the name of the new download rule."));
    if (rule_name.isEmpty()) return;
    // Check if this rule name already exists
    if (m_editableRuleList->getRule(rule_name)) {
        QMessageBox::warning(this, tr("Rule name conflict"), tr("A rule with this name already exists, please choose another name."));
        return;
    }

    disconnectRuleFeedSlots();

    // Add the new rule to the list
    QListWidgetItem *item = new QListWidgetItem(rule_name, ui->listRules);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsTristate);
    item->setCheckState(Qt::Checked); // Enable as a default
    m_editedRule = 0;

    // Reconnects slots
    updateRuleDefinitionBox(item);
}

void AutomatedRssDownloader::on_removeRuleBtn_clicked()
{
    const QList<QListWidgetItem *> selection = ui->listRules->selectedItems();
    if (selection.isEmpty()) return;
    // Ask for confirmation
    QString confirm_text;
    if (selection.count() == 1)
        confirm_text = tr("Are you sure you want to remove the download rule named '%1'?").arg(selection.first()->text());
    else
        confirm_text = tr("Are you sure you want to remove the selected download rules?");
    if (QMessageBox::question(this, tr("Rule deletion confirmation"), confirm_text, QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
        return;

    disconnectRuleFeedSlots();

    foreach (QListWidgetItem *item, selection) {
        // Actually remove the item
        ui->listRules->removeItemWidget(item);
        const QString rule_name = item->text();
        // Clean up memory
        delete item;
        qDebug("Removed item for the UI list");
        // Remove it from the m_editableRuleList
        m_editableRuleList->removeRule(rule_name);
    }

    m_editedRule = 0;
    // Reconnects slots
    updateRuleDefinitionBox();
}

void AutomatedRssDownloader::on_browseSP_clicked()
{
    QString save_path = QFileDialog::getExistingDirectory(this, tr("Destination directory"), QDir::homePath());
    if (!save_path.isEmpty())
        ui->lineSavePath->setText(Utils::Fs::toNativePath(save_path));
}

void AutomatedRssDownloader::on_exportBtn_clicked()
{
    if (m_editableRuleList->isEmpty()) {
        QMessageBox::warning(this, tr("Invalid action"), tr("The list is empty, there is nothing to export."));
        return;
    }
    // Ask for a save path
    QString save_path = QFileDialog::getSaveFileName(this, tr("Where would you like to save the list?"), QDir::homePath(), tr("Rules list (*.rssrules)"));
    if (save_path.isEmpty()) return;
    if (!save_path.endsWith(".rssrules", Qt::CaseInsensitive))
        save_path += ".rssrules";
    if (!m_editableRuleList->serialize(save_path)) {
        QMessageBox::warning(this, tr("I/O Error"), tr("Failed to create the destination file"));
        return;
    }
}

void AutomatedRssDownloader::on_importBtn_clicked()
{
    // Ask for filter path
    QString load_path = QFileDialog::getOpenFileName(this, tr("Please point to the RSS download rules file"), QDir::homePath(), tr("Rules list") + QString(" (*.rssrules *.filters)"));
    if (load_path.isEmpty() || !QFile::exists(load_path)) return;
    // Load it
    if (!m_editableRuleList->unserialize(load_path)) {
        QMessageBox::warning(this, tr("Import Error"), tr("Failed to import the selected rules file"));
        return;
    }
    // Reload the rule list
    loadRulesList();
}

void AutomatedRssDownloader::displayRulesListMenu(const QPoint &pos)
{
    Q_UNUSED(pos);
    QMenu menu;
    QAction *addAct = menu.addAction(GuiIconProvider::instance()->getIcon("list-add"), tr("Add new rule..."));
    QAction *delAct = 0;
    QAction *renameAct = 0;
    const QList<QListWidgetItem *> selection = ui->listRules->selectedItems();
    if (!selection.isEmpty()) {
        if (selection.count() == 1) {
            delAct = menu.addAction(GuiIconProvider::instance()->getIcon("list-remove"), tr("Delete rule"));
            menu.addSeparator();
            renameAct = menu.addAction(GuiIconProvider::instance()->getIcon("edit-rename"), tr("Rename rule..."));
        }
        else {
            delAct = menu.addAction(GuiIconProvider::instance()->getIcon("list-remove"), tr("Delete selected rules"));
        }
    }
    QAction *act = menu.exec(QCursor::pos());
    if (!act) return;
    if (act == addAct) {
        on_addRuleBtn_clicked();
        return;
    }
    if (act == delAct) {
        on_removeRuleBtn_clicked();
        return;
    }
    if (act == renameAct) {
        renameSelectedRule();
        return;
    }
}

void AutomatedRssDownloader::renameSelectedRule()
{
    const QList<QListWidgetItem *> selection = ui->listRules->selectedItems();
    if (selection.isEmpty())
        return;

    QListWidgetItem *item = selection.first();
    forever {
        QString new_name = AutoExpandableDialog::getText(this, tr("Rule renaming"), tr("Please type the new rule name"), QLineEdit::Normal, item->text());
        new_name = new_name.trimmed();
        if (new_name.isEmpty()) return;
        if (m_editableRuleList->ruleNames().contains(new_name, Qt::CaseInsensitive)) {
            QMessageBox::warning(this, tr("Rule name conflict"), tr("A rule with this name already exists, please choose another name."));
        }
        else {
            // Rename the rule
            m_editableRuleList->renameRule(item->text(), new_name);
            item->setText(new_name);
            return;
        }
    }
}

void AutomatedRssDownloader::handleRuleCheckStateChange(QListWidgetItem *rule_item)
{
    // Make sure the current rule is saved
    saveEditedRule();

    // Make sure we save the rule that was enabled or disabled - it might not be the current selection.
    m_editedRule = rule_item;
    saveEditedRule();
    updateRuleDefinitionBox();
}

void AutomatedRssDownloader::handleFeedCheckStateChange(QListWidgetItem *feed_item)
{
    // Make sure the current rule is saved
    saveEditedRule();
    const QString feed_url = feed_item->data(Qt::UserRole).toString();
    foreach (QListWidgetItem *rule_item, ui->listRules->selectedItems()) {
        Rss::DownloadRulePtr rule = m_editableRuleList->getRule(rule_item->text());
        Q_ASSERT(rule);
        QStringList affected_feeds = rule->rssFeeds();
        if ((feed_item->checkState() == Qt::Checked) && !affected_feeds.contains(feed_url))
            affected_feeds << feed_url;
        else if ((feed_item->checkState() == Qt::Unchecked) && affected_feeds.contains(feed_url))
            affected_feeds.removeOne(feed_url);
        // Save the updated rule
        if (affected_feeds.size() != rule->rssFeeds().size()) {
            rule->setRssFeeds(affected_feeds);
            m_editableRuleList->saveRule(rule);
        }
    }
    // Update Matching articles
    updateMatchingArticles();
}

void AutomatedRssDownloader::updateMatchingArticles()
{
    ui->treeMatchingArticles->clear();
    Rss::ManagerPtr manager = m_manager.toStrongRef();
    if (!manager)
        return;
    const QHash<QString, Rss::FeedPtr> all_feeds = manager->rootFolder()->getAllFeedsAsHash();

    saveEditedRule();
    foreach (const QListWidgetItem *rule_item, ui->listRules->selectedItems()) {
        Rss::DownloadRulePtr rule = m_editableRuleList->getRule(rule_item->text());
        if (!rule) continue;
        foreach (const QString &feed_url, rule->rssFeeds()) {
            qDebug() << Q_FUNC_INFO << feed_url;
            if (!all_feeds.contains(feed_url)) continue; // Feed was removed
            Rss::FeedPtr feed = all_feeds.value(feed_url);
            Q_ASSERT(feed);
            if (!feed) continue;
            const QStringList matching_articles = rule->findMatchingArticles(feed);
            if (!matching_articles.isEmpty())
                addFeedArticlesToTree(feed, matching_articles);
        }
    }

    m_treeListEntries.clear();
}

void AutomatedRssDownloader::addFeedArticlesToTree(const Rss::FeedPtr &feed, const QStringList &articles)
{
    // Turn off sorting while inserting
    ui->treeMatchingArticles->setSortingEnabled(false);

    // Check if this feed is already in the tree
    QTreeWidgetItem *treeFeedItem = 0;
    for (int i = 0; i<ui->treeMatchingArticles->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = ui->treeMatchingArticles->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toString() == feed->url()) {
            treeFeedItem = item;
            break;
        }
    }

    // If there is none, create it
    if (!treeFeedItem) {
        treeFeedItem = new QTreeWidgetItem(QStringList() << feed->displayName());
        treeFeedItem->setToolTip(0, feed->displayName());
        QFont f = treeFeedItem->font(0);
        f.setBold(true);
        treeFeedItem->setFont(0, f);
        treeFeedItem->setData(0, Qt::DecorationRole, GuiIconProvider::instance()->getIcon("inode-directory"));
        treeFeedItem->setData(0, Qt::UserRole, feed->url());
        ui->treeMatchingArticles->addTopLevelItem(treeFeedItem);
    }

    // Insert the articles
    foreach (const QString &art, articles) {
        QPair<QString, QString> key(feed->displayName(), art);

        if (!m_treeListEntries.contains(key)) {
            m_treeListEntries << key;
            QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << art);
            item->setToolTip(0, art);
            treeFeedItem->addChild(item);
        }
    }

    ui->treeMatchingArticles->expandItem(treeFeedItem);
    ui->treeMatchingArticles->sortItems(0, Qt::AscendingOrder);
    ui->treeMatchingArticles->setSortingEnabled(true);
}

void AutomatedRssDownloader::updateFieldsToolTips(bool regex)
{
    QString tip;
    if (regex) {
        tip = "<p>" + tr("Regex mode: use Perl-compatible regular expressions") + "</p>";
    }
    else {
        tip = "<p>" + tr("Wildcard mode: you can use") + "<ul>"
              + "<li>" + tr("? to match any single character") + "</li>"
              + "<li>" + tr("* to match zero or more of any characters") + "</li>"
              + "<li>" + tr("Whitespaces count as AND operators (all words, any order)") + "</li>"
              + "<li>" + tr("| is used as OR operator") + "</li></ul></p>"
              + "<p>" + tr("If word order is important use * instead of whitespace.") + "</p>";
    }

    // Whether regex or wildcard, warn about a potential gotcha for users.
    // Explanatory string broken over multiple lines for readability (and multiple
    // statements to prevent uncrustify indenting excessively.
    tip += "<p>";
    tip += tr("An expression with an empty %1 clause (e.g. %2)",
              "We talk about regex/wildcards in the RSS filters section here."
              " So a valid sentence would be: An expression with an empty | clause (e.g. expr|)"
              ).arg("<tt>|</tt>").arg("<tt>expr|</tt>");
    ui->lineContains->setToolTip(tip + tr(" will match all articles.") + "</p>");
    ui->lineNotContains->setToolTip(tip + tr(" will exclude all articles.") + "</p>");
}

void AutomatedRssDownloader::updateMustLineValidity()
{
    const QString text = ui->lineContains->text();
    bool isRegex = ui->checkRegex->isChecked();
    bool valid = true;
    QString error;

    if (!text.isEmpty()) {
        QStringList tokens;
        if (isRegex)
            tokens << text;
        else
            foreach (const QString &token, text.split("|"))
                tokens << Utils::String::wildcardToRegex(token);

        foreach (const QString &token, tokens) {
            QRegularExpression reg(token, QRegularExpression::CaseInsensitiveOption);
            if (!reg.isValid()) {
                if (isRegex)
                    error = tr("Position %1: %2").arg(reg.patternErrorOffset()).arg(reg.errorString());
                valid = false;
                break;
            }
        }
    }

    if (valid) {
        ui->lineContains->setStyleSheet("");
        ui->lbl_must_stat->setPixmap(QPixmap());
        ui->lbl_must_stat->setToolTip(QLatin1String(""));
    }
    else {
        ui->lineContains->setStyleSheet("QLineEdit { color: #ff0000; }");
        ui->lbl_must_stat->setPixmap(GuiIconProvider::instance()->getIcon("task-attention").pixmap(16, 16));
        ui->lbl_must_stat->setToolTip(error);
    }
}

void AutomatedRssDownloader::updateMustNotLineValidity()
{
    const QString text = ui->lineNotContains->text();
    bool isRegex = ui->checkRegex->isChecked();
    bool valid = true;
    QString error;

    if (!text.isEmpty()) {
        QStringList tokens;
        if (isRegex)
            tokens << text;
        else
            foreach (const QString &token, text.split("|"))
                tokens << Utils::String::wildcardToRegex(token);

        foreach (const QString &token, tokens) {
            QRegularExpression reg(token, QRegularExpression::CaseInsensitiveOption);
            if (!reg.isValid()) {
                if (isRegex)
                    error = tr("Position %1: %2").arg(reg.patternErrorOffset()).arg(reg.errorString());
                valid = false;
                break;
            }
        }
    }

    if (valid) {
        ui->lineNotContains->setStyleSheet("");
        ui->lbl_mustnot_stat->setPixmap(QPixmap());
        ui->lbl_mustnot_stat->setToolTip(QLatin1String(""));
    }
    else {
        ui->lineNotContains->setStyleSheet("QLineEdit { color: #ff0000; }");
        ui->lbl_mustnot_stat->setPixmap(GuiIconProvider::instance()->getIcon("task-attention").pixmap(16, 16));
        ui->lbl_mustnot_stat->setToolTip(error);
    }
}

void AutomatedRssDownloader::updateEpisodeFilterValidity()
{
    const QString text = ui->lineEFilter->text();
    bool valid = text.isEmpty() || m_episodeRegex->match(text).hasMatch();

    if (valid) {
        ui->lineEFilter->setStyleSheet("");
        ui->lbl_epfilter_stat->setPixmap(QPixmap());
    }
    else {
        ui->lineEFilter->setStyleSheet("QLineEdit { color: #ff0000; }");
        ui->lbl_epfilter_stat->setPixmap(GuiIconProvider::instance()->getIcon("task-attention").pixmap(16, 16));
    }
}

void AutomatedRssDownloader::showEvent(QShowEvent *event)
{
    // Connects the signals and slots
    loadFeedList();
    QDialog::showEvent(event);
}

void AutomatedRssDownloader::hideEvent(QHideEvent *event)
{
    disconnectRuleFeedSlots();
    QDialog::hideEvent(event);
}

void AutomatedRssDownloader::onFinished(int result)
{
    Q_UNUSED(result);
    disconnectRuleFeedSlots();

    // Save current item on exit
    saveEditedRule();
    ui->listRules->clearSelection();
    m_ruleList->replace(m_editableRuleList);
    m_ruleList->saveRulesToStorage();
    saveSettings();

    m_treeListEntries.clear();
    ui->treeMatchingArticles->clear();
}
