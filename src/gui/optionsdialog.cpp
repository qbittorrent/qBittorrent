/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "optionsdialog.h"

#include <cstdlib>
#include <limits>

#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QTranslator>

#include "base/bittorrent/session.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/net/dnsupdater.h"
#include "base/net/portforwarder.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/preferences.h"
#include "base/rss/rss_autodownloader.h"
#include "base/rss/rss_session.h"
#include "base/torrentfileguard.h"
#include "base/torrentfileswatcher.h"
#include "base/unicodestrings.h"
#include "base/utils/fs.h"
#include "base/utils/net.h"
#include "base/utils/password.h"
#include "base/utils/random.h"
#include "addnewtorrentdialog.h"
#include "advancedsettings.h"
#include "app/application.h"
#include "banlistoptionsdialog.h"
#include "ipsubnetwhitelistoptionsdialog.h"
#include "rss/automatedrssdownloader.h"
#include "ui_optionsdialog.h"
#include "uithememanager.h"
#include "utils.h"
#include "watchedfolderoptionsdialog.h"
#include "watchedfoldersmodel.h"

#define SETTINGS_KEY(name) "OptionsDialog/" name

namespace
{
    QStringList translatedWeekdayNames()
    {
        // return translated strings from Monday to Sunday in user selected locale

        const QLocale locale {Preferences::instance()->getLocale()};
        const QDate date {2018, 11, 5};  // Monday
        QStringList ret;
        for (int i = 0; i < 7; ++i)
            ret.append(locale.toString(date.addDays(i), "dddd"));
        return ret;
    }

    QString languageToLocalizedString(const QLocale &locale)
    {
        switch (locale.language())
        {
        case QLocale::Arabic: return QString::fromUtf8(C_LOCALE_ARABIC);
        case QLocale::Armenian: return QString::fromUtf8(C_LOCALE_ARMENIAN);
        case QLocale::Azerbaijani: return QString::fromUtf8(C_LOCALE_AZERBAIJANI);
        case QLocale::Basque: return QString::fromUtf8(C_LOCALE_BASQUE);
        case QLocale::Bulgarian: return QString::fromUtf8(C_LOCALE_BULGARIAN);
        case QLocale::Byelorussian: return QString::fromUtf8(C_LOCALE_BYELORUSSIAN);
        case QLocale::Catalan: return QString::fromUtf8(C_LOCALE_CATALAN);
        case QLocale::Chinese:
            switch (locale.country())
            {
            case QLocale::China: return QString::fromUtf8(C_LOCALE_CHINESE_SIMPLIFIED);
            case QLocale::HongKong: return QString::fromUtf8(C_LOCALE_CHINESE_TRADITIONAL_HK);
            default: return QString::fromUtf8(C_LOCALE_CHINESE_TRADITIONAL_TW);
            }
        case QLocale::Croatian: return QString::fromUtf8(C_LOCALE_CROATIAN);
        case QLocale::Czech: return QString::fromUtf8(C_LOCALE_CZECH);
        case QLocale::Danish: return QString::fromUtf8(C_LOCALE_DANISH);
        case QLocale::Dutch: return QString::fromUtf8(C_LOCALE_DUTCH);
        case QLocale::English:
            switch (locale.country())
            {
            case QLocale::Australia: return QString::fromUtf8(C_LOCALE_ENGLISH_AUSTRALIA);
            case QLocale::UnitedKingdom: return QString::fromUtf8(C_LOCALE_ENGLISH_UNITEDKINGDOM);
            default: return QString::fromUtf8(C_LOCALE_ENGLISH);
            }
        case QLocale::Estonian: return QString::fromUtf8(C_LOCALE_ESTONIAN);
        case QLocale::Finnish: return QString::fromUtf8(C_LOCALE_FINNISH);
        case QLocale::French: return QString::fromUtf8(C_LOCALE_FRENCH);
        case QLocale::Galician: return QString::fromUtf8(C_LOCALE_GALICIAN);
        case QLocale::Georgian: return QString::fromUtf8(C_LOCALE_GEORGIAN);
        case QLocale::German: return QString::fromUtf8(C_LOCALE_GERMAN);
        case QLocale::Greek: return QString::fromUtf8(C_LOCALE_GREEK);
        case QLocale::Hebrew: return QString::fromUtf8(C_LOCALE_HEBREW);
        case QLocale::Hindi: return QString::fromUtf8(C_LOCALE_HINDI);
        case QLocale::Hungarian: return QString::fromUtf8(C_LOCALE_HUNGARIAN);
        case QLocale::Icelandic: return QString::fromUtf8(C_LOCALE_ICELANDIC);
        case QLocale::Indonesian: return QString::fromUtf8(C_LOCALE_INDONESIAN);
        case QLocale::Italian: return QString::fromUtf8(C_LOCALE_ITALIAN);
        case QLocale::Japanese: return QString::fromUtf8(C_LOCALE_JAPANESE);
        case QLocale::Korean: return QString::fromUtf8(C_LOCALE_KOREAN);
        case QLocale::Latvian: return QString::fromUtf8(C_LOCALE_LATVIAN);
        case QLocale::Lithuanian: return QString::fromUtf8(C_LOCALE_LITHUANIAN);
        case QLocale::Malay: return QString::fromUtf8(C_LOCALE_MALAY);
        case QLocale::Mongolian: return QString::fromUtf8(C_LOCALE_MONGOLIAN);
        case QLocale::NorwegianBokmal: return QString::fromUtf8(C_LOCALE_NORWEGIAN);
        case QLocale::Occitan: return QString::fromUtf8(C_LOCALE_OCCITAN);
        case QLocale::Persian: return QString::fromUtf8(C_LOCALE_PERSIAN);
        case QLocale::Polish: return QString::fromUtf8(C_LOCALE_POLISH);
        case QLocale::Portuguese:
            if (locale.country() == QLocale::Brazil)
                return QString::fromUtf8(C_LOCALE_PORTUGUESE_BRAZIL);
            return QString::fromUtf8(C_LOCALE_PORTUGUESE);
        case QLocale::Romanian: return QString::fromUtf8(C_LOCALE_ROMANIAN);
        case QLocale::Russian: return QString::fromUtf8(C_LOCALE_RUSSIAN);
        case QLocale::Serbian: return QString::fromUtf8(C_LOCALE_SERBIAN);
        case QLocale::Slovak: return QString::fromUtf8(C_LOCALE_SLOVAK);
        case QLocale::Slovenian: return QString::fromUtf8(C_LOCALE_SLOVENIAN);
        case QLocale::Spanish: return QString::fromUtf8(C_LOCALE_SPANISH);
        case QLocale::Swedish: return QString::fromUtf8(C_LOCALE_SWEDISH);
        case QLocale::Thai: return QString::fromUtf8(C_LOCALE_THAI);
        case QLocale::Turkish: return QString::fromUtf8(C_LOCALE_TURKISH);
        case QLocale::Ukrainian: return QString::fromUtf8(C_LOCALE_UKRAINIAN);
        case QLocale::Uzbek: return QString::fromUtf8(C_LOCALE_UZBEK);
        case QLocale::Vietnamese: return QString::fromUtf8(C_LOCALE_VIETNAMESE);
        default:
            const QString lang = QLocale::languageToString(locale.language());
            qWarning() << "Unrecognized language name: " << lang;
            return lang;
        }
    }
}

class WheelEventEater final : public QObject
{
public:
    using QObject::QObject;

private:
    bool eventFilter(QObject *, QEvent *event) override
    {
        return (event->type() == QEvent::Wheel);
    }
};

// Constructor
OptionsDialog::OptionsDialog(QWidget *parent)
    : QDialog {parent}
    , m_ui {new Ui::OptionsDialog}
    , m_storeDialogSize {SETTINGS_KEY("Size")}
    , m_storeHSplitterSize {SETTINGS_KEY("HorizontalSplitterSizes")}
    , m_storeLastViewedPage {SETTINGS_KEY("LastViewedPage")}
{
    qDebug("-> Constructing Options");
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(true);

#if (defined(Q_OS_UNIX))
    setWindowTitle(tr("Preferences"));
#endif

    // Icons
    m_ui->tabSelection->item(TAB_UI)->setIcon(UIThemeManager::instance()->getIcon("preferences-desktop"));
    m_ui->tabSelection->item(TAB_BITTORRENT)->setIcon(UIThemeManager::instance()->getIcon("preferences-system-network"));
    m_ui->tabSelection->item(TAB_CONNECTION)->setIcon(UIThemeManager::instance()->getIcon("configure"));
    m_ui->tabSelection->item(TAB_DOWNLOADS)->setIcon(UIThemeManager::instance()->getIcon("folder-download"));
    m_ui->tabSelection->item(TAB_SPEED)->setIcon(UIThemeManager::instance()->getIcon("speedometer", "chronometer"));
    m_ui->tabSelection->item(TAB_RSS)->setIcon(UIThemeManager::instance()->getIcon("rss-config", "application-rss+xml"));
#ifndef DISABLE_WEBUI
    m_ui->tabSelection->item(TAB_WEBUI)->setIcon(UIThemeManager::instance()->getIcon("webui"));
#else
    m_ui->tabSelection->item(TAB_WEBUI)->setHidden(true);
#endif
    m_ui->tabSelection->item(TAB_ADVANCED)->setIcon(UIThemeManager::instance()->getIcon("preferences-other"));

    // set uniform size for all icons
    int maxHeight = -1;
    for (int i = 0; i < m_ui->tabSelection->count(); ++i)
        maxHeight = std::max(maxHeight, m_ui->tabSelection->visualItemRect(m_ui->tabSelection->item(i)).size().height());
    for (int i = 0; i < m_ui->tabSelection->count(); ++i)
    {
        const QSize size(std::numeric_limits<int>::max(), static_cast<int>(maxHeight * 1.2));
        m_ui->tabSelection->item(i)->setSizeHint(size);
    }

    m_ui->IpFilterRefreshBtn->setIcon(UIThemeManager::instance()->getIcon("view-refresh"));

    m_ui->labelGlobalRate->setPixmap(Utils::Gui::scaledPixmap(UIThemeManager::instance()->getIcon(QLatin1String("slow_off")), this, 24));
    m_ui->labelAltRate->setPixmap(Utils::Gui::scaledPixmap(UIThemeManager::instance()->getIcon(QLatin1String("slow")), this, 24));

    m_ui->deleteTorrentWarningIcon->setPixmap(QApplication::style()->standardIcon(QStyle::SP_MessageBoxCritical).pixmap(16, 16));
    m_ui->deleteTorrentWarningIcon->hide();
    m_ui->deleteTorrentWarningLabel->hide();
    m_ui->deleteTorrentWarningLabel->setToolTip(QLatin1String("<html><body><p>") +
        tr("By enabling these options, you can <strong>irrevocably lose</strong> your .torrent files!") +
        QLatin1String("</p><p>") +
        tr("When these options are enabled, qBittorrent will <strong>delete</strong> .torrent files "
        "after they were successfully (the first option) or not (the second option) added to its "
        "download queue. This will be applied <strong>not only</strong> to the files opened via "
        "&ldquo;Add torrent&rdquo; menu action but to those opened via <strong>file type association</strong> as well") +
        QLatin1String("</p><p>") +
        tr("If you enable the second option (&ldquo;Also when addition is cancelled&rdquo;) the "
        ".torrent file <strong>will be deleted</strong> even if you press &ldquo;<strong>Cancel</strong>&rdquo; in "
        "the &ldquo;Add torrent&rdquo; dialog") +
        QLatin1String("</p></body></html>"));

    m_ui->hsplitter->setCollapsible(0, false);
    m_ui->hsplitter->setCollapsible(1, false);
    // Get apply button in button box
    m_applyButton = m_ui->buttonBox->button(QDialogButtonBox::Apply);
    connect(m_applyButton, &QPushButton::clicked, this, &OptionsDialog::applySettings);

    auto watchedFoldersModel = new WatchedFoldersModel(TorrentFilesWatcher::instance(), this);
    connect(watchedFoldersModel, &QAbstractListModel::dataChanged, this, &ThisType::enableApplyButton);
    m_ui->scanFoldersView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_ui->scanFoldersView->setModel(watchedFoldersModel);
    connect(m_ui->scanFoldersView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ThisType::handleWatchedFolderViewSelectionChanged);
    connect(m_ui->scanFoldersView, &QTreeView::doubleClicked, this, &ThisType::editWatchedFolderOptions);

    // Languages supported
    initializeLanguageCombo();

    m_ui->checkUseCustomTheme->setChecked(Preferences::instance()->useCustomUITheme());
    m_ui->customThemeFilePath->setSelectedPath(Preferences::instance()->customUIThemePath());
    m_ui->customThemeFilePath->setMode(FileSystemPathEdit::Mode::FileOpen);
    m_ui->customThemeFilePath->setDialogCaption(tr("Select qBittorrent UI Theme file"));
    m_ui->customThemeFilePath->setFileNameFilter(tr("qBittorrent UI Theme file (*.qbtheme)"));

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    m_ui->checkUseSystemIcon->setChecked(Preferences::instance()->useSystemIconTheme());
#else
    m_ui->checkUseSystemIcon->setVisible(false);
#endif

    // Load week days (scheduler)
    m_ui->comboBoxScheduleDays->addItems(translatedWeekdayNames());

    // Load options
    loadOptions();
#ifdef Q_OS_MACOS
    m_ui->checkShowSystray->setVisible(false);
#else
    // Disable systray integration if it is not supported by the system
    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        m_ui->checkShowSystray->setChecked(false);
        m_ui->checkShowSystray->setEnabled(false);
        m_ui->labelTrayIconStyle->setVisible(false);
        m_ui->comboTrayIcon->setVisible(false);
    }
