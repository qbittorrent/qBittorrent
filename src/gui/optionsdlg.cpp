/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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

#include "optionsdlg.h"

#include <cstdlib>

#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QTranslator>

#ifndef QT_NO_OPENSSL
#include <QSslCertificate>
#include <QSslKey>
#endif

#include "app/application.h"
#include "base/bittorrent/session.h"
#include "base/net/dnsupdater.h"
#include "base/net/portforwarder.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/preferences.h"
#include "base/rss/rss_autodownloader.h"
#include "base/rss/rss_session.h"
#include "base/scanfoldersmodel.h"
#include "base/torrentfileguard.h"
#include "base/unicodestrings.h"
#include "base/utils/fs.h"
#include "base/utils/random.h"
#include "addnewtorrentdialog.h"
#include "advancedsettings.h"
#include "rss/automatedrssdownloader.h"
#include "banlistoptions.h"
#include "guiiconprovider.h"
#include "scanfoldersdelegate.h"

#include "ui_optionsdlg.h"

// Constructor
OptionsDialog::OptionsDialog(QWidget *parent)
    : QDialog(parent)
    , m_refreshingIpFilter(false)
    , m_ui(new Ui::OptionsDialog)
{
    qDebug("-> Constructing Options");
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(true);

#if (defined(Q_OS_UNIX))
    setWindowTitle(tr("Preferences"));
#endif

    // Icons
    m_ui->tabSelection->item(TAB_UI)->setIcon(GuiIconProvider::instance()->getIcon("preferences-desktop"));
    m_ui->tabSelection->item(TAB_BITTORRENT)->setIcon(GuiIconProvider::instance()->getIcon("preferences-system-network"));
    m_ui->tabSelection->item(TAB_CONNECTION)->setIcon(GuiIconProvider::instance()->getIcon("network-wired"));
    m_ui->tabSelection->item(TAB_DOWNLOADS)->setIcon(GuiIconProvider::instance()->getIcon("folder-download"));
    m_ui->tabSelection->item(TAB_SPEED)->setIcon(GuiIconProvider::instance()->getIcon("speedometer", "chronometer"));
    m_ui->tabSelection->item(TAB_RSS)->setIcon(GuiIconProvider::instance()->getIcon("rss-config", "application-rss+xml"));
#ifndef DISABLE_WEBUI
    m_ui->tabSelection->item(TAB_WEBUI)->setIcon(GuiIconProvider::instance()->getIcon("network-server"));
#else
    m_ui->tabSelection->item(TAB_WEBUI)->setHidden(true);
#endif
    m_ui->tabSelection->item(TAB_ADVANCED)->setIcon(GuiIconProvider::instance()->getIcon("preferences-other"));
    for (int i = 0; i < m_ui->tabSelection->count(); ++i) {
        // uniform size for all icons
        m_ui->tabSelection->item(i)->setSizeHint(QSize(std::numeric_limits<int>::max(), 62));
    }

    m_ui->IpFilterRefreshBtn->setIcon(GuiIconProvider::instance()->getIcon("view-refresh"));

    m_ui->deleteTorrentWarningIcon->setPixmap(QApplication::style()->standardIcon(QStyle::SP_MessageBoxCritical).pixmap(16, 16));
    m_ui->deleteTorrentWarningIcon->hide();
    m_ui->deleteTorrentWarningLabel->hide();
    m_ui->deleteTorrentWarningLabel->setToolTip(QLatin1String("<html><body><p>") +
        tr("By enabling these options, you can <strong>irrevocably lose</strong> your .torrent files!") +
        QLatin1String("</p><p>") +
        tr("When these options are enabled, qBittorent will <strong>delete</strong> .torrent files "
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
    QList<QAbstractButton *> buttons = m_ui->buttonBox->buttons();
    foreach (QAbstractButton *button, buttons) {
        if (m_ui->buttonBox->buttonRole(button) == QDialogButtonBox::ApplyRole) {
            applyButton = button;
            break;
        }
    }

    m_ui->scanFoldersView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_ui->scanFoldersView->setModel(ScanFoldersModel::instance());
    m_ui->scanFoldersView->setItemDelegate(new ScanFoldersDelegate(this, m_ui->scanFoldersView));
    connect(ScanFoldersModel::instance(), &QAbstractListModel::dataChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->scanFoldersView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ThisType::handleScanFolderViewSelectionChanged);

    connect(m_ui->buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(applySettings(QAbstractButton*)));
    // Languages supported
    initializeLanguageCombo();

    // Load week days (scheduler)
    for (uint i = 1; i <= 7; ++i)
        m_ui->schedule_days->addItem(QDate::longDayName(i, QDate::StandaloneFormat));

    // Load options
    loadOptions();
#ifdef Q_OS_MAC
    m_ui->checkShowSystray->setVisible(false);
#else
    // Disable systray integration if it is not supported by the system
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        m_ui->checkShowSystray->setChecked(false);
        m_ui->checkShowSystray->setEnabled(false);
        m_ui->label_trayIconStyle->setVisible(false);
        m_ui->comboTrayIcon->setVisible(false);
    }
#endif

#if defined(QT_NO_OPENSSL)
    m_ui->checkWebUiHttps->setVisible(false);
    m_ui->checkSmtpSSL->setVisible(false);
#endif

#ifndef Q_OS_WIN
    m_ui->checkStartup->setVisible(false);
#endif

#if !(defined(Q_OS_WIN) || defined(Q_OS_MAC))
    m_ui->groupFileAssociation->setVisible(false);
#endif

    // Connect signals / slots
    // Shortcuts for frequently used signals that have more than one overload. They would require
    // type casts and that is why we declare required member pointer here instead.
    void (QComboBox::*qComboBoxCurrentIndexChanged)(int) = &QComboBox::currentIndexChanged;
    void (QSpinBox::*qSpinBoxValueChanged)(int) = &QSpinBox::valueChanged;

    connect(m_ui->comboProxyType, qComboBoxCurrentIndexChanged, this, &ThisType::enableProxy);
    connect(m_ui->checkRandomPort, &QAbstractButton::toggled, m_ui->spinPort, &ThisType::setDisabled);

    // Apply button is activated when a value is changed
    // General tab
    connect(m_ui->comboI18n, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
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
    connect(m_ui->checkPreventFromSuspend, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->comboTrayIcon, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC)) && !defined(QT_DBUS_LIB)
    m_ui->checkPreventFromSuspend->setDisabled(true);
#endif
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    connect(m_ui->checkAssociateTorrents, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkAssociateMagnetLinks, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
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
    connect(m_ui->checkAdditionDialog, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkAdditionDialogFront, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkStartPaused, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkCreateSubfolder, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
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
    connect(m_ui->addScanFolderButton, &QAbstractButton::clicked, this, &ThisType::enableApplyButton);
    connect(m_ui->removeScanFolderButton, &QAbstractButton::clicked, this, &ThisType::enableApplyButton);
    connect(m_ui->groupMailNotification, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->dest_email_txt, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->smtp_server_txt, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkSmtpSSL, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->groupMailNotifAuth, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->mailNotifUsername, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->mailNotifPassword, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->autoRunBox, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->autoRun_txt, &FileSystemPathEdit::selectedPathChanged, this, &ThisType::enableApplyButton);

    const QString autoRunStr = QString::fromUtf8("%1\n    %2\n    %3\n    %4\n    %5\n    %6\n    %7\n    %8\n    %9\n    %10\n%11")
                               .arg(tr("Supported parameters (case sensitive):"))
                               .arg(tr("%N: Torrent name"))
                               .arg(tr("%L: Category"))
                               .arg(tr("%F: Content path (same as root path for multifile torrent)"))
                               .arg(tr("%R: Root path (first torrent subdirectory path)"))
                               .arg(tr("%D: Save path"))
                               .arg(tr("%C: Number of files"))
                               .arg(tr("%Z: Torrent size (bytes)"))
                               .arg(tr("%T: Current tracker"))
                               .arg(tr("%I: Info hash"))
                               .arg(tr("Tip: Encapsulate parameter with quotation marks to avoid text being cut off at whitespace (e.g., \"%N\")"));
    m_ui->autoRun_param->setText(autoRunStr);

    // Connection tab
    connect(m_ui->comboProtocol, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinPort, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkRandomPort, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkUPnP, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkUploadLimit, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkDownloadLimit, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkUploadLimitAlt, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkDownloadLimitAlt, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->spinUploadLimit, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinDownloadLimit, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinUploadLimitAlt, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinDownloadLimitAlt, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->check_schedule, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->schedule_from, &QDateTimeEdit::timeChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->schedule_to, &QDateTimeEdit::timeChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->schedule_days, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
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
    connect(m_ui->spinMaxRatio, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, &ThisType::enableApplyButton);
    connect(m_ui->comboRatioLimitAct, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMaxSeedingMinutes, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMaxSeedingMinutes, &QAbstractButton::toggled, this, &ThisType::toggleComboRatioLimitAct);
    connect(m_ui->spinMaxSeedingMinutes, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &ThisType::enableApplyButton);
    // Proxy tab
    connect(m_ui->comboProxyType, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->textProxyIP, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinProxyPort, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkProxyPeerConnecs, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkForceProxy, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
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
    connect(m_ui->checkIgnoreSlowTorrentsForQueueing, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkEnableAddTrackers, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->textTrackers, &QPlainTextEdit::textChanged, this, &ThisType::enableApplyButton);
#ifndef DISABLE_WEBUI
    // Web UI tab
    connect(m_ui->textServerDomains, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkWebUi, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->spinWebUiPort, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkWebUIUPnP, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkWebUiHttps, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->btnWebUiKey, &QAbstractButton::clicked, this, &ThisType::enableApplyButton);
    connect(m_ui->btnWebUiCrt, &QAbstractButton::clicked, this, &ThisType::enableApplyButton);
    connect(m_ui->textWebUiUsername, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->textWebUiPassword, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkBypassLocalAuth, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkDynDNS, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->comboDNSService, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->domainNameTxt, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->DNSUsernameTxt, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->DNSPasswordTxt, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
#endif

    // RSS tab
    connect(m_ui->checkRSSEnable, &QCheckBox::toggled, this, &OptionsDialog::enableApplyButton);
    connect(m_ui->checkRSSAutoDownloaderEnable, &QCheckBox::toggled, this, &OptionsDialog::enableApplyButton);
    connect(m_ui->spinRSSRefreshInterval, qSpinBoxValueChanged, this, &OptionsDialog::enableApplyButton);
    connect(m_ui->spinRSSMaxArticlesPerFeed, qSpinBoxValueChanged, this, &OptionsDialog::enableApplyButton);
    connect(m_ui->btnEditRules, &QPushButton::clicked, [this]() { AutomatedRssDownloader(this).exec(); });

    // Disable apply Button
    applyButton->setEnabled(false);
    // Tab selection mechanism
    connect(m_ui->tabSelection, &QListWidget::currentItemChanged, this, &ThisType::changePage);
    // Load Advanced settings
    advancedSettings = new AdvancedSettings(m_ui->tabAdvancedPage);
    m_ui->advPageLayout->addWidget(advancedSettings);
    connect(advancedSettings, &AdvancedSettings::settingsChanged, this, &ThisType::enableApplyButton);

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

    show();
    loadWindowState();
}

void OptionsDialog::initializeLanguageCombo()
{
    // List language files
    const QDir langDir(":/lang");
    const QStringList langFiles = langDir.entryList(QStringList("qbittorrent_*.qm"), QDir::Files);
    foreach (const QString langFile, langFiles) {
        QString localeStr = langFile.mid(12); // remove "qbittorrent_"
        localeStr.chop(3); // Remove ".qm"
        QString languageName;
        if (localeStr.startsWith("eo", Qt::CaseInsensitive)) {
            // QLocale doesn't work with that locale. Esperanto isn't a "real" language.
            languageName = QString::fromUtf8(C_LOCALE_ESPERANTO);
        }
        else {
            QLocale locale(localeStr);
            languageName = languageToLocalizedString(locale);
        }
        m_ui->comboI18n->addItem(/*QIcon(":/icons/flags/"+country+".png"), */ languageName, localeStr);
        qDebug() << "Supported locale:" << localeStr;
    }
}

// Main destructor
OptionsDialog::~OptionsDialog()
{
    qDebug("-> destructing Options");
    foreach (const QString &path, addedScanDirs)
        ScanFoldersModel::instance()->removePath(path);
    ScanFoldersModel::instance()->configure(); // reloads "removed" paths
    delete m_ui;
}

void OptionsDialog::changePage(QListWidgetItem *current, QListWidgetItem *previous)
{
    if (!current)
        current = previous;
    m_ui->tabOption->setCurrentIndex(m_ui->tabSelection->row(current));
}

void OptionsDialog::loadWindowState()
{
    const Preferences* const pref = Preferences::instance();

    resize(pref->getPrefSize(this->size()));
    const QStringList sizes_str = pref->getPrefHSplitterSizes();
    QList<int> sizes;
    if (sizes_str.size() == 2) {
        sizes << sizes_str.first().toInt();
        sizes << sizes_str.last().toInt();
    }
    else {
        sizes << 116;
        sizes << m_ui->hsplitter->width() - 116;
    }
    m_ui->hsplitter->setSizes(sizes);
}

void OptionsDialog::saveWindowState() const
{
    Preferences* const pref = Preferences::instance();

    // window size
    pref->setPrefSize(size());

    // Splitter size
    QStringList sizes_str;
    sizes_str << QString::number(m_ui->hsplitter->sizes().first());
    sizes_str << QString::number(m_ui->hsplitter->sizes().last());
    pref->setPrefHSplitterSizes(sizes_str);
}

void OptionsDialog::saveOptions()
{
    applyButton->setEnabled(false);
    Preferences* const pref = Preferences::instance();
    // Load the translation
    QString locale = getLocale();
    if (pref->getLocale() != locale) {
        QTranslator *translator = new QTranslator;
        if (translator->load(QString::fromUtf8(":/lang/qbittorrent_") + locale))
            qDebug("%s locale recognized, using translation.", qUtf8Printable(locale));
        else
            qDebug("%s locale unrecognized, using default (en).", qUtf8Printable(locale));
        qApp->installTranslator(translator);
    }

    // General preferences
    pref->setLocale(locale);
    pref->setConfirmTorrentDeletion(m_ui->confirmDeletion->isChecked());
    pref->setAlternatingRowColors(m_ui->checkAltRowColors->isChecked());
    pref->setHideZeroValues(m_ui->checkHideZero->isChecked());
    pref->setHideZeroComboValues(m_ui->comboHideZero->currentIndex());
#ifndef Q_OS_MAC
    pref->setSystrayIntegration(systrayIntegration());
    pref->setTrayIconStyle(TrayIcon::Style(m_ui->comboTrayIcon->currentIndex()));
    pref->setCloseToTray(closeToTray());
    pref->setMinimizeToTray(minimizeToTray());
    pref->setStartMinimized(startMinimized());
#endif
    pref->setSplashScreenDisabled(isSlashScreenDisabled());
    pref->setConfirmOnExit(m_ui->checkProgramExitConfirm->isChecked());
    pref->setDontConfirmAutoExit(!m_ui->checkProgramAutoExitConfirm->isChecked());
    pref->setPreventFromSuspend(preventFromSuspend());
#ifdef Q_OS_WIN
    pref->setWinStartup(WinStartup());
    // Windows: file association settings
    Preferences::setTorrentFileAssoc(m_ui->checkAssociateTorrents->isChecked());
    Preferences::setMagnetLinkAssoc(m_ui->checkAssociateMagnetLinks->isChecked());
#endif
#ifdef Q_OS_MAC
    if (m_ui->checkAssociateTorrents->isChecked()) {
        Preferences::setTorrentFileAssoc();
        m_ui->checkAssociateTorrents->setChecked(Preferences::isTorrentFileAssocSet());
        m_ui->checkAssociateTorrents->setEnabled(!m_ui->checkAssociateTorrents->isChecked());
    }
    if (m_ui->checkAssociateMagnetLinks->isChecked()) {
        Preferences::setMagnetLinkAssoc();
        m_ui->checkAssociateMagnetLinks->setChecked(Preferences::isMagnetLinkAssocSet());
        m_ui->checkAssociateMagnetLinks->setEnabled(!m_ui->checkAssociateMagnetLinks->isChecked());
    }
#endif
    Application * const app = static_cast<Application*>(QCoreApplication::instance());
    app->setFileLoggerPath(m_ui->textFileLogPath->selectedPath());
    app->setFileLoggerBackup(m_ui->checkFileLogBackup->isChecked());
    app->setFileLoggerMaxSize(m_ui->spinFileLogSize->value());
    app->setFileLoggerAge(m_ui->spinFileLogAge->value());
    app->setFileLoggerAgeType(m_ui->comboFileLogAgeType->currentIndex());
    app->setFileLoggerDeleteOld(m_ui->checkFileLogDelete->isChecked());
    app->setFileLoggerEnabled(m_ui->checkFileLog->isChecked());
    // End General preferences

    RSS::Session::instance()->setRefreshInterval(m_ui->spinRSSRefreshInterval->value());
    RSS::Session::instance()->setMaxArticlesPerFeed(m_ui->spinRSSMaxArticlesPerFeed->value());
    RSS::Session::instance()->setProcessingEnabled(m_ui->checkRSSEnable->isChecked());
    RSS::AutoDownloader::instance()->setProcessingEnabled(m_ui->checkRSSAutoDownloaderEnable->isChecked());

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
    AddNewTorrentDialog::setEnabled(useAdditionDialog());
    AddNewTorrentDialog::setTopLevel(m_ui->checkAdditionDialogFront->isChecked());
    session->setAddTorrentPaused(addTorrentsInPause());
    session->setCreateTorrentSubfolder(m_ui->checkCreateSubfolder->isChecked());
    ScanFoldersModel::instance()->removeFromFSWatcher(removedScanDirs);
    ScanFoldersModel::instance()->addToFSWatcher(addedScanDirs);
    ScanFoldersModel::instance()->makePersistent();
    removedScanDirs.clear();
    addedScanDirs.clear();
    session->setTorrentExportDirectory(getTorrentExportDir());
    session->setFinishedTorrentExportDirectory(getFinishedTorrentExportDir());
    pref->setMailNotificationEnabled(m_ui->groupMailNotification->isChecked());
    pref->setMailNotificationEmail(m_ui->dest_email_txt->text());
    pref->setMailNotificationSMTP(m_ui->smtp_server_txt->text());
    pref->setMailNotificationSMTPSSL(m_ui->checkSmtpSSL->isChecked());
    pref->setMailNotificationSMTPAuth(m_ui->groupMailNotifAuth->isChecked());
    pref->setMailNotificationSMTPUsername(m_ui->mailNotifUsername->text());
    pref->setMailNotificationSMTPPassword(m_ui->mailNotifPassword->text());
    pref->setAutoRunEnabled(m_ui->autoRunBox->isChecked());
    pref->setAutoRunProgram(m_ui->autoRun_txt->selectedPath().trimmed());
    pref->setActionOnDblClOnTorrentDl(getActionOnDblClOnTorrentDl());
    pref->setActionOnDblClOnTorrentFn(getActionOnDblClOnTorrentFn());
    TorrentFileGuard::setAutoDeleteMode(!m_ui->deleteTorrentBox->isChecked() ? TorrentFileGuard::Never
                             : !m_ui->deleteCancelledTorrentBox->isChecked() ? TorrentFileGuard::IfAdded
                             : TorrentFileGuard::Always);
    // End Downloads preferences

    // Connection preferences
    session->setBTProtocol(static_cast<BitTorrent::BTProtocol>(m_ui->comboProtocol->currentIndex()));
    session->setPort(getPort());
    session->setUseRandomPort(m_ui->checkRandomPort->isChecked());
    Net::PortForwarder::instance()->setEnabled(isUPnPEnabled());
    const QPair<int, int> down_up_limit = getGlobalBandwidthLimits();
    session->setGlobalDownloadSpeedLimit(down_up_limit.first);
    session->setGlobalUploadSpeedLimit(down_up_limit.second);
    session->setUTPRateLimited(m_ui->checkLimituTPConnections->isChecked());
    session->setIncludeOverheadInLimits(m_ui->checkLimitTransportOverhead->isChecked());
    session->setIgnoreLimitsOnLAN(!m_ui->checkLimitLocalPeerRate->isChecked());
    const QPair<int, int> alt_down_up_limit = getAltGlobalBandwidthLimits();
    session->setAltGlobalDownloadSpeedLimit(alt_down_up_limit.first);
    session->setAltGlobalUploadSpeedLimit(alt_down_up_limit.second);
    pref->setSchedulerStartTime(m_ui->schedule_from->time());
    pref->setSchedulerEndTime(m_ui->schedule_to->time());
    pref->setSchedulerDays(static_cast<scheduler_days>(m_ui->schedule_days->currentIndex()));
    session->setBandwidthSchedulerEnabled(m_ui->check_schedule->isChecked());

    auto proxyConfigManager  = Net::ProxyConfigurationManager::instance();
    Net::ProxyConfiguration proxyConf;
    proxyConf.type = getProxyType();
    proxyConf.ip = getProxyIp();
    proxyConf.port = getProxyPort();
    proxyConf.username = getProxyUsername();
    proxyConf.password = getProxyPassword();
    proxyConfigManager->setProxyOnlyForTorrents(m_ui->isProxyOnlyForTorrents->isChecked());
    proxyConfigManager->setProxyConfiguration(proxyConf);

    session->setProxyPeerConnectionsEnabled(m_ui->checkProxyPeerConnecs->isChecked());
    session->setForceProxyEnabled(m_ui->checkForceProxy->isChecked());
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
    session->setMaxRatioAction(static_cast<MaxRatioAction>(m_ui->comboRatioLimitAct->currentIndex()));
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
    // End Queueing system preferences
    // Web UI
    pref->setWebUiEnabled(isWebUiEnabled());
    if (isWebUiEnabled()) {
        pref->setServerDomains(m_ui->textServerDomains->text());
        pref->setWebUiPort(webUiPort());
        pref->setUPnPForWebUIPort(m_ui->checkWebUIUPnP->isChecked());
        pref->setWebUiHttpsEnabled(m_ui->checkWebUiHttps->isChecked());
        if (m_ui->checkWebUiHttps->isChecked()) {
            pref->setWebUiHttpsCertificate(m_sslCert);
            pref->setWebUiHttpsKey(m_sslKey);
        }
        pref->setWebUiUsername(webUiUsername());
        pref->setWebUiPassword(webUiPassword());
        pref->setWebUiLocalAuthEnabled(!m_ui->checkBypassLocalAuth->isChecked());
        // DynDNS
        pref->setDynDNSEnabled(m_ui->checkDynDNS->isChecked());
        pref->setDynDNSService(m_ui->comboDNSService->currentIndex());
        pref->setDynDomainName(m_ui->domainNameTxt->text());
        pref->setDynDNSUsername(m_ui->DNSUsernameTxt->text());
        pref->setDynDNSPassword(m_ui->DNSPasswordTxt->text());
    }
    // End Web UI
    // End preferences
    // Save advanced settings
    advancedSettings->saveAdvancedSettings();
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
    switch (m_ui->comboProxyType->currentIndex()) {
    case 1:
        return Net::ProxyType::SOCKS4;
        break;
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
    const Preferences* const pref = Preferences::instance();

    // General preferences
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

#ifndef Q_OS_MAC
    m_ui->checkShowSystray->setChecked(pref->systrayIntegration());
    if (m_ui->checkShowSystray->isChecked()) {
        m_ui->checkMinimizeToSysTray->setChecked(pref->minimizeToTray());
        m_ui->checkCloseToSystray->setChecked(pref->closeToTray());
        m_ui->comboTrayIcon->setCurrentIndex(pref->trayIconStyle());
    }
#endif

    m_ui->checkPreventFromSuspend->setChecked(pref->preventFromSuspend());

#ifdef Q_OS_WIN
    m_ui->checkStartup->setChecked(pref->WinStartup());
    m_ui->checkAssociateTorrents->setChecked(Preferences::isTorrentFileAssocSet());
    m_ui->checkAssociateMagnetLinks->setChecked(Preferences::isMagnetLinkAssocSet());
#endif
#ifdef Q_OS_MAC
    m_ui->checkAssociateTorrents->setChecked(Preferences::isTorrentFileAssocSet());
    m_ui->checkAssociateTorrents->setEnabled(!m_ui->checkAssociateTorrents->isChecked());
    m_ui->checkAssociateMagnetLinks->setChecked(Preferences::isMagnetLinkAssocSet());
    m_ui->checkAssociateMagnetLinks->setEnabled(!m_ui->checkAssociateMagnetLinks->isChecked());
#endif

    const Application * const app = static_cast<Application*>(QCoreApplication::instance());
    m_ui->checkFileLog->setChecked(app->isFileLoggerEnabled());
    m_ui->textFileLogPath->setSelectedPath(app->fileLoggerPath());
    fileLogBackup = app->isFileLoggerBackup();
    m_ui->checkFileLogBackup->setChecked(fileLogBackup);
    m_ui->spinFileLogSize->setEnabled(fileLogBackup);
    fileLogDelete = app->isFileLoggerDeleteOld();
    m_ui->checkFileLogDelete->setChecked(fileLogDelete);
    m_ui->spinFileLogAge->setEnabled(fileLogDelete);
    m_ui->comboFileLogAgeType->setEnabled(fileLogDelete);
    m_ui->spinFileLogSize->setValue(app->fileLoggerMaxSize());
    m_ui->spinFileLogAge->setValue(app->fileLoggerAge());
    m_ui->comboFileLogAgeType->setCurrentIndex(app->fileLoggerAgeType());
    // End General preferences

    m_ui->checkRSSEnable->setChecked(RSS::Session::instance()->isProcessingEnabled());
    m_ui->checkRSSAutoDownloaderEnable->setChecked(RSS::AutoDownloader::instance()->isProcessingEnabled());
    m_ui->spinRSSRefreshInterval->setValue(RSS::Session::instance()->refreshInterval());
    m_ui->spinRSSMaxArticlesPerFeed->setValue(RSS::Session::instance()->maxArticlesPerFeed());

    auto session = BitTorrent::Session::instance();

    // Downloads preferences
    m_ui->checkAdditionDialog->setChecked(AddNewTorrentDialog::isEnabled());
    m_ui->checkAdditionDialogFront->setChecked(AddNewTorrentDialog::isTopLevel());
    m_ui->checkStartPaused->setChecked(session->isAddTorrentPaused());
    m_ui->checkCreateSubfolder->setChecked(session->isCreateTorrentSubfolder());
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

    strValue = session->torrentExportDirectory();
    if (strValue.isEmpty()) {
        // Disable
        m_ui->checkExportDir->setChecked(false);
        m_ui->textExportDir->setEnabled(false);
    }
    else {
        // Enable
        m_ui->checkExportDir->setChecked(true);
        m_ui->textExportDir->setEnabled(true);
        m_ui->textExportDir->setSelectedPath(strValue);
    }

    strValue = session->finishedTorrentExportDirectory();
    if (strValue.isEmpty()) {
        // Disable
        m_ui->checkExportDirFin->setChecked(false);
        m_ui->textExportDirFin->setEnabled(false);
    }
    else {
        // Enable
        m_ui->checkExportDirFin->setChecked(true);
        m_ui->textExportDirFin->setEnabled(true);
        m_ui->textExportDirFin->setSelectedPath(strValue);
    }

    m_ui->groupMailNotification->setChecked(pref->isMailNotificationEnabled());
    m_ui->dest_email_txt->setText(pref->getMailNotificationEmail());
    m_ui->smtp_server_txt->setText(pref->getMailNotificationSMTP());
    m_ui->checkSmtpSSL->setChecked(pref->getMailNotificationSMTPSSL());
    m_ui->groupMailNotifAuth->setChecked(pref->getMailNotificationSMTPAuth());
    m_ui->mailNotifUsername->setText(pref->getMailNotificationSMTPUsername());
    m_ui->mailNotifPassword->setText(pref->getMailNotificationSMTPPassword());

    m_ui->autoRunBox->setChecked(pref->isAutoRunEnabled());
    m_ui->autoRun_txt->setSelectedPath(pref->getAutoRunProgram());
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
    m_ui->checkUPnP->setChecked(Net::PortForwarder::instance()->isEnabled());
    m_ui->checkRandomPort->setChecked(session->useRandomPort());
    m_ui->spinPort->setValue(session->port());
    m_ui->spinPort->setDisabled(m_ui->checkRandomPort->isChecked());

    intValue = session->maxConnections();
    if (intValue > 0) {
        // enable
        m_ui->checkMaxConnecs->setChecked(true);
        m_ui->spinMaxConnec->setEnabled(true);
        m_ui->spinMaxConnec->setValue(intValue);
    }
    else {
        // disable
        m_ui->checkMaxConnecs->setChecked(false);
        m_ui->spinMaxConnec->setEnabled(false);
    }
    intValue = session->maxConnectionsPerTorrent();
    if (intValue > 0) {
        // enable
        m_ui->checkMaxConnecsPerTorrent->setChecked(true);
        m_ui->spinMaxConnecPerTorrent->setEnabled(true);
        m_ui->spinMaxConnecPerTorrent->setValue(intValue);
    }
    else {
        // disable
        m_ui->checkMaxConnecsPerTorrent->setChecked(false);
        m_ui->spinMaxConnecPerTorrent->setEnabled(false);
    }
    intValue = session->maxUploads();
    if (intValue > 0) {
        // enable
        m_ui->checkMaxUploads->setChecked(true);
        m_ui->spinMaxUploads->setEnabled(true);
        m_ui->spinMaxUploads->setValue(intValue);
    }
    else {
        // disable
        m_ui->checkMaxUploads->setChecked(false);
        m_ui->spinMaxUploads->setEnabled(false);
    }
    intValue = session->maxUploadsPerTorrent();
    if (intValue > 0) {
        // enable
        m_ui->checkMaxUploadsPerTorrent->setChecked(true);
        m_ui->spinMaxUploadsPerTorrent->setEnabled(true);
        m_ui->spinMaxUploadsPerTorrent->setValue(intValue);
    }
    else {
        // disable
        m_ui->checkMaxUploadsPerTorrent->setChecked(false);
        m_ui->spinMaxUploadsPerTorrent->setEnabled(false);
    }

    auto proxyConfigManager = Net::ProxyConfigurationManager::instance();
    Net::ProxyConfiguration proxyConf = proxyConfigManager->proxyConfiguration();
    using Net::ProxyType;
    bool useProxyAuth = false;
    switch (proxyConf.type) {
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
    m_ui->checkForceProxy->setChecked(session->isForceProxyEnabled());
    m_ui->isProxyOnlyForTorrents->setChecked(proxyConfigManager->isProxyOnlyForTorrents());
    enableProxy(m_ui->comboProxyType->currentIndex());

    m_ui->checkIPFilter->setChecked(session->isIPFilteringEnabled());
    m_ui->textFilterPath->setEnabled(m_ui->checkIPFilter->isChecked());
    m_ui->textFilterPath->setSelectedPath(session->IPFilterFile());
    m_ui->IpFilterRefreshBtn->setEnabled(m_ui->checkIPFilter->isChecked());
    m_ui->checkIpFilterTrackers->setChecked(session->isTrackerFilteringEnabled());
    // End Connection preferences

    // Speed preferences
    intValue = session->globalDownloadSpeedLimit() / 1024;
    if (intValue > 0) {
        // Enabled
        m_ui->checkDownloadLimit->setChecked(true);
        m_ui->spinDownloadLimit->setEnabled(true);
        m_ui->spinDownloadLimit->setValue(intValue);
    }
    else {
        // Disabled
        m_ui->checkDownloadLimit->setChecked(false);
        m_ui->spinDownloadLimit->setEnabled(false);
    }
    intValue = session->globalUploadSpeedLimit() / 1024;
    if (intValue > 0) {
        // Enabled
        m_ui->checkUploadLimit->setChecked(true);
        m_ui->spinUploadLimit->setEnabled(true);
        m_ui->spinUploadLimit->setValue(intValue);
    }
    else {
        // Disabled
        m_ui->checkUploadLimit->setChecked(false);
        m_ui->spinUploadLimit->setEnabled(false);
    }

    intValue = session->altGlobalDownloadSpeedLimit() / 1024;
    if (intValue > 0) {
        // Enabled
        m_ui->checkDownloadLimitAlt->setChecked(true);
        m_ui->spinDownloadLimitAlt->setEnabled(true);
        m_ui->spinDownloadLimitAlt->setValue(intValue);
    }
    else {
        // Disabled
        m_ui->checkDownloadLimitAlt->setChecked(false);
        m_ui->spinDownloadLimitAlt->setEnabled(false);
    }
    intValue = session->altGlobalUploadSpeedLimit() / 1024;
    if (intValue > 0) {
        // Enabled
        m_ui->checkUploadLimitAlt->setChecked(true);
        m_ui->spinUploadLimitAlt->setEnabled(true);
        m_ui->spinUploadLimitAlt->setValue(intValue);
    }
    else {
        // Disabled
        m_ui->checkUploadLimitAlt->setChecked(false);
        m_ui->spinUploadLimitAlt->setEnabled(false);
    }

    m_ui->checkLimituTPConnections->setChecked(session->isUTPRateLimited());
    m_ui->checkLimitTransportOverhead->setChecked(session->includeOverheadInLimits());
    m_ui->checkLimitLocalPeerRate->setChecked(!session->ignoreLimitsOnLAN());

    m_ui->check_schedule->setChecked(session->isBandwidthSchedulerEnabled());
    m_ui->schedule_from->setTime(pref->getSchedulerStartTime());
    m_ui->schedule_to->setTime(pref->getSchedulerEndTime());
    m_ui->schedule_days->setCurrentIndex(static_cast<int>(pref->getSchedulerDays()));
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

    if (session->globalMaxRatio() >= 0.) {
        // Enable
        m_ui->checkMaxRatio->setChecked(true);
        m_ui->spinMaxRatio->setEnabled(true);
        m_ui->comboRatioLimitAct->setEnabled(true);
        m_ui->spinMaxRatio->setValue(session->globalMaxRatio());
    }
    else {
        // Disable
        m_ui->checkMaxRatio->setChecked(false);
        m_ui->spinMaxRatio->setEnabled(false);
    }
    if (session->globalMaxSeedingMinutes() >= 0) {
        // Enable
        m_ui->checkMaxSeedingMinutes->setChecked(true);
        m_ui->spinMaxSeedingMinutes->setEnabled(true);
        m_ui->spinMaxSeedingMinutes->setValue(session->globalMaxSeedingMinutes());
    }
    else {
        // Disable
        m_ui->checkMaxSeedingMinutes->setChecked(false);
        m_ui->spinMaxSeedingMinutes->setEnabled(false);
    }
    m_ui->comboRatioLimitAct->setEnabled((session->globalMaxSeedingMinutes() >= 0) || (session->globalMaxRatio() >= 0.));
    m_ui->comboRatioLimitAct->setCurrentIndex(session->maxRatioAction());
    // End Bittorrent preferences

    // Web UI preferences
    m_ui->textServerDomains->setText(pref->getServerDomains());
    m_ui->checkWebUi->setChecked(pref->isWebUiEnabled());
    m_ui->spinWebUiPort->setValue(pref->getWebUiPort());
    m_ui->checkWebUIUPnP->setChecked(pref->useUPnPForWebUIPort());
    m_ui->checkWebUiHttps->setChecked(pref->isWebUiHttpsEnabled());
    setSslCertificate(pref->getWebUiHttpsCertificate());
    setSslKey(pref->getWebUiHttpsKey());
    m_ui->textWebUiUsername->setText(pref->getWebUiUsername());
    m_ui->textWebUiPassword->setText(pref->getWebUiPassword());
    m_ui->checkBypassLocalAuth->setChecked(!pref->isWebUiLocalAuthEnabled());

    m_ui->checkDynDNS->setChecked(pref->isDynDNSEnabled());
    m_ui->comboDNSService->setCurrentIndex(static_cast<int>(pref->getDynDNSService()));
    m_ui->domainNameTxt->setText(pref->getDynDomainName());
    m_ui->DNSUsernameTxt->setText(pref->getDynDNSUsername());
    m_ui->DNSPasswordTxt->setText(pref->getDynDNSPassword());
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

// Return Download & Upload limits in kbps
// [download,upload]
QPair<int, int> OptionsDialog::getGlobalBandwidthLimits() const
{
    int DL = 0, UP = 0;
    if (m_ui->checkDownloadLimit->isChecked())
        DL = m_ui->spinDownloadLimit->value() * 1024;
    if (m_ui->checkUploadLimit->isChecked())
        UP = m_ui->spinUploadLimit->value() * 1024;
    return qMakePair(DL, UP);
}

// Return alternate Download & Upload limits in kbps
// [download,upload]
QPair<int, int> OptionsDialog::getAltGlobalBandwidthLimits() const
{
    int DL = 0, UP = 0;
    if (m_ui->checkDownloadLimitAlt->isChecked())
        DL = m_ui->spinDownloadLimitAlt->value() * 1024;
    if (m_ui->checkUploadLimitAlt->isChecked())
        UP = m_ui->spinUploadLimitAlt->value() * 1024;
    return qMakePair(DL, UP);
}

bool OptionsDialog::startMinimized() const
{
    return m_ui->checkStartMinimized->isChecked();
}

#ifndef Q_OS_MAC
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
#endif

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
    else
        return m_ui->spinMaxConnec->value();
}

int OptionsDialog::getMaxConnecsPerTorrent() const
{
    if (!m_ui->checkMaxConnecsPerTorrent->isChecked())
        return -1;
    else
        return m_ui->spinMaxConnecPerTorrent->value();
}

int OptionsDialog::getMaxUploads() const
{
    if (!m_ui->checkMaxUploads->isChecked())
        return -1;
    else
        return m_ui->spinMaxUploads->value();
}

int OptionsDialog::getMaxUploadsPerTorrent() const
{
    if (!m_ui->checkMaxUploadsPerTorrent->isChecked())
        return -1;
    else
        return m_ui->spinMaxUploadsPerTorrent->value();
}

void OptionsDialog::on_buttonBox_accepted()
{
    if (applyButton->isEnabled()) {
        if (!schedTimesOk()) {
            m_ui->tabSelection->setCurrentRow(TAB_SPEED);
            return;
        }
        if (!webUIAuthenticationOk()) {
            m_ui->tabSelection->setCurrentRow(TAB_WEBUI);
            return;
        }
        applyButton->setEnabled(false);
        this->hide();
        saveOptions();
    }
    saveWindowState();
    accept();
}

void OptionsDialog::applySettings(QAbstractButton* button)
{
    if (button == applyButton) {
        if (!schedTimesOk()) {
            m_ui->tabSelection->setCurrentRow(TAB_SPEED);
            return;
        }
        if (!webUIAuthenticationOk()) {
            m_ui->tabSelection->setCurrentRow(TAB_WEBUI);
            return;
        }
        saveOptions();
    }
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
    applyButton->setEnabled(true);
}

void OptionsDialog::toggleComboRatioLimitAct()
{
    // Verify if the share action button must be enabled
    m_ui->comboRatioLimitAct->setEnabled(m_ui->checkMaxRatio->isChecked() || m_ui->checkMaxSeedingMinutes->isChecked());
}

void OptionsDialog::enableProxy(int index)
{
    if (index) {
        //enable
        m_ui->lblProxyIP->setEnabled(true);
        m_ui->textProxyIP->setEnabled(true);
        m_ui->lblProxyPort->setEnabled(true);
        m_ui->spinProxyPort->setEnabled(true);
        m_ui->checkProxyPeerConnecs->setEnabled(true);
        m_ui->checkForceProxy->setEnabled(true);
        if (index > 1) {
            m_ui->checkProxyAuth->setEnabled(true);
            m_ui->isProxyOnlyForTorrents->setEnabled(true);
        }
        else {
            m_ui->checkProxyAuth->setEnabled(false);
            m_ui->checkProxyAuth->setChecked(false);
            m_ui->isProxyOnlyForTorrents->setEnabled(false);
            m_ui->isProxyOnlyForTorrents->setChecked(true);
        }
    }
    else {
        //disable
        m_ui->lblProxyIP->setEnabled(false);
        m_ui->textProxyIP->setEnabled(false);
        m_ui->lblProxyPort->setEnabled(false);
        m_ui->spinProxyPort->setEnabled(false);
        m_ui->checkProxyPeerConnecs->setEnabled(false);
        m_ui->checkForceProxy->setEnabled(false);
        m_ui->isProxyOnlyForTorrents->setEnabled(false);
        m_ui->checkProxyAuth->setEnabled(false);
        m_ui->checkProxyAuth->setChecked(false);
    }
}

bool OptionsDialog::isSlashScreenDisabled() const
{
    return !m_ui->checkShowSplash->isChecked();
}

#ifdef Q_OS_WIN
bool OptionsDialog::WinStartup() const
{
    return m_ui->checkStartup->isChecked();
}
#endif

bool OptionsDialog::preventFromSuspend() const
{
    return m_ui->checkPreventFromSuspend->isChecked();
}

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
    if (localeStr.startsWith("eo", Qt::CaseInsensitive)) {
        name = "eo";
    }
    else {
        QLocale locale(localeStr);
        if (locale.language() == QLocale::Uzbek)
            name = "uz@Latn";
        else
            name = locale.name();
    }
    // Attempt to find exact match
    int index = m_ui->comboI18n->findData(name, Qt::UserRole);
    if (index < 0) {
        //Attempt to find a language match without a country
        int pos = name.indexOf('_');
        if (pos > -1) {
            QString lang = name.left(pos);
            index = m_ui->comboI18n->findData(lang, Qt::UserRole);
        }
    }
    if (index < 0) {
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
    return QString();
}

QString OptionsDialog::getFinishedTorrentExportDir() const
{
    if (m_ui->checkExportDirFin->isChecked())
        return Utils::Fs::expandPathAbs(m_ui->textExportDirFin->selectedPath());
    return QString();
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

void OptionsDialog::on_addScanFolderButton_clicked()
{
    Preferences* const pref = Preferences::instance();
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select folder to monitor"),
                                                          Utils::Fs::toNativePath(Utils::Fs::folderName(pref->getScanDirsLastPath())));
    if (!dir.isEmpty()) {
        const ScanFoldersModel::PathStatus status = ScanFoldersModel::instance()->addPath(dir, ScanFoldersModel::DEFAULT_LOCATION, QString(), false);
        QString error;
        switch (status) {
        case ScanFoldersModel::AlreadyInList:
            error = tr("Folder is already being monitored:");
            break;
        case ScanFoldersModel::DoesNotExist:
            error = tr("Folder does not exist:");
            break;
        case ScanFoldersModel::CannotRead:
            error = tr("Folder is not readable:");
            break;
        default:
            pref->setScanDirsLastPath(dir);
            addedScanDirs << dir;
            for (int i = 0; i < ScanFoldersModel::instance()->columnCount(); ++i)
                m_ui->scanFoldersView->resizeColumnToContents(i);
            enableApplyButton();
        }

        if (!error.isEmpty())
            QMessageBox::critical(this, tr("Adding entry failed"), QString("%1\n%2").arg(error).arg(dir));
    }
}

void OptionsDialog::on_removeScanFolderButton_clicked()
{
    const QModelIndexList selected
        = m_ui->scanFoldersView->selectionModel()->selectedIndexes();
    if (selected.isEmpty())
        return;
    Q_ASSERT(selected.count() == ScanFoldersModel::instance()->columnCount());
    foreach (const QModelIndex &index, selected) {
        if (index.column() == ScanFoldersModel::WATCH)
            removedScanDirs << index.data().toString();
    }
    ScanFoldersModel::instance()->removePath(selected.first().row(), false);
}

void OptionsDialog::handleScanFolderViewSelectionChanged()
{
    m_ui->removeScanFolderButton->setEnabled(!m_ui->scanFoldersView->selectionModel()->selectedIndexes().isEmpty());
}

QString OptionsDialog::askForExportDir(const QString& currentExportPath)
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

quint16 OptionsDialog::webUiPort() const
{
    return m_ui->spinWebUiPort->value();
}

QString OptionsDialog::webUiUsername() const
{
    return m_ui->textWebUiUsername->text();
}

QString OptionsDialog::webUiPassword() const
{
    return m_ui->textWebUiPassword->text();
}

void OptionsDialog::showConnectionTab()
{
    m_ui->tabSelection->setCurrentRow(TAB_CONNECTION);
}

void OptionsDialog::on_btnWebUiCrt_clicked()
{
    const QString filename = QFileDialog::getOpenFileName(this, tr("Import SSL certificate"), QString(), tr("SSL Certificate") + QLatin1String(" (*.crt *.pem)"));
    if (filename.isEmpty())
        return;

    QFile cert(filename);
    if (!cert.open(QIODevice::ReadOnly))
        return;

    bool success = setSslCertificate(cert.read(1024 * 1024));
    if (!success)
        QMessageBox::warning(this, tr("Invalid certificate"), tr("This is not a valid SSL certificate."));
}

void OptionsDialog::on_btnWebUiKey_clicked()
{
    const QString filename = QFileDialog::getOpenFileName(this, tr("Import SSL key"), QString(), tr("SSL key") + QLatin1String(" (*.key *.pem)"));
    if (filename.isEmpty())
        return;

    QFile key(filename);
    if (!key.open(QIODevice::ReadOnly))
        return;

    bool success = setSslKey(key.read(1024 * 1024));
    if (!success)
        QMessageBox::warning(this, tr("Invalid key"), tr("This is not a valid SSL key."));
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
    connect(session, SIGNAL(IPFilterParsed(bool, int)), SLOT(handleIPFilterParsed(bool, int)));
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
    disconnect(BitTorrent::Session::instance(), SIGNAL(IPFilterParsed(bool, int)), this, SLOT(handleIPFilterParsed(bool, int)));
}

QString OptionsDialog::languageToLocalizedString(const QLocale &locale)
{
    switch (locale.language()) {
    case QLocale::English: {
        if (locale.country() == QLocale::Australia)
            return QString::fromUtf8(C_LOCALE_ENGLISH_AUSTRALIA);
        else if (locale.country() == QLocale::UnitedKingdom)
            return QString::fromUtf8(C_LOCALE_ENGLISH_UNITEDKINGDOM);
        return QString::fromUtf8(C_LOCALE_ENGLISH);
    }
    case QLocale::French: return QString::fromUtf8(C_LOCALE_FRENCH);
    case QLocale::German: return QString::fromUtf8(C_LOCALE_GERMAN);
    case QLocale::Hungarian: return QString::fromUtf8(C_LOCALE_HUNGARIAN);
    case QLocale::Icelandic: return QString::fromUtf8(C_LOCALE_ICELANDIC);
    case QLocale::Indonesian: return QString::fromUtf8(C_LOCALE_INDONESIAN);
    case QLocale::Italian: return QString::fromUtf8(C_LOCALE_ITALIAN);
    case QLocale::Dutch: return QString::fromUtf8(C_LOCALE_DUTCH);
    case QLocale::Spanish: return QString::fromUtf8(C_LOCALE_SPANISH);
    case QLocale::Catalan: return QString::fromUtf8(C_LOCALE_CATALAN);
    case QLocale::Galician: return QString::fromUtf8(C_LOCALE_GALICIAN);
    case QLocale::Occitan: return QString::fromUtf8(C_LOCALE_OCCITAN);
    case QLocale::Portuguese: {
        if (locale.country() == QLocale::Brazil)
            return QString::fromUtf8(C_LOCALE_PORTUGUESE_BRAZIL);
        return QString::fromUtf8(C_LOCALE_PORTUGUESE);
    }
    case QLocale::Polish: return QString::fromUtf8(C_LOCALE_POLISH);
    case QLocale::Latvian: return QString::fromUtf8(C_LOCALE_LATVIAN);
    case QLocale::Lithuanian: return QString::fromUtf8(C_LOCALE_LITHUANIAN);
    case QLocale::Malay: return QString::fromUtf8(C_LOCALE_MALAY);
    case QLocale::Czech: return QString::fromUtf8(C_LOCALE_CZECH);
    case QLocale::Slovak: return QString::fromUtf8(C_LOCALE_SLOVAK);
    case QLocale::Slovenian: return QString::fromUtf8(C_LOCALE_SLOVENIAN);
    case QLocale::Serbian: return QString::fromUtf8(C_LOCALE_SERBIAN);
    case QLocale::Croatian: return QString::fromUtf8(C_LOCALE_CROATIAN);
    case QLocale::Armenian: return QString::fromUtf8(C_LOCALE_ARMENIAN);
    case QLocale::Romanian: return QString::fromUtf8(C_LOCALE_ROMANIAN);
    case QLocale::Turkish: return QString::fromUtf8(C_LOCALE_TURKISH);
    case QLocale::Greek: return QString::fromUtf8(C_LOCALE_GREEK);
    case QLocale::Swedish: return QString::fromUtf8(C_LOCALE_SWEDISH);
    case QLocale::Finnish: return QString::fromUtf8(C_LOCALE_FINNISH);
    case QLocale::Norwegian: return QString::fromUtf8(C_LOCALE_NORWEGIAN);
    case QLocale::Danish: return QString::fromUtf8(C_LOCALE_DANISH);
    case QLocale::Bulgarian: return QString::fromUtf8(C_LOCALE_BULGARIAN);
    case QLocale::Ukrainian: return QString::fromUtf8(C_LOCALE_UKRAINIAN);
    case QLocale::Uzbek: return QString::fromUtf8(C_LOCALE_UZBEK);
    case QLocale::Russian: return QString::fromUtf8(C_LOCALE_RUSSIAN);
    case QLocale::Japanese: return QString::fromUtf8(C_LOCALE_JAPANESE);
    case QLocale::Hebrew: return QString::fromUtf8(C_LOCALE_HEBREW);
    case QLocale::Hindi: return QString::fromUtf8(C_LOCALE_HINDI);
    case QLocale::Arabic: return QString::fromUtf8(C_LOCALE_ARABIC);
    case QLocale::Georgian: return QString::fromUtf8(C_LOCALE_GEORGIAN);
    case QLocale::Byelorussian: return QString::fromUtf8(C_LOCALE_BYELORUSSIAN);
    case QLocale::Basque: return QString::fromUtf8(C_LOCALE_BASQUE);
    case QLocale::Vietnamese: return QString::fromUtf8(C_LOCALE_VIETNAMESE);
    case QLocale::Chinese: {
        switch (locale.country()) {
        case QLocale::China:
            return QString::fromUtf8(C_LOCALE_CHINESE_SIMPLIFIED);
        case QLocale::HongKong:
            return QString::fromUtf8(C_LOCALE_CHINESE_TRADITIONAL_HK);
        default:
            return QString::fromUtf8(C_LOCALE_CHINESE_TRADITIONAL_TW);

        }
    }
    case QLocale::Korean: return QString::fromUtf8(C_LOCALE_KOREAN);
    default: {
        // Fallback to English
        const QString eng_lang = QLocale::languageToString(locale.language());
        qWarning() << "Unrecognized language name: " << eng_lang;
        return eng_lang;
    }
    }
}

bool OptionsDialog::setSslKey(const QByteArray &key)
{
#ifndef QT_NO_OPENSSL
    // try different formats
    const bool isKeyValid = (!QSslKey(key, QSsl::Rsa).isNull() || !QSslKey(key, QSsl::Ec).isNull());
    if (isKeyValid) {
        m_ui->lblSslKeyStatus->setPixmap(QPixmap(":/icons/qbt-theme/security-high.png").scaledToHeight(20, Qt::SmoothTransformation));
        m_sslKey = key;
    }
    else {
        m_ui->lblSslKeyStatus->setPixmap(QPixmap(":/icons/qbt-theme/security-low.png").scaledToHeight(20, Qt::SmoothTransformation));
        m_sslKey.clear();
    }
    return isKeyValid;
#else
    Q_UNUSED(key);
    return false;
#endif
}

bool OptionsDialog::setSslCertificate(const QByteArray &cert)
{
#ifndef QT_NO_OPENSSL
    const bool isCertValid = !QSslCertificate(cert).isNull();
    if (isCertValid) {
        m_ui->lblSslCertStatus->setPixmap(QPixmap(":/icons/qbt-theme/security-high.png").scaledToHeight(20, Qt::SmoothTransformation));
        m_sslCert = cert;
    }
    else {
        m_ui->lblSslCertStatus->setPixmap(QPixmap(":/icons/qbt-theme/security-low.png").scaledToHeight(20, Qt::SmoothTransformation));
        m_sslCert.clear();
    }
    return isCertValid;
#else
    Q_UNUSED(cert);
    return false;
#endif
}

bool OptionsDialog::schedTimesOk()
{
    if (m_ui->schedule_from->time() == m_ui->schedule_to->time()) {
        QMessageBox::warning(this, tr("Time Error"), tr("The start time and the end time can't be the same."));
        return false;
    }
    return true;
}

bool OptionsDialog::webUIAuthenticationOk()
{
    if (webUiUsername().length() < 3) {
        QMessageBox::warning(this, tr("Length Error"), tr("The Web UI username must be at least 3 characters long."));
        return false;
    }
    if (webUiPassword().length() < 6) {
        QMessageBox::warning(this, tr("Length Error"), tr("The Web UI password must be at least 6 characters long."));
        return false;
    }
    return true;
}

void OptionsDialog::on_banListButton_clicked()
{
    //have to call dialog window
    BanListOptions bl(this);
    bl.exec();
}