#endif

#ifndef Q_OS_WIN
    m_ui->checkStartup->setVisible(false);
#endif

#if !(defined(Q_OS_WIN) || defined(Q_OS_MACOS))
    m_ui->groupFileAssociation->setVisible(false);
    m_ui->checkProgramUpdates->setVisible(false);
#endif

    m_ui->textWebUIRootFolder->setMode(FileSystemPathEdit::Mode::DirectoryOpen);
    m_ui->textWebUIRootFolder->setDialogCaption(tr("Choose Alternative UI files location"));

    // Connect signals / slots
    // Shortcuts for frequently used signals that have more than one overload. They would require
    // type casts and that is why we declare required member pointer here instead.
    void (QComboBox::*qComboBoxCurrentIndexChanged)(int) = &QComboBox::currentIndexChanged;
    void (QSpinBox::*qSpinBoxValueChanged)(int) = &QSpinBox::valueChanged;

    connect(m_ui->comboProxyType, qComboBoxCurrentIndexChanged, this, &ThisType::enableProxy);

    // Apply button is activated when a value is changed
    // Behavior tab
    connect(m_ui->comboI18n, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkUseCustomTheme, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->customThemeFilePath, &FileSystemPathEdit::selectedPathChanged, this, &ThisType::enableApplyButton);
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    connect(m_ui->checkUseSystemIcon, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
#endif
    connect(m_ui->confirmDeletion, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkAltRowColors, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkHideZero, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkHideZero, &QAbstractButton::toggled, m_ui->comboHideZero, &QWidget::setEnabled);
    connect(m_ui->comboHideZero, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkShowSystray, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkCloseToSystray, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMinimizeToSysTray, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkStartMinimized, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
#ifdef Q_OS_WIN
    connect(m_ui->checkStartup, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
#endif
    connect(m_ui->checkShowSplash, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkProgramExitConfirm, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkProgramAutoExitConfirm, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkPreventFromSuspendWhenDownloading, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkPreventFromSuspendWhenSeeding, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->comboTrayIcon, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)) && !defined(QT_DBUS_LIB)
    m_ui->checkPreventFromSuspendWhenDownloading->setDisabled(true);
    m_ui->checkPreventFromSuspendWhenSeeding->setDisabled(true);
#endif
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    connect(m_ui->checkAssociateTorrents, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkAssociateMagnetLinks, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkProgramUpdates, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
#endif
    connect(m_ui->checkFileLog, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->textFileLogPath, &FileSystemPathEdit::selectedPathChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkFileLogBackup, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkFileLogBackup, &QAbstractButton::toggled, m_ui->spinFileLogSize, &QWidget::setEnabled);
    connect(m_ui->checkFileLogDelete, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkFileLogDelete, &QAbstractButton::toggled, m_ui->spinFileLogAge, &QWidget::setEnabled);
    connect(m_ui->checkFileLogDelete, &QAbstractButton::toggled, m_ui->comboFileLogAgeType, &QWidget::setEnabled);
    connect(m_ui->spinFileLogSize, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinFileLogAge, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->comboFileLogAgeType, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    // Downloads tab
    connect(m_ui->textSavePath, &FileSystemPathEdit::selectedPathChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkUseSubcategories, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->comboSavingMode, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->comboTorrentCategoryChanged, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->comboCategoryDefaultPathChanged, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->comboCategoryChanged, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->textTempPath, &FileSystemPathEdit::selectedPathChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkAppendqB, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkPreallocateAll, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkRecursiveDownload, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkAdditionDialog, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkAdditionDialogFront, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkStartPaused, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->contentLayoutComboBox, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->deleteTorrentBox, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->deleteCancelledTorrentBox, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkExportDir, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkExportDir, &QAbstractButton::toggled, m_ui->textExportDir, &QWidget::setEnabled);
    connect(m_ui->checkExportDirFin, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkExportDirFin, &QAbstractButton::toggled, m_ui->textExportDirFin, &QWidget::setEnabled);
    connect(m_ui->textExportDir, &FileSystemPathEdit::selectedPathChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->textExportDirFin, &FileSystemPathEdit::selectedPathChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->actionTorrentDlOnDblClBox, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->actionTorrentFnOnDblClBox, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkTempFolder, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkTempFolder, &QAbstractButton::toggled, m_ui->textTempPath, &QWidget::setEnabled);
    connect(m_ui->addWatchedFolderButton, &QAbstractButton::clicked, this, &ThisType::enableApplyButton);
    connect(m_ui->removeWatchedFolderButton, &QAbstractButton::clicked, this, &ThisType::enableApplyButton);
    connect(m_ui->groupMailNotification, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->senderEmailTxt, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->lineEditDestEmail, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->lineEditSmtpServer, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkSmtpSSL, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->groupMailNotifAuth, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->mailNotifUsername, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->mailNotifPassword, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->autoRunBox, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->lineEditAutoRun, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->autoRunConsole, &QCheckBox::toggled, this, &ThisType::enableApplyButton);

    const QString autoRunStr = QString("%1\n    %2\n    %3\n    %4\n    %5\n    %6\n    %7\n    %8\n    %9\n    %10\n    %11\n    %12\n    %13\n%14")
        .arg(tr("Supported parameters (case sensitive):")
            , tr("%N: Torrent name")
            , tr("%L: Category")
            , tr("%G: Tags (separated by comma)")
            , tr("%F: Content path (same as root path for multifile torrent)")
            , tr("%R: Root path (first torrent subdirectory path)")
            , tr("%D: Save path")
            , tr("%C: Number of files")
            , tr("%Z: Torrent size (bytes)"))
        .arg(tr("%T: Current tracker")
            , tr("%I: Info hash v1 (or '-' if unavailable)")
            , tr("%J: Info hash v2 (or '-' if unavailable)")
            , tr("%K: Torrent ID (either sha-1 info hash for v1 torrent or truncated sha-256 info hash for v2/hybrid torrent)")
            , tr("Tip: Encapsulate parameter with quotation marks to avoid text being cut off at whitespace (e.g., \"%N\")"));
    m_ui->labelAutoRunParam->setText(autoRunStr);

    // Connection tab
    connect(m_ui->comboProtocol, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinPort, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkUPnP, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->spinUploadLimit, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinDownloadLimit, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinUploadLimitAlt, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinDownloadLimitAlt, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->groupBoxSchedule, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->timeEditScheduleFrom, &QDateTimeEdit::timeChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->timeEditScheduleTo, &QDateTimeEdit::timeChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->comboBoxScheduleDays, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkLimituTPConnections, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkLimitTransportOverhead, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkLimitLocalPeerRate, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    // Bittorrent tab
    connect(m_ui->checkMaxConnecs, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMaxConnecsPerTorrent, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMaxUploads, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMaxUploadsPerTorrent, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxConnec, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxConnecPerTorrent, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxUploads, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxUploadsPerTorrent, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkDHT, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkAnonymousMode, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkPeX, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkLSD, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->comboEncryption, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMaxRatio, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMaxRatio, &QAbstractButton::toggled, this, &ThisType::toggleComboRatioLimitAct);
    connect(m_ui->spinMaxRatio, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &ThisType::enableApplyButton);
    connect(m_ui->comboRatioLimitAct, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMaxSeedingMinutes, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMaxSeedingMinutes, &QAbstractButton::toggled, this, &ThisType::toggleComboRatioLimitAct);
    connect(m_ui->spinMaxSeedingMinutes, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    // Proxy tab
    connect(m_ui->comboProxyType, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->textProxyIP, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinProxyPort, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkProxyPeerConnecs, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->isProxyOnlyForTorrents, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkProxyAuth, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->textProxyUsername, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->textProxyPassword, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    // Misc tab
    connect(m_ui->checkIPFilter, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkIPFilter, &QAbstractButton::toggled, m_ui->textFilterPath, &QWidget::setEnabled);
    connect(m_ui->checkIPFilter, &QAbstractButton::toggled, m_ui->IpFilterRefreshBtn, &QWidget::setEnabled);
    connect(m_ui->checkIpFilterTrackers, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->textFilterPath, &FileSystemPathEdit::selectedPathChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkEnableQueueing, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxActiveDownloads, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxActiveUploads, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxActiveTorrents, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkIgnoreSlowTorrentsForQueueing, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->spinDownloadRateForSlowTorrents, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinUploadRateForSlowTorrents, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinSlowTorrentsInactivityTimer, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkEnableAddTrackers, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->textTrackers, &QPlainTextEdit::textChanged, this, &ThisType::enableApplyButton);

    const QString slowTorrentsExplanation = QLatin1String("<html><body><p>")
            + tr("A torrent will be considered slow if its download and upload rates stay below these values for \"Torrent inactivity timer\" seconds")
            + QLatin1String("</p></body></html>");
    m_ui->labelDownloadRateForSlowTorrents->setToolTip(slowTorrentsExplanation);
    m_ui->labelUploadRateForSlowTorrents->setToolTip(slowTorrentsExplanation);
    m_ui->labelSlowTorrentInactivityTimer->setToolTip(slowTorrentsExplanation);

#ifndef DISABLE_WEBUI
    // Web UI tab
    m_ui->textWebUIHttpsCert->setMode(FileSystemPathEdit::Mode::FileOpen);
    m_ui->textWebUIHttpsCert->setFileNameFilter(tr("Certificate") + QLatin1String(" (*.cer *.crt *.pem)"));
    m_ui->textWebUIHttpsCert->setDialogCaption(tr("Select certificate"));
    m_ui->textWebUIHttpsKey->setMode(FileSystemPathEdit::Mode::FileOpen);
    m_ui->textWebUIHttpsKey->setFileNameFilter(tr("Private key") + QLatin1String(" (*.key *.pem)"));
    m_ui->textWebUIHttpsKey->setDialogCaption(tr("Select private key"));

    connect(m_ui->textServerDomains, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkWebUi, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->textWebUiAddress, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinWebUiPort, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkWebUIUPnP, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkWebUiHttps, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->textWebUIHttpsCert, &FileSystemPathLineEdit::selectedPathChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->textWebUIHttpsCert, &FileSystemPathLineEdit::selectedPathChanged, this, [this](const QString &s) { webUIHttpsCertChanged(s, ShowError::Show); });
    connect(m_ui->textWebUIHttpsKey, &FileSystemPathLineEdit::selectedPathChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->textWebUIHttpsKey, &FileSystemPathLineEdit::selectedPathChanged, this, [this](const QString &s) { webUIHttpsKeyChanged(s, ShowError::Show); });
    connect(m_ui->textWebUiUsername, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->textWebUiPassword, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkBypassLocalAuth, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkBypassAuthSubnetWhitelist, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkBypassAuthSubnetWhitelist, &QAbstractButton::toggled, m_ui->IPSubnetWhitelistButton, &QPushButton::setEnabled);
    connect(m_ui->spinBanCounter, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinBanDuration, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinSessionTimeout, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkClickjacking, &QCheckBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkCSRFProtection, &QCheckBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkWebUiHttps, &QGroupBox::toggled, m_ui->checkSecureCookie, &QWidget::setEnabled);
    connect(m_ui->checkSecureCookie, &QCheckBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->groupHostHeaderValidation, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkDynDNS, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->comboDNSService, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->domainNameTxt, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->DNSUsernameTxt, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->DNSPasswordTxt, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->groupAltWebUI, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->textWebUIRootFolder, &FileSystemPathLineEdit::selectedPathChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->groupWebUIAddCustomHTTPHeaders, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->textWebUICustomHTTPHeaders, &QPlainTextEdit::textChanged, this, &OptionsDialog::enableApplyButton);
    connect(m_ui->groupEnableReverseProxySupport, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->textTrustedReverseProxiesList, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
#endif // DISABLE_WEBUI

    // RSS tab
    connect(m_ui->checkRSSEnable, &QCheckBox::toggled, this, &OptionsDialog::enableApplyButton);
    connect(m_ui->checkRSSAutoDownloaderEnable, &QCheckBox::toggled, this, &OptionsDialog::enableApplyButton);
    connect(m_ui->textSmartEpisodeFilters, &QPlainTextEdit::textChanged, this, &OptionsDialog::enableApplyButton);
    connect(m_ui->checkSmartFilterDownloadRepacks, &QCheckBox::toggled, this, &OptionsDialog::enableApplyButton);
    connect(m_ui->spinRSSRefreshInterval, qSpinBoxValueChanged, this, &OptionsDialog::enableApplyButton);
    connect(m_ui->spinRSSMaxArticlesPerFeed, qSpinBoxValueChanged, this, &OptionsDialog::enableApplyButton);
    connect(m_ui->btnEditRules, &QPushButton::clicked, this, [this]()
    {
        auto *downloader = new AutomatedRssDownloader(this);
        downloader->setAttribute(Qt::WA_DeleteOnClose);
        downloader->open();
    });

    // Disable apply Button
    m_applyButton->setEnabled(false);
    // Tab selection mechanism
    connect(m_ui->tabSelection, &QListWidget::currentItemChanged, this, &ThisType::changePage);
    // Load Advanced settings
    m_advancedSettings = new AdvancedSettings(m_ui->tabAdvancedPage);
    m_ui->advPageLayout->addWidget(m_advancedSettings);
    connect(m_advancedSettings, &AdvancedSettings::settingsChanged, this, &ThisType::enableApplyButton);

    m_ui->textFileLogPath->setDialogCaption(tr("Choose a save directory"));
    m_ui->textFileLogPath->setMode(FileSystemPathEdit::Mode::DirectorySave);

    m_ui->textExportDir->setDialogCaption(tr("Choose export directory"));
    m_ui->textExportDir->setMode(FileSystemPathEdit::Mode::DirectorySave);

    m_ui->textExportDirFin->setDialogCaption(tr("Choose export directory"));
    m_ui->textExportDirFin->setMode(FileSystemPathEdit::Mode::DirectorySave);

    m_ui->textFilterPath->setDialogCaption(tr("Choose an IP filter file"));
    m_ui->textFilterPath->setFileNameFilter(tr("All supported filters")
        + QLatin1String(" (*.dat *.p2p *.p2b);;.dat (*.dat);;.p2p (*.p2p);;.p2b (*.p2b)"));

    m_ui->textSavePath->setDialogCaption(tr("Choose a save directory"));
    m_ui->textSavePath->setMode(FileSystemPathEdit::Mode::DirectorySave);

    m_ui->textTempPath->setDialogCaption(tr("Choose a save directory"));
    m_ui->textTempPath->setMode(FileSystemPathEdit::Mode::DirectorySave);

    // disable mouse wheel event on widgets to avoid mis-selection
    auto *wheelEventEater = new WheelEventEater(this);
    for (QComboBox *widget : asConst(findChildren<QComboBox *>()))
        widget->installEventFilter(wheelEventEater);
    for (QSpinBox *widget : asConst(findChildren<QSpinBox *>()))
        widget->installEventFilter(wheelEventEater);

    m_ui->tabSelection->setCurrentRow(m_storeLastViewedPage);

    Utils::Gui::resize(this, m_storeDialogSize);
    show();
    // Have to be called after show(), because splitter width needed
    loadSplitterState();
}

void OptionsDialog::initializeLanguageCombo()
{
    // List language files
    const QDir langDir(":/lang");
    const QStringList langFiles = langDir.entryList(QStringList("qbittorrent_*.qm"), QDir::Files);
    for (const QString &langFile : langFiles)
    {
        QString localeStr = langFile.mid(12); // remove "qbittorrent_"
        localeStr.chop(3); // Remove ".qm"
        QString languageName;
        if (localeStr.startsWith("eo", Qt::CaseInsensitive))
        {
            // QLocale doesn't work with that locale. Esperanto isn't a "real" language.
            languageName = QString::fromUtf8(C_LOCALE_ESPERANTO);
        }
        else if (localeStr.startsWith("ltg", Qt::CaseInsensitive))
        {
            // QLocale doesn't work with that locale.
            languageName = QString::fromUtf8(C_LOCALE_LATGALIAN);
        }
        else
        {
            QLocale locale(localeStr);
            languageName = languageToLocalizedString(locale);
        }
        m_ui->comboI18n->addItem(/*QIcon(":/icons/flags/"+country+".svg"), */ languageName, localeStr);
        qDebug() << "Supported locale:" << localeStr;
    }
}

// Main destructor
OptionsDialog::~OptionsDialog()
{
    qDebug("-> destructing Options");

    // save dialog states
    m_storeDialogSize = size();

    QStringList hSplitterSizes;
    for (const int size : asConst(m_ui->hsplitter->sizes()))
        hSplitterSizes.append(QString::number(size));
    m_storeHSplitterSize = hSplitterSizes;

    m_storeLastViewedPage = m_ui->tabSelection->currentRow();

    delete m_ui;
}

void OptionsDialog::changePage(QListWidgetItem *current, QListWidgetItem *previous)
{
    if (!current)
        current = previous;
    m_ui->tabOption->setCurrentIndex(m_ui->tabSelection->row(current));
}

void OptionsDialog::loadSplitterState()
{
    // width has been modified, use height as width reference instead
    const int width = Utils::Gui::scaledSize(this
        , (m_ui->tabSelection->item(TAB_UI)->sizeHint().height() * 2));
    const QStringList defaultSizes = {QString::number(width), QString::number(m_ui->hsplitter->width() - width)};

    QList<int> splitterSizes;
    for (const QString &string : asConst(m_storeHSplitterSize.get(defaultSizes)))
        splitterSizes.append(string.toInt());

    m_ui->hsplitter->setSizes(splitterSizes);
}

void OptionsDialog::saveOptions()
{
    m_applyButton->setEnabled(false);
    Preferences *const pref = Preferences::instance();
    // Load the translation
    QString locale = getLocale();
    if (pref->getLocale() != locale)
    {
        auto *translator = new QTranslator;
        if (translator->load(QLatin1String(":/lang/qbittorrent_") + locale))
            qDebug("%s locale recognized, using translation.", qUtf8Printable(locale));
        else
            qDebug("%s locale unrecognized, using default (en).", qUtf8Printable(locale));
        qApp->installTranslator(translator);
    }

    // Behavior preferences
    pref->setLocale(locale);

    pref->setUseCustomUITheme(m_ui->checkUseCustomTheme->isChecked());
    pref->setCustomUIThemePath(m_ui->customThemeFilePath->selectedPath());

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    pref->useSystemIconTheme(m_ui->checkUseSystemIcon->isChecked());
#endif

    pref->setConfirmTorrentDeletion(m_ui->confirmDeletion->isChecked());
    pref->setAlternatingRowColors(m_ui->checkAltRowColors->isChecked());
    pref->setHideZeroValues(m_ui->checkHideZero->isChecked());
    pref->setHideZeroComboValues(m_ui->comboHideZero->currentIndex());
#ifndef Q_OS_MACOS
    pref->setSystrayIntegration(systrayIntegration());
    pref->setTrayIconStyle(TrayIcon::Style(m_ui->comboTrayIcon->currentIndex()));
    pref->setCloseToTray(closeToTray());
    pref->setMinimizeToTray(minimizeToTray());
#endif
    pref->setStartMinimized(startMinimized());
    pref->setSplashScreenDisabled(isSplashScreenDisabled());
    pref->setConfirmOnExit(m_ui->checkProgramExitConfirm->isChecked());
    pref->setDontConfirmAutoExit(!m_ui->checkProgramAutoExitConfirm->isChecked());
    pref->setPreventFromSuspendWhenDownloading(m_ui->checkPreventFromSuspendWhenDownloading->isChecked());
    pref->setPreventFromSuspendWhenSeeding(m_ui->checkPreventFromSuspendWhenSeeding->isChecked());
#ifdef Q_OS_WIN
    pref->setWinStartup(WinStartup());
    // Windows: file association settings
    Preferences::setTorrentFileAssoc(m_ui->checkAssociateTorrents->isChecked());
    Preferences::setMagnetLinkAssoc(m_ui->checkAssociateMagnetLinks->isChecked());
#endif
#ifdef Q_OS_MACOS
    if (m_ui->checkAssociateTorrents->isChecked())
    {
        Preferences::setTorrentFileAssoc();
        m_ui->checkAssociateTorrents->setChecked(Preferences::isTorrentFileAssocSet());
        m_ui->checkAssociateTorrents->setEnabled(!m_ui->checkAssociateTorrents->isChecked());
    }
    if (m_ui->checkAssociateMagnetLinks->isChecked())
    {
        Preferences::setMagnetLinkAssoc();
        m_ui->checkAssociateMagnetLinks->setChecked(Preferences::isMagnetLinkAssocSet());
        m_ui->checkAssociateMagnetLinks->setEnabled(!m_ui->checkAssociateMagnetLinks->isChecked());
    }
#endif
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    pref->setUpdateCheckEnabled(m_ui->checkProgramUpdates->isChecked());
#endif
    auto *const app = static_cast<Application *>(QCoreApplication::instance());
    app->setFileLoggerPath(m_ui->textFileLogPath->selectedPath());
    app->setFileLoggerBackup(m_ui->checkFileLogBackup->isChecked());
    app->setFileLoggerMaxSize(m_ui->spinFileLogSize->value() * 1024);
    app->setFileLoggerAge(m_ui->spinFileLogAge->value());
    app->setFileLoggerAgeType(m_ui->comboFileLogAgeType->currentIndex());
    app->setFileLoggerDeleteOld(m_ui->checkFileLogDelete->isChecked());
    app->setFileLoggerEnabled(m_ui->checkFileLog->isChecked());
    // End Behavior preferences

    RSS::Session::instance()->setRefreshInterval(m_ui->spinRSSRefreshInterval->value());
    RSS::Session::instance()->setMaxArticlesPerFeed(m_ui->spinRSSMaxArticlesPerFeed->value());
    RSS::Session::instance()->setProcessingEnabled(m_ui->checkRSSEnable->isChecked());
    RSS::AutoDownloader::instance()->setProcessingEnabled(m_ui->checkRSSAutoDownloaderEnable->isChecked());
    RSS::AutoDownloader::instance()->setSmartEpisodeFilters(m_ui->textSmartEpisodeFilters->toPlainText().split('\n', Qt::SkipEmptyParts));
    RSS::AutoDownloader::instance()->setDownloadRepacks(m_ui->checkSmartFilterDownloadRepacks->isChecked());

    auto session = BitTorrent::Session::instance();

    // Downloads preferences
    session->setDefaultSavePath(Utils::Fs::expandPathAbs(m_ui->textSavePath->selectedPath()));
    session->setSubcategoriesEnabled(m_ui->checkUseSubcategories->isChecked());
    session->setAutoTMMDisabledByDefault(m_ui->comboSavingMode->currentIndex() == 0);
    session->setDisableAutoTMMWhenCategoryChanged(m_ui->comboTorrentCategoryChanged->currentIndex() == 1);
    session->setDisableAutoTMMWhenCategorySavePathChanged(m_ui->comboCategoryChanged->currentIndex() == 1);
    session->setDisableAutoTMMWhenDefaultSavePathChanged(m_ui->comboCategoryDefaultPathChanged->currentIndex() == 1);
    session->setTempPathEnabled(m_ui->checkTempFolder->isChecked());
    session->setTempPath(Utils::Fs::expandPathAbs(m_ui->textTempPath->selectedPath()));
    session->setAppendExtensionEnabled(m_ui->checkAppendqB->isChecked());
    session->setPreallocationEnabled(preAllocateAllFiles());
    pref->disableRecursiveDownload(!m_ui->checkRecursiveDownload->isChecked());
    AddNewTorrentDialog::setEnabled(useAdditionDialog());
    AddNewTorrentDialog::setTopLevel(m_ui->checkAdditionDialogFront->isChecked());
    session->setAddTorrentPaused(addTorrentsInPause());
    session->setTorrentContentLayout(static_cast<BitTorrent::TorrentContentLayout>(m_ui->contentLayoutComboBox->currentIndex()));
    auto watchedFoldersModel = static_cast<WatchedFoldersModel *>(m_ui->scanFoldersView->model());
    watchedFoldersModel->apply();
    session->setTorrentExportDirectory(getTorrentExportDir());
    session->setFinishedTorrentExportDirectory(getFinishedTorrentExportDir());
    pref->setMailNotificationEnabled(m_ui->groupMailNotification->isChecked());
    pref->setMailNotificationSender(m_ui->senderEmailTxt->text());
    pref->setMailNotificationEmail(m_ui->lineEditDestEmail->text());
    pref->setMailNotificationSMTP(m_ui->lineEditSmtpServer->text());
    pref->setMailNotificationSMTPSSL(m_ui->checkSmtpSSL->isChecked());
    pref->setMailNotificationSMTPAuth(m_ui->groupMailNotifAuth->isChecked());
    pref->setMailNotificationSMTPUsername(m_ui->mailNotifUsername->text());
    pref->setMailNotificationSMTPPassword(m_ui->mailNotifPassword->text());
    pref->setAutoRunEnabled(m_ui->autoRunBox->isChecked());
    pref->setAutoRunProgram(m_ui->lineEditAutoRun->text().trimmed());
#if defined(Q_OS_WIN)
    pref->setAutoRunConsoleEnabled(m_ui->autoRunConsole->isChecked());
#endif
    pref->setActionOnDblClOnTorrentDl(getActionOnDblClOnTorrentDl());
    pref->setActionOnDblClOnTorrentFn(getActionOnDblClOnTorrentFn());
    TorrentFileGuard::setAutoDeleteMode(!m_ui->deleteTorrentBox->isChecked() ? TorrentFileGuard::Never
                             : !m_ui->deleteCancelledTorrentBox->isChecked() ? TorrentFileGuard::IfAdded
                             : TorrentFileGuard::Always);
    // End Downloads preferences

    // Connection preferences
    session->setBTProtocol(static_cast<BitTorrent::BTProtocol>(m_ui->comboProtocol->currentIndex()));
    session->setPort(getPort());
    Net::PortForwarder::instance()->setEnabled(isUPnPEnabled());
    session->setGlobalDownloadSpeedLimit(m_ui->spinDownloadLimit->value() * 1024);
    session->setGlobalUploadSpeedLimit(m_ui->spinUploadLimit->value() * 1024);
    session->setAltGlobalDownloadSpeedLimit(m_ui->spinDownloadLimitAlt->value() * 1024);
    session->setAltGlobalUploadSpeedLimit(m_ui->spinUploadLimitAlt->value() * 1024);
    session->setUTPRateLimited(m_ui->checkLimituTPConnections->isChecked());
    session->setIncludeOverheadInLimits(m_ui->checkLimitTransportOverhead->isChecked());
    session->setIgnoreLimitsOnLAN(!m_ui->checkLimitLocalPeerRate->isChecked());
    pref->setSchedulerStartTime(m_ui->timeEditScheduleFrom->time());
    pref->setSchedulerEndTime(m_ui->timeEditScheduleTo->time());
    pref->setSchedulerDays(static_cast<SchedulerDays>(m_ui->comboBoxScheduleDays->currentIndex()));
    session->setBandwidthSchedulerEnabled(m_ui->groupBoxSchedule->isChecked());

    auto proxyConfigManager = Net::ProxyConfigurationManager::instance();
    Net::ProxyConfiguration proxyConf;
    proxyConf.type = getProxyType();
    proxyConf.ip = getProxyIp();
    proxyConf.port = getProxyPort();
    proxyConf.username = getProxyUsername();
    proxyConf.password = getProxyPassword();
    proxyConfigManager->setProxyOnlyForTorrents(m_ui->isProxyOnlyForTorrents->isChecked());
    proxyConfigManager->setProxyConfiguration(proxyConf);

    session->setProxyPeerConnectionsEnabled(m_ui->checkProxyPeerConnecs->isChecked());
    // End Connection preferences

    // Bittorrent preferences
    session->setMaxConnections(getMaxConnecs());
    session->setMaxConnectionsPerTorrent(getMaxConnecsPerTorrent());
    session->setMaxUploads(getMaxUploads());
    session->setMaxUploadsPerTorrent(getMaxUploadsPerTorrent());
    session->setDHTEnabled(isDHTEnabled());
    session->setPeXEnabled(m_ui->checkPeX->isChecked());
    session->setLSDEnabled(isLSDEnabled());
    session->setEncryption(getEncryptionSetting());
    session->setAnonymousModeEnabled(m_ui->checkAnonymousMode->isChecked());
    session->setAddTrackersEnabled(m_ui->checkEnableAddTrackers->isChecked());
    session->setAdditionalTrackers(m_ui->textTrackers->toPlainText());
    session->setGlobalMaxRatio(getMaxRatio());
    session->setGlobalMaxSeedingMinutes(getMaxSeedingMinutes());

    const QVector<MaxRatioAction> actIndex =
    {
        Pause,
        Remove,
        DeleteFiles,
        EnableSuperSeeding
    };
    session->setMaxRatioAction(actIndex.value(m_ui->comboRatioLimitAct->currentIndex()));
    // End Bittorrent preferences

    // Misc preferences
    // * IPFilter
    session->setIPFilteringEnabled(isIPFilteringEnabled());
    session->setTrackerFilteringEnabled(m_ui->checkIpFilterTrackers->isChecked());
    session->setIPFilterFile(m_ui->textFilterPath->selectedPath());
    // End IPFilter preferences
    // Queueing system
    session->setQueueingSystemEnabled(isQueueingSystemEnabled());
    session->setMaxActiveDownloads(m_ui->spinMaxActiveDownloads->value());
    session->setMaxActiveUploads(m_ui->spinMaxActiveUploads->value());
    session->setMaxActiveTorrents(m_ui->spinMaxActiveTorrents->value());
    session->setIgnoreSlowTorrentsForQueueing(m_ui->checkIgnoreSlowTorrentsForQueueing->isChecked());
    session->setDownloadRateForSlowTorrents(m_ui->spinDownloadRateForSlowTorrents->value());
    session->setUploadRateForSlowTorrents(m_ui->spinUploadRateForSlowTorrents->value());
    session->setSlowTorrentsInactivityTimer(m_ui->spinSlowTorrentsInactivityTimer->value());
    // End Queueing system preferences
    // Web UI
    pref->setWebUiEnabled(isWebUiEnabled());
    if (isWebUiEnabled())
    {
        pref->setServerDomains(m_ui->textServerDomains->text());
        pref->setWebUiAddress(m_ui->textWebUiAddress->text());
        pref->setWebUiPort(m_ui->spinWebUiPort->value());
        pref->setUPnPForWebUIPort(m_ui->checkWebUIUPnP->isChecked());
        pref->setWebUiHttpsEnabled(m_ui->checkWebUiHttps->isChecked());
        pref->setWebUIHttpsCertificatePath(m_ui->textWebUIHttpsCert->selectedPath());
        pref->setWebUIHttpsKeyPath(m_ui->textWebUIHttpsKey->selectedPath());
        pref->setWebUIMaxAuthFailCount(m_ui->spinBanCounter->value());
        pref->setWebUIBanDuration(std::chrono::seconds {m_ui->spinBanDuration->value()});
        pref->setWebUISessionTimeout(m_ui->spinSessionTimeout->value());
        // Authentication
        pref->setWebUiUsername(webUiUsername());
        if (!webUiPassword().isEmpty())
            pref->setWebUIPassword(Utils::Password::PBKDF2::generate(webUiPassword()));
        pref->setWebUiLocalAuthEnabled(!m_ui->checkBypassLocalAuth->isChecked());
        pref->setWebUiAuthSubnetWhitelistEnabled(m_ui->checkBypassAuthSubnetWhitelist->isChecked());
        // Security
        pref->setWebUiClickjackingProtectionEnabled(m_ui->checkClickjacking->isChecked());
        pref->setWebUiCSRFProtectionEnabled(m_ui->checkCSRFProtection->isChecked());
        pref->setWebUiSecureCookieEnabled(m_ui->checkSecureCookie->isChecked());
        pref->setWebUIHostHeaderValidationEnabled(m_ui->groupHostHeaderValidation->isChecked());
        // DynDNS
        pref->setDynDNSEnabled(m_ui->checkDynDNS->isChecked());
        pref->setDynDNSService(m_ui->comboDNSService->currentIndex());
        pref->setDynDomainName(m_ui->domainNameTxt->text());
        pref->setDynDNSUsername(m_ui->DNSUsernameTxt->text());
        pref->setDynDNSPassword(m_ui->DNSPasswordTxt->text());
        // Alternative UI
        pref->setAltWebUiEnabled(m_ui->groupAltWebUI->isChecked());
        pref->setWebUiRootFolder(m_ui->textWebUIRootFolder->selectedPath());
        // Custom HTTP headers
        pref->setWebUICustomHTTPHeadersEnabled(m_ui->groupWebUIAddCustomHTTPHeaders->isChecked());
        pref->setWebUICustomHTTPHeaders(m_ui->textWebUICustomHTTPHeaders->toPlainText());
        // Reverse proxy
        pref->setWebUIReverseProxySupportEnabled(m_ui->groupEnableReverseProxySupport->isChecked());
        pref->setWebUITrustedReverseProxiesList(m_ui->textTrustedReverseProxiesList->text());
    }
    // End Web UI
    // End preferences
    // Save advanced settings
    m_advancedSettings->saveAdvancedSettings();
    // Assume that user changed multiple settings
    // so it's best to save immediately
    pref->apply();
}

bool OptionsDialog::isIPFilteringEnabled() const
{
    return m_ui->checkIPFilter->isChecked();
}

Net::ProxyType OptionsDialog::getProxyType() const
{
    switch (m_ui->comboProxyType->currentIndex())
    {
    case 1:
        return Net::ProxyType::SOCKS4;
    case 2:
        if (isProxyAuthEnabled())
            return Net::ProxyType::SOCKS5_PW;
        return Net::ProxyType::SOCKS5;
    case 3:
        if (isProxyAuthEnabled())
            return Net::ProxyType::HTTP_PW;
        return Net::ProxyType::HTTP;
    default:
        return Net::ProxyType::None;
    }
}

void OptionsDialog::loadOptions()
{
    int intValue;
    QString strValue;
    bool fileLogBackup = true;
    bool fileLogDelete = true;
    const Preferences *const pref = Preferences::instance();

    // Behavior preferences
    setLocale(pref->getLocale());
    m_ui->confirmDeletion->setChecked(pref->confirmTorrentDeletion());
    m_ui->checkAltRowColors->setChecked(pref->useAlternatingRowColors());
    m_ui->checkHideZero->setChecked(pref->getHideZeroValues());
    m_ui->comboHideZero->setEnabled(m_ui->checkHideZero->isChecked());
    m_ui->comboHideZero->setCurrentIndex(pref->getHideZeroComboValues());

    m_ui->checkShowSplash->setChecked(!pref->isSplashScreenDisabled());
    m_ui->checkStartMinimized->setChecked(pref->startMinimized());
    m_ui->checkProgramExitConfirm->setChecked(pref->confirmOnExit());
    m_ui->checkProgramAutoExitConfirm->setChecked(!pref->dontConfirmAutoExit());

#ifndef Q_OS_MACOS
    m_ui->checkShowSystray->setChecked(pref->systrayIntegration());
    if (m_ui->checkShowSystray->isChecked())
    {
        m_ui->checkMinimizeToSysTray->setChecked(pref->minimizeToTray());
        m_ui->checkCloseToSystray->setChecked(pref->closeToTray());
        m_ui->comboTrayIcon->setCurrentIndex(pref->trayIconStyle());
    }
#endif

    m_ui->checkPreventFromSuspendWhenDownloading->setChecked(pref->preventFromSuspendWhenDownloading());
    m_ui->checkPreventFromSuspendWhenSeeding->setChecked(pref->preventFromSuspendWhenSeeding());

#ifdef Q_OS_WIN
    m_ui->checkStartup->setChecked(pref->WinStartup());
    m_ui->checkAssociateTorrents->setChecked(Preferences::isTorrentFileAssocSet());
    m_ui->checkAssociateMagnetLinks->setChecked(Preferences::isMagnetLinkAssocSet());
#endif
#ifdef Q_OS_MACOS
    m_ui->checkAssociateTorrents->setChecked(Preferences::isTorrentFileAssocSet());
    m_ui->checkAssociateTorrents->setEnabled(!m_ui->checkAssociateTorrents->isChecked());
    m_ui->checkAssociateMagnetLinks->setChecked(Preferences::isMagnetLinkAssocSet());
    m_ui->checkAssociateMagnetLinks->setEnabled(!m_ui->checkAssociateMagnetLinks->isChecked());
#endif
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    m_ui->checkProgramUpdates->setChecked(pref->isUpdateCheckEnabled());
#endif

    const Application *const app = static_cast<Application*>(QCoreApplication::instance());
    m_ui->checkFileLog->setChecked(app->isFileLoggerEnabled());
    m_ui->textFileLogPath->setSelectedPath(app->fileLoggerPath());
    fileLogBackup = app->isFileLoggerBackup();
    m_ui->checkFileLogBackup->setChecked(fileLogBackup);
    m_ui->spinFileLogSize->setEnabled(fileLogBackup);
    fileLogDelete = app->isFileLoggerDeleteOld();
    m_ui->checkFileLogDelete->setChecked(fileLogDelete);
    m_ui->spinFileLogAge->setEnabled(fileLogDelete);
    m_ui->comboFileLogAgeType->setEnabled(fileLogDelete);
    m_ui->spinFileLogSize->setValue(app->fileLoggerMaxSize() / 1024);
    m_ui->spinFileLogAge->setValue(app->fileLoggerAge());
    m_ui->comboFileLogAgeType->setCurrentIndex(app->fileLoggerAgeType());
    // End Behavior preferences

    m_ui->checkRSSEnable->setChecked(RSS::Session::instance()->isProcessingEnabled());
    m_ui->checkRSSAutoDownloaderEnable->setChecked(RSS::AutoDownloader::instance()->isProcessingEnabled());
    m_ui->textSmartEpisodeFilters->setPlainText(RSS::AutoDownloader::instance()->smartEpisodeFilters().join('\n'));
    m_ui->checkSmartFilterDownloadRepacks->setChecked(RSS::AutoDownloader::instance()->downloadRepacks());

    m_ui->spinRSSRefreshInterval->setValue(RSS::Session::instance()->refreshInterval());
    m_ui->spinRSSMaxArticlesPerFeed->setValue(RSS::Session::instance()->maxArticlesPerFeed());

    const auto *session = BitTorrent::Session::instance();

    // Downloads preferences
    m_ui->checkAdditionDialog->setChecked(AddNewTorrentDialog::isEnabled());
    m_ui->checkAdditionDialogFront->setChecked(AddNewTorrentDialog::isTopLevel());
    m_ui->checkStartPaused->setChecked(session->isAddTorrentPaused());
    m_ui->contentLayoutComboBox->setCurrentIndex(static_cast<int>(session->torrentContentLayout()));
    const TorrentFileGuard::AutoDeleteMode autoDeleteMode = TorrentFileGuard::autoDeleteMode();
    m_ui->deleteTorrentBox->setChecked(autoDeleteMode != TorrentFileGuard::Never);
    m_ui->deleteCancelledTorrentBox->setChecked(autoDeleteMode == TorrentFileGuard::Always);

    m_ui->textSavePath->setSelectedPath(session->defaultSavePath());
    m_ui->checkUseSubcategories->setChecked(session->isSubcategoriesEnabled());
    m_ui->comboSavingMode->setCurrentIndex(!session->isAutoTMMDisabledByDefault());
    m_ui->comboTorrentCategoryChanged->setCurrentIndex(session->isDisableAutoTMMWhenCategoryChanged());
    m_ui->comboCategoryChanged->setCurrentIndex(session->isDisableAutoTMMWhenCategorySavePathChanged());
    m_ui->comboCategoryDefaultPathChanged->setCurrentIndex(session->isDisableAutoTMMWhenDefaultSavePathChanged());
    m_ui->checkTempFolder->setChecked(session->isTempPathEnabled());
    m_ui->textTempPath->setEnabled(m_ui->checkTempFolder->isChecked());
    m_ui->textTempPath->setEnabled(m_ui->checkTempFolder->isChecked());
    m_ui->textTempPath->setSelectedPath(Utils::Fs::toNativePath(session->tempPath()));
    m_ui->checkAppendqB->setChecked(session->isAppendExtensionEnabled());
    m_ui->checkPreallocateAll->setChecked(session->isPreallocationEnabled());
    m_ui->checkRecursiveDownload->setChecked(!pref->recursiveDownloadDisabled());

    strValue = session->torrentExportDirectory();
    if (strValue.isEmpty())
    {
        // Disable
        m_ui->checkExportDir->setChecked(false);
        m_ui->textExportDir->setEnabled(false);
    }
    else
    {
        // Enable
        m_ui->checkExportDir->setChecked(true);
        m_ui->textExportDir->setEnabled(true);
        m_ui->textExportDir->setSelectedPath(strValue);
    }

    strValue = session->finishedTorrentExportDirectory();
    if (strValue.isEmpty())
    {
        // Disable
        m_ui->checkExportDirFin->setChecked(false);
        m_ui->textExportDirFin->setEnabled(false);
    }
    else
    {
        // Enable
        m_ui->checkExportDirFin->setChecked(true);
        m_ui->textExportDirFin->setEnabled(true);
        m_ui->textExportDirFin->setSelectedPath(strValue);
    }

    m_ui->groupMailNotification->setChecked(pref->isMailNotificationEnabled());
    m_ui->senderEmailTxt->setText(pref->getMailNotificationSender());
    m_ui->lineEditDestEmail->setText(pref->getMailNotificationEmail());
    m_ui->lineEditSmtpServer->setText(pref->getMailNotificationSMTP());
    m_ui->checkSmtpSSL->setChecked(pref->getMailNotificationSMTPSSL());
    m_ui->groupMailNotifAuth->setChecked(pref->getMailNotificationSMTPAuth());
    m_ui->mailNotifUsername->setText(pref->getMailNotificationSMTPUsername());
    m_ui->mailNotifPassword->setText(pref->getMailNotificationSMTPPassword());

    m_ui->autoRunBox->setChecked(pref->isAutoRunEnabled());
    m_ui->lineEditAutoRun->setText(pref->getAutoRunProgram());
#if defined(Q_OS_WIN)
    m_ui->autoRunConsole->setChecked(pref->isAutoRunConsoleEnabled());
#else
    m_ui->autoRunConsole->hide();
#endif
    intValue = pref->getActionOnDblClOnTorrentDl();
    if (intValue >= m_ui->actionTorrentDlOnDblClBox->count())
        intValue = 0;
    m_ui->actionTorrentDlOnDblClBox->setCurrentIndex(intValue);
    intValue = pref->getActionOnDblClOnTorrentFn();
    if (intValue >= m_ui->actionTorrentFnOnDblClBox->count())
        intValue = 1;
    m_ui->actionTorrentFnOnDblClBox->setCurrentIndex(intValue);
    // End Downloads preferences

    // Connection preferences
    m_ui->comboProtocol->setCurrentIndex(static_cast<int>(session->btProtocol()));
    m_ui->spinPort->setValue(session->port());
    m_ui->checkUPnP->setChecked(Net::PortForwarder::instance()->isEnabled());

    intValue = session->maxConnections();
    if (intValue > 0)
    {
        // enable
        m_ui->checkMaxConnecs->setChecked(true);
        m_ui->spinMaxConnec->setEnabled(true);
        m_ui->spinMaxConnec->setValue(intValue);
    }
    else
    {
        // disable
        m_ui->checkMaxConnecs->setChecked(false);
        m_ui->spinMaxConnec->setEnabled(false);
    }
    intValue = session->maxConnectionsPerTorrent();
    if (intValue > 0)
    {
        // enable
        m_ui->checkMaxConnecsPerTorrent->setChecked(true);
        m_ui->spinMaxConnecPerTorrent->setEnabled(true);
        m_ui->spinMaxConnecPerTorrent->setValue(intValue);
    }
    else
    {
        // disable
        m_ui->checkMaxConnecsPerTorrent->setChecked(false);
        m_ui->spinMaxConnecPerTorrent->setEnabled(false);
    }
    intValue = session->maxUploads();
    if (intValue > 0)
    {
        // enable
        m_ui->checkMaxUploads->setChecked(true);
        m_ui->spinMaxUploads->setEnabled(true);
        m_ui->spinMaxUploads->setValue(intValue);
    }
    else
    {
        // disable
        m_ui->checkMaxUploads->setChecked(false);
        m_ui->spinMaxUploads->setEnabled(false);
    }
    intValue = session->maxUploadsPerTorrent();
    if (intValue > 0)
    {
        // enable
        m_ui->checkMaxUploadsPerTorrent->setChecked(true);
        m_ui->spinMaxUploadsPerTorrent->setEnabled(true);
        m_ui->spinMaxUploadsPerTorrent->setValue(intValue);
    }
    else
    {
        // disable
        m_ui->checkMaxUploadsPerTorrent->setChecked(false);
        m_ui->spinMaxUploadsPerTorrent->setEnabled(false);
    }

    const auto *proxyConfigManager = Net::ProxyConfigurationManager::instance();
    Net::ProxyConfiguration proxyConf = proxyConfigManager->proxyConfiguration();
    using Net::ProxyType;
    bool useProxyAuth = false;
    switch (proxyConf.type)
    {
    case ProxyType::SOCKS4:
        m_ui->comboProxyType->setCurrentIndex(1);
        break;

    case ProxyType::SOCKS5_PW:
        useProxyAuth = true;
        // fallthrough
    case ProxyType::SOCKS5:
        m_ui->comboProxyType->setCurrentIndex(2);
        break;

    case ProxyType::HTTP_PW:
        useProxyAuth = true;
        // fallthrough
    case ProxyType::HTTP:
        m_ui->comboProxyType->setCurrentIndex(3);
        break;

    default:
        m_ui->comboProxyType->setCurrentIndex(0);
    }
    m_ui->textProxyIP->setText(proxyConf.ip);
    m_ui->spinProxyPort->setValue(proxyConf.port);
    m_ui->checkProxyAuth->setChecked(useProxyAuth);
    m_ui->textProxyUsername->setText(proxyConf.username);
    m_ui->textProxyPassword->setText(proxyConf.password);

    m_ui->checkProxyPeerConnecs->setChecked(session->isProxyPeerConnectionsEnabled());
    m_ui->isProxyOnlyForTorrents->setChecked(proxyConfigManager->isProxyOnlyForTorrents());
    enableProxy(m_ui->comboProxyType->currentIndex());

    m_ui->checkIPFilter->setChecked(session->isIPFilteringEnabled());
    m_ui->textFilterPath->setEnabled(m_ui->checkIPFilter->isChecked());
    m_ui->textFilterPath->setSelectedPath(session->IPFilterFile());
    m_ui->IpFilterRefreshBtn->setEnabled(m_ui->checkIPFilter->isChecked());
    m_ui->checkIpFilterTrackers->setChecked(session->isTrackerFilteringEnabled());
    // End Connection preferences

    // Speed preferences
    m_ui->spinDownloadLimit->setValue(session->globalDownloadSpeedLimit() / 1024);
    m_ui->spinUploadLimit->setValue(session->globalUploadSpeedLimit() / 1024);
    m_ui->spinDownloadLimitAlt->setValue(session->altGlobalDownloadSpeedLimit() / 1024);
    m_ui->spinUploadLimitAlt->setValue(session->altGlobalUploadSpeedLimit() / 1024);

    m_ui->checkLimituTPConnections->setChecked(session->isUTPRateLimited());
    m_ui->checkLimitTransportOverhead->setChecked(session->includeOverheadInLimits());
    m_ui->checkLimitLocalPeerRate->setChecked(!session->ignoreLimitsOnLAN());

    m_ui->groupBoxSchedule->setChecked(session->isBandwidthSchedulerEnabled());
    m_ui->timeEditScheduleFrom->setTime(pref->getSchedulerStartTime());
    m_ui->timeEditScheduleTo->setTime(pref->getSchedulerEndTime());
    m_ui->comboBoxScheduleDays->setCurrentIndex(static_cast<int>(pref->getSchedulerDays()));
    // End Speed preferences

    // Bittorrent preferences
    m_ui->checkDHT->setChecked(session->isDHTEnabled());
    m_ui->checkPeX->setChecked(session->isPeXEnabled());
    m_ui->checkLSD->setChecked(session->isLSDEnabled());
    m_ui->comboEncryption->setCurrentIndex(session->encryption());
    m_ui->checkAnonymousMode->setChecked(session->isAnonymousModeEnabled());
    m_ui->checkEnableAddTrackers->setChecked(session->isAddTrackersEnabled());
    m_ui->textTrackers->setPlainText(session->additionalTrackers());

    m_ui->checkEnableQueueing->setChecked(session->isQueueingSystemEnabled());
    m_ui->spinMaxActiveDownloads->setValue(session->maxActiveDownloads());
    m_ui->spinMaxActiveUploads->setValue(session->maxActiveUploads());
    m_ui->spinMaxActiveTorrents->setValue(session->maxActiveTorrents());
    m_ui->checkIgnoreSlowTorrentsForQueueing->setChecked(session->ignoreSlowTorrentsForQueueing());
    m_ui->spinDownloadRateForSlowTorrents->setValue(session->downloadRateForSlowTorrents());
    m_ui->spinUploadRateForSlowTorrents->setValue(session->uploadRateForSlowTorrents());
    m_ui->spinSlowTorrentsInactivityTimer->setValue(session->slowTorrentsInactivityTimer());

    if (session->globalMaxRatio() >= 0.)
    {
        // Enable
        m_ui->checkMaxRatio->setChecked(true);
        m_ui->spinMaxRatio->setEnabled(true);
        m_ui->comboRatioLimitAct->setEnabled(true);
        m_ui->spinMaxRatio->setValue(session->globalMaxRatio());
    }
    else
    {
        // Disable
        m_ui->checkMaxRatio->setChecked(false);
        m_ui->spinMaxRatio->setEnabled(false);
    }
    if (session->globalMaxSeedingMinutes() >= 0)
    {
        // Enable
        m_ui->checkMaxSeedingMinutes->setChecked(true);
        m_ui->spinMaxSeedingMinutes->setEnabled(true);
        m_ui->spinMaxSeedingMinutes->setValue(session->globalMaxSeedingMinutes());
    }
    else
    {
        // Disable
        m_ui->checkMaxSeedingMinutes->setChecked(false);
        m_ui->spinMaxSeedingMinutes->setEnabled(false);
    }
    m_ui->comboRatioLimitAct->setEnabled((session->globalMaxSeedingMinutes() >= 0) || (session->globalMaxRatio() >= 0.));

    const QHash<MaxRatioAction, int> actIndex =
    {
        {Pause, 0},
        {Remove, 1},
        {DeleteFiles, 2},
        {EnableSuperSeeding, 3}
    };
    m_ui->comboRatioLimitAct->setCurrentIndex(actIndex.value(session->maxRatioAction()));
    // End Bittorrent preferences

    // Web UI preferences
    m_ui->textServerDomains->setText(pref->getServerDomains());
    m_ui->checkWebUi->setChecked(pref->isWebUiEnabled());
    m_ui->textWebUiAddress->setText(pref->getWebUiAddress());
    m_ui->spinWebUiPort->setValue(pref->getWebUiPort());
    m_ui->checkWebUIUPnP->setChecked(pref->useUPnPForWebUIPort());
    m_ui->checkWebUiHttps->setChecked(pref->isWebUiHttpsEnabled());
    webUIHttpsCertChanged(pref->getWebUIHttpsCertificatePath(), ShowError::NotShow);
    webUIHttpsKeyChanged(pref->getWebUIHttpsKeyPath(), ShowError::NotShow);
    m_ui->textWebUiUsername->setText(pref->getWebUiUsername());
    m_ui->checkBypassLocalAuth->setChecked(!pref->isWebUiLocalAuthEnabled());
    m_ui->checkBypassAuthSubnetWhitelist->setChecked(pref->isWebUiAuthSubnetWhitelistEnabled());
    m_ui->IPSubnetWhitelistButton->setEnabled(m_ui->checkBypassAuthSubnetWhitelist->isChecked());
    m_ui->spinBanCounter->setValue(pref->getWebUIMaxAuthFailCount());
    m_ui->spinBanDuration->setValue(pref->getWebUIBanDuration().count());
    m_ui->spinSessionTimeout->setValue(pref->getWebUISessionTimeout());

    // Security
    m_ui->checkClickjacking->setChecked(pref->isWebUiClickjackingProtectionEnabled());
    m_ui->checkCSRFProtection->setChecked(pref->isWebUiCSRFProtectionEnabled());
    m_ui->checkSecureCookie->setEnabled(pref->isWebUiHttpsEnabled());
    m_ui->checkSecureCookie->setChecked(pref->isWebUiSecureCookieEnabled());
    m_ui->groupHostHeaderValidation->setChecked(pref->isWebUIHostHeaderValidationEnabled());

    m_ui->checkDynDNS->setChecked(pref->isDynDNSEnabled());
    m_ui->comboDNSService->setCurrentIndex(static_cast<int>(pref->getDynDNSService()));
    m_ui->domainNameTxt->setText(pref->getDynDomainName());
    m_ui->DNSUsernameTxt->setText(pref->getDynDNSUsername());
    m_ui->DNSPasswordTxt->setText(pref->getDynDNSPassword());

    m_ui->groupAltWebUI->setChecked(pref->isAltWebUiEnabled());
    m_ui->textWebUIRootFolder->setSelectedPath(pref->getWebUiRootFolder());
    // Custom HTTP headers
    m_ui->groupWebUIAddCustomHTTPHeaders->setChecked(pref->isWebUICustomHTTPHeadersEnabled());
    m_ui->textWebUICustomHTTPHeaders->setPlainText(pref->getWebUICustomHTTPHeaders());
    // Reverse proxy
    m_ui->groupEnableReverseProxySupport->setChecked(pref->isWebUIReverseProxySupportEnabled());
    m_ui->textTrustedReverseProxiesList->setText(pref->getWebUITrustedReverseProxiesList());
    // End Web UI preferences
}

// return min & max ports
// [min, max]
int OptionsDialog::getPort() const
{
    return m_ui->spinPort->value();
}

void OptionsDialog::on_randomButton_clicked()
{
    // Range [1024: 65535]
    m_ui->spinPort->setValue(Utils::Random::rand(1024, 65535));
}

int OptionsDialog::getEncryptionSetting() const
{
    return m_ui->comboEncryption->currentIndex();
}

int OptionsDialog::getMaxActiveDownloads() const
{
    return m_ui->spinMaxActiveDownloads->value();
}

int OptionsDialog::getMaxActiveUploads() const
{
    return m_ui->spinMaxActiveUploads->value();
}

int OptionsDialog::getMaxActiveTorrents() const
{
    return m_ui->spinMaxActiveTorrents->value();
}

bool OptionsDialog::isQueueingSystemEnabled() const
{
    return m_ui->checkEnableQueueing->isChecked();
}

bool OptionsDialog::isDHTEnabled() const
{
    return m_ui->checkDHT->isChecked();
}

bool OptionsDialog::isLSDEnabled() const
{
    return m_ui->checkLSD->isChecked();
}

bool OptionsDialog::isUPnPEnabled() const
{
    return m_ui->checkUPnP->isChecked();
}

bool OptionsDialog::startMinimized() const
{
    return m_ui->checkStartMinimized->isChecked();
}

#ifndef Q_OS_MACOS
bool OptionsDialog::systrayIntegration() const
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return false;
    return m_ui->checkShowSystray->isChecked();
}

bool OptionsDialog::minimizeToTray() const
{
    if (!m_ui->checkShowSystray->isChecked()) return false;
    return m_ui->checkMinimizeToSysTray->isChecked();
}

bool OptionsDialog::closeToTray() const
{
    if (!m_ui->checkShowSystray->isChecked()) return false;
    return m_ui->checkCloseToSystray->isChecked();
}
#endif // Q_OS_MACOS

// Return Share ratio
qreal OptionsDialog::getMaxRatio() const
{
    if (m_ui->checkMaxRatio->isChecked())
        return m_ui->spinMaxRatio->value();
    return -1;
}

// Return Seeding Minutes
int OptionsDialog::getMaxSeedingMinutes() const
{
    if (m_ui->checkMaxSeedingMinutes->isChecked())
        return m_ui->spinMaxSeedingMinutes->value();
    return -1;
}

// Return max connections number
int OptionsDialog::getMaxConnecs() const
{
    if (!m_ui->checkMaxConnecs->isChecked())
        return -1;

    return m_ui->spinMaxConnec->value();
}

int OptionsDialog::getMaxConnecsPerTorrent() const
{
    if (!m_ui->checkMaxConnecsPerTorrent->isChecked())
        return -1;

    return m_ui->spinMaxConnecPerTorrent->value();
}

int OptionsDialog::getMaxUploads() const
{
    if (!m_ui->checkMaxUploads->isChecked())
        return -1;

    return m_ui->spinMaxUploads->value();
}

int OptionsDialog::getMaxUploadsPerTorrent() const
{
    if (!m_ui->checkMaxUploadsPerTorrent->isChecked())
        return -1;

    return m_ui->spinMaxUploadsPerTorrent->value();
}

void OptionsDialog::on_buttonBox_accepted()
{
    if (m_applyButton->isEnabled())
    {
        if (!schedTimesOk())
        {
            m_ui->tabSelection->setCurrentRow(TAB_SPEED);
            return;
        }
        if (!webUIAuthenticationOk())
        {
            m_ui->tabSelection->setCurrentRow(TAB_WEBUI);
            return;
        }
        if (!isAlternativeWebUIPathValid())
        {
            m_ui->tabSelection->setCurrentRow(TAB_WEBUI);
            return;
        }
        m_applyButton->setEnabled(false);
        this->hide();
        saveOptions();
    }

    accept();
}

void OptionsDialog::applySettings()
{
    if (!schedTimesOk())
    {
        m_ui->tabSelection->setCurrentRow(TAB_SPEED);
        return;
    }
    if (!webUIAuthenticationOk())
    {
        m_ui->tabSelection->setCurrentRow(TAB_WEBUI);
        return;
    }
    if (!isAlternativeWebUIPathValid())
    {
        m_ui->tabSelection->setCurrentRow(TAB_WEBUI);
        return;
    }
    saveOptions();
}

void OptionsDialog::closeEvent(QCloseEvent *e)
{
    setAttribute(Qt::WA_DeleteOnClose);
    e->accept();
}

void OptionsDialog::on_buttonBox_rejected()
{
    setAttribute(Qt::WA_DeleteOnClose);
    reject();
}

bool OptionsDialog::useAdditionDialog() const
{
    return m_ui->checkAdditionDialog->isChecked();
}

void OptionsDialog::enableApplyButton()
{
    m_applyButton->setEnabled(true);
}

void OptionsDialog::toggleComboRatioLimitAct()
{
    // Verify if the share action button must be enabled
    m_ui->comboRatioLimitAct->setEnabled(m_ui->checkMaxRatio->isChecked() || m_ui->checkMaxSeedingMinutes->isChecked());
}

void OptionsDialog::enableProxy(const int index)
{
    if (index >= 1)
    { // Any proxy type is used
        //enable
        m_ui->lblProxyIP->setEnabled(true);
        m_ui->textProxyIP->setEnabled(true);
        m_ui->lblProxyPort->setEnabled(true);
        m_ui->spinProxyPort->setEnabled(true);
        m_ui->checkProxyPeerConnecs->setEnabled(true);
        if (index >= 2)
        { // SOCKS5 or HTTP
            m_ui->checkProxyAuth->setEnabled(true);
            m_ui->isProxyOnlyForTorrents->setEnabled(true);
        }
        else
        {
            m_ui->checkProxyAuth->setEnabled(false);
            m_ui->isProxyOnlyForTorrents->setEnabled(false);
            m_ui->isProxyOnlyForTorrents->setChecked(true);
        }
    }
    else
    { // No proxy
        // disable
        m_ui->lblProxyIP->setEnabled(false);
        m_ui->textProxyIP->setEnabled(false);
        m_ui->lblProxyPort->setEnabled(false);
        m_ui->spinProxyPort->setEnabled(false);
        m_ui->checkProxyPeerConnecs->setEnabled(false);
        m_ui->isProxyOnlyForTorrents->setEnabled(false);
        m_ui->checkProxyAuth->setEnabled(false);
    }
}

bool OptionsDialog::isSplashScreenDisabled() const
{
    return !m_ui->checkShowSplash->isChecked();
}

#ifdef Q_OS_WIN
bool OptionsDialog::WinStartup() const
{
    return m_ui->checkStartup->isChecked();
}
#endif

bool OptionsDialog::preAllocateAllFiles() const
{
    return m_ui->checkPreallocateAll->isChecked();
}

bool OptionsDialog::addTorrentsInPause() const
{
    return m_ui->checkStartPaused->isChecked();
}

// Proxy settings
bool OptionsDialog::isProxyEnabled() const
{
    return m_ui->comboProxyType->currentIndex();
}

bool OptionsDialog::isProxyAuthEnabled() const
{
    return m_ui->checkProxyAuth->isChecked();
}

QString OptionsDialog::getProxyIp() const
{
    return m_ui->textProxyIP->text().trimmed();
}

unsigned short OptionsDialog::getProxyPort() const
{
    return m_ui->spinProxyPort->value();
}

QString OptionsDialog::getProxyUsername() const
{
    QString username = m_ui->textProxyUsername->text().trimmed();
    return username;
}

QString OptionsDialog::getProxyPassword() const
{
    QString password = m_ui->textProxyPassword->text();
    password = password.trimmed();
    return password;
}

// Locale Settings
QString OptionsDialog::getLocale() const
{
    return m_ui->comboI18n->itemData(m_ui->comboI18n->currentIndex(), Qt::UserRole).toString();
}

void OptionsDialog::setLocale(const QString &localeStr)
{
    QString name;
    if (localeStr.startsWith("eo", Qt::CaseInsensitive))
    {
        name = "eo";
    }
    else if (localeStr.startsWith("ltg", Qt::CaseInsensitive))
    {
        name = "ltg";
    }
    else
    {
        QLocale locale(localeStr);
        if (locale.language() == QLocale::Uzbek)
            name = "uz@Latn";
        else if (locale.language() == QLocale::Azerbaijani)
            name = "az@latin";
        else
            name = locale.name();
    }
    // Attempt to find exact match
    int index = m_ui->comboI18n->findData(name, Qt::UserRole);
    if (index < 0)
    {
        //Attempt to find a language match without a country
        int pos = name.indexOf('_');
        if (pos > -1)
        {
            QString lang = name.left(pos);
            index = m_ui->comboI18n->findData(lang, Qt::UserRole);
        }
    }
    if (index < 0)
    {
        // Unrecognized, use US English
        index = m_ui->comboI18n->findData("en", Qt::UserRole);
        Q_ASSERT(index >= 0);
    }
    m_ui->comboI18n->setCurrentIndex(index);
}

QString OptionsDialog::getTorrentExportDir() const
{
    if (m_ui->checkExportDir->isChecked())
        return Utils::Fs::expandPathAbs(m_ui->textExportDir->selectedPath());
    return {};
}

QString OptionsDialog::getFinishedTorrentExportDir() const
{
    if (m_ui->checkExportDirFin->isChecked())
        return Utils::Fs::expandPathAbs(m_ui->textExportDirFin->selectedPath());
    return {};
}

// Return action on double-click on a downloading torrent set in options
int OptionsDialog::getActionOnDblClOnTorrentDl() const
{
    if (m_ui->actionTorrentDlOnDblClBox->currentIndex() < 1)
        return 0;
    return m_ui->actionTorrentDlOnDblClBox->currentIndex();
}

// Return action on double-click on a finished torrent set in options
int OptionsDialog::getActionOnDblClOnTorrentFn() const
{
    if (m_ui->actionTorrentFnOnDblClBox->currentIndex() < 1)
        return 0;
    return m_ui->actionTorrentFnOnDblClBox->currentIndex();
}

void OptionsDialog::on_addWatchedFolderButton_clicked()
{
    Preferences *const pref = Preferences::instance();
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select folder to monitor"),
        Utils::Fs::toNativePath(Utils::Fs::folderName(pref->getScanDirsLastPath())));
    if (dir.isEmpty())
        return;

    auto dialog = new WatchedFolderOptionsDialog({}, this);
    dialog->setModal(true);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this, dialog, dir, pref]()
    {
        try
        {
            auto watchedFoldersModel = static_cast<WatchedFoldersModel *>(m_ui->scanFoldersView->model());
            watchedFoldersModel->addFolder(dir, dialog->watchedFolderOptions());

            pref->setScanDirsLastPath(dir);

            for (int i = 0; i < watchedFoldersModel->columnCount(); ++i)
                m_ui->scanFoldersView->resizeColumnToContents(i);

            enableApplyButton();
        }
        catch (const RuntimeError &err)
        {
            QMessageBox::critical(this, tr("Adding entry failed"), err.message());
        }
    });

    dialog->open();
}

void OptionsDialog::on_editWatchedFolderButton_clicked()
{
    const QModelIndex selected
        = m_ui->scanFoldersView->selectionModel()->selectedIndexes().at(0);

    editWatchedFolderOptions(selected);
}

void OptionsDialog::on_removeWatchedFolderButton_clicked()
{
    const QModelIndexList selected
        = m_ui->scanFoldersView->selectionModel()->selectedIndexes();

    for (const QModelIndex &index : selected)
        m_ui->scanFoldersView->model()->removeRow(index.row());
}

void OptionsDialog::handleWatchedFolderViewSelectionChanged()
{
    const QModelIndexList selectedIndexes = m_ui->scanFoldersView->selectionModel()->selectedIndexes();
    m_ui->removeWatchedFolderButton->setEnabled(!selectedIndexes.isEmpty());
    m_ui->editWatchedFolderButton->setEnabled(selectedIndexes.count() == 1);
}

void OptionsDialog::editWatchedFolderOptions(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    auto watchedFoldersModel = static_cast<WatchedFoldersModel *>(m_ui->scanFoldersView->model());
    auto dialog = new WatchedFolderOptionsDialog(watchedFoldersModel->folderOptions(index.row()), this);
    dialog->setModal(true);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this, dialog, index, watchedFoldersModel]()
    {
        if (index.isValid())
        {
            // The index could be invalidated while the dialog was displayed,
            // for example, if you deleted the folder using the Web API.
            watchedFoldersModel->setFolderOptions(index.row(), dialog->watchedFolderOptions());
            enableApplyButton();
        }
    });

    dialog->open();
}

QString OptionsDialog::askForExportDir(const QString &currentExportPath)
{
    QDir currentExportDir(Utils::Fs::expandPathAbs(currentExportPath));
    QString dir;
    if (!currentExportPath.isEmpty() && currentExportDir.exists())
        dir = QFileDialog::getExistingDirectory(this, tr("Choose export directory"), currentExportDir.absolutePath());
    else
        dir = QFileDialog::getExistingDirectory(this, tr("Choose export directory"), QDir::homePath());
    return dir;
}

// Return Filter object to apply to BT session
QString OptionsDialog::getFilter() const
{
    return m_ui->textFilterPath->selectedPath();
}

// Web UI

bool OptionsDialog::isWebUiEnabled() const
{
    return m_ui->checkWebUi->isChecked();
}

QString OptionsDialog::webUiUsername() const
{
    return m_ui->textWebUiUsername->text();
}

QString OptionsDialog::webUiPassword() const
{
    return m_ui->textWebUiPassword->text();
}

void OptionsDialog::webUIHttpsCertChanged(const QString &path, const ShowError showError)
{
    m_ui->textWebUIHttpsCert->setSelectedPath(path);
    m_ui->lblSslCertStatus->setPixmap(Utils::Gui::scaledPixmapSvg(UIThemeManager::instance()->getIconPath(QLatin1String("security-low")), this, 24));

    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (showError == ShowError::Show)
            QMessageBox::warning(this, tr("Invalid path"), file.errorString());
        return;
    }

    if (!Utils::Net::isSSLCertificatesValid(file.read(Utils::Net::MAX_SSL_FILE_SIZE)))
    {
        if (showError == ShowError::Show)
            QMessageBox::warning(this, tr("Invalid certificate"), tr("This is not a valid SSL certificate."));
        return;
    }

    m_ui->lblSslCertStatus->setPixmap(Utils::Gui::scaledPixmapSvg(UIThemeManager::instance()->getIconPath(QLatin1String("security-high")), this, 24));
}

void OptionsDialog::webUIHttpsKeyChanged(const QString &path, const ShowError showError)
{
    m_ui->textWebUIHttpsKey->setSelectedPath(path);
    m_ui->lblSslKeyStatus->setPixmap(Utils::Gui::scaledPixmapSvg(UIThemeManager::instance()->getIconPath(QLatin1String("security-low")), this, 24));

    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (showError == ShowError::Show)
            QMessageBox::warning(this, tr("Invalid path"), file.errorString());
        return;
    }

    if (!Utils::Net::isSSLKeyValid(file.read(Utils::Net::MAX_SSL_FILE_SIZE)))
    {
        if (showError == ShowError::Show)
            QMessageBox::warning(this, tr("Invalid key"), tr("This is not a valid SSL key."));
        return;
    }

    m_ui->lblSslKeyStatus->setPixmap(Utils::Gui::scaledPixmapSvg(UIThemeManager::instance()->getIconPath(QLatin1String("security-high")), this, 24));
}

void OptionsDialog::showConnectionTab()
{
    m_ui->tabSelection->setCurrentRow(TAB_CONNECTION);
}

void OptionsDialog::on_registerDNSBtn_clicked()
{
    QDesktopServices::openUrl(Net::DNSUpdater::getRegistrationUrl(m_ui->comboDNSService->currentIndex()));
}

void OptionsDialog::on_IpFilterRefreshBtn_clicked()
{
    if (m_refreshingIpFilter) return;
    m_refreshingIpFilter = true;
    // Updating program preferences
    BitTorrent::Session *const session = BitTorrent::Session::instance();
    session->setIPFilteringEnabled(true);
    session->setIPFilterFile(""); // forcing Session reload filter file
    session->setIPFilterFile(getFilter());
    connect(session, &BitTorrent::Session::IPFilterParsed, this, &OptionsDialog::handleIPFilterParsed);
    setCursor(QCursor(Qt::WaitCursor));
}

void OptionsDialog::handleIPFilterParsed(bool error, int ruleCount)
{
    setCursor(QCursor(Qt::ArrowCursor));
    if (error)
        QMessageBox::warning(this, tr("Parsing error"), tr("Failed to parse the provided IP filter"));
    else
        QMessageBox::information(this, tr("Successfully refreshed"), tr("Successfully parsed the provided IP filter: %1 rules were applied.", "%1 is a number").arg(ruleCount));
    m_refreshingIpFilter = false;
    disconnect(BitTorrent::Session::instance(), &BitTorrent::Session::IPFilterParsed, this, &OptionsDialog::handleIPFilterParsed);
}

bool OptionsDialog::schedTimesOk()
{
    if (m_ui->timeEditScheduleFrom->time() == m_ui->timeEditScheduleTo->time())
    {
        QMessageBox::warning(this, tr("Time Error"), tr("The start time and the end time can't be the same."));
        return false;
    }
    return true;
}

bool OptionsDialog::webUIAuthenticationOk()
{
    if (webUiUsername().length() < 3)
    {
        QMessageBox::warning(this, tr("Length Error"), tr("The Web UI username must be at least 3 characters long."));
        return false;
    }
    if (!webUiPassword().isEmpty() && (webUiPassword().length() < 6))
    {
        QMessageBox::warning(this, tr("Length Error"), tr("The Web UI password must be at least 6 characters long."));
        return false;
    }
    return true;
}

bool OptionsDialog::isAlternativeWebUIPathValid()
{
    if (m_ui->groupAltWebUI->isChecked() && m_ui->textWebUIRootFolder->selectedPath().trimmed().isEmpty())
    {
        QMessageBox::warning(this, tr("Location Error"), tr("The alternative Web UI files location cannot be blank."));
        return false;
    }
    return true;
}

void OptionsDialog::on_banListButton_clicked()
{
    auto dialog = new BanListOptionsDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, &OptionsDialog::enableApplyButton);
    dialog->open();
}

void OptionsDialog::on_IPSubnetWhitelistButton_clicked()
{
    auto dialog = new IPSubnetWhitelistOptionsDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, &OptionsDialog::enableApplyButton);
    dialog->open();
}
