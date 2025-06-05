/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017, 2021  Vladimir Golovnev <glassez@yandex.ru>
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

#include "torrentcategorydialog.h"

#include <QMessageBox>
#include <QPushButton>

#include "base/bittorrent/session.h"
#include "base/utils/fs.h"
#include "ui_torrentcategorydialog.h"

TorrentCategoryDialog::TorrentCategoryDialog(QWidget *parent)
    : QDialog {parent}
    , m_ui {new Ui::TorrentCategoryDialog}
{
    m_ui->setupUi(this);

    m_ui->comboSavePath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->comboSavePath->setDialogCaption(tr("Choose save path"));

    m_ui->comboDownloadPath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->comboDownloadPath->setDialogCaption(tr("Choose download path"));
    m_ui->comboDownloadPath->setEnabled(false);
    m_ui->labelDownloadPath->setEnabled(false);

    // disable save button
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_ui->textCategoryName, &QLineEdit::textChanged, this, &TorrentCategoryDialog::categoryNameChanged);
    connect(m_ui->comboUseDownloadPath, &QComboBox::currentIndexChanged, this, &TorrentCategoryDialog::useDownloadPathChanged);
}

TorrentCategoryDialog::~TorrentCategoryDialog()
{
    delete m_ui;
}

QString TorrentCategoryDialog::createCategory(QWidget *parent, const QString &parentCategoryName)
{
    using BitTorrent::CategoryOptions;
    using BitTorrent::Session;

    QString newCategoryName = parentCategoryName;
    if (!newCategoryName.isEmpty())
        newCategoryName += u'/';
    newCategoryName += tr("New Category");

    TorrentCategoryDialog dialog {parent};
    dialog.setCategoryName(newCategoryName);
    while (dialog.exec() == TorrentCategoryDialog::Accepted)
    {
        newCategoryName = dialog.categoryName();

        if (!Session::isValidCategoryName(newCategoryName))
        {
            QMessageBox::critical(
                        parent, tr("Invalid category name")
                        , tr("Category name cannot contain '\\'.\n"
                             "Category name cannot start/end with '/'.\n"
                             "Category name cannot contain '//' sequence."));
        }
        else if (Session::instance()->categories().contains(newCategoryName))
        {
            QMessageBox::critical(
                        parent, tr("Category creation error")
                        , tr("Category with the given name already exists.\n"
                             "Please choose a different name and try again."));
        }
        else
        {
            Session::instance()->addCategory(newCategoryName, dialog.categoryOptions());
            return newCategoryName;
        }
    }

    return {};
}

void TorrentCategoryDialog::editCategory(QWidget *parent, const QString &categoryName)
{
    using BitTorrent::Session;

    Q_ASSERT(Session::instance()->categories().contains(categoryName));

    auto *dialog = new TorrentCategoryDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setCategoryNameEditable(false);
    dialog->setCategoryName(categoryName);
    dialog->setCategoryOptions(Session::instance()->categoryOptions(categoryName));
    connect(dialog, &TorrentCategoryDialog::accepted, parent, [dialog, categoryName]()
    {
        Session::instance()->editCategory(categoryName, dialog->categoryOptions());
    });
    dialog->open();
}

void TorrentCategoryDialog::setCategoryNameEditable(bool editable)
{
    m_ui->textCategoryName->setEnabled(editable);
}

QString TorrentCategoryDialog::categoryName() const
{
    return m_ui->textCategoryName->text();
}

void TorrentCategoryDialog::setCategoryName(const QString &categoryName)
{
    m_ui->textCategoryName->setText(categoryName);

    const int subcategoryNameStart = categoryName.lastIndexOf(u"/") + 1;
    m_ui->textCategoryName->setSelection(subcategoryNameStart, (categoryName.size() - subcategoryNameStart));
}

BitTorrent::CategoryOptions TorrentCategoryDialog::categoryOptions() const
{
    BitTorrent::CategoryOptions categoryOptions;
    categoryOptions.savePath = m_ui->comboSavePath->selectedPath();
    if (m_ui->comboUseDownloadPath->currentIndex() == 1)
        categoryOptions.downloadPath = {true, m_ui->comboDownloadPath->selectedPath()};
    else if (m_ui->comboUseDownloadPath->currentIndex() == 2)
        categoryOptions.downloadPath = {false, {}};

    return categoryOptions;
}

void TorrentCategoryDialog::setCategoryOptions(const BitTorrent::CategoryOptions &categoryOptions)
{
    m_ui->comboSavePath->setSelectedPath(categoryOptions.savePath);
    if (categoryOptions.downloadPath)
    {
        m_ui->comboUseDownloadPath->setCurrentIndex(categoryOptions.downloadPath->enabled ? 1 : 2);
        m_ui->comboDownloadPath->setSelectedPath(categoryOptions.downloadPath->enabled ? categoryOptions.downloadPath->path : Path());
    }
    else
    {
        m_ui->comboUseDownloadPath->setCurrentIndex(0);
        m_ui->comboDownloadPath->setSelectedPath({});
    }
}

void TorrentCategoryDialog::categoryNameChanged(const QString &categoryName)
{
    const auto *btSession = BitTorrent::Session::instance();
    m_ui->comboSavePath->setPlaceholder(btSession->categorySavePath(categoryName, categoryOptions()));

    const int index = m_ui->comboUseDownloadPath->currentIndex();
    const bool useDownloadPath = (index == 1) || ((index == 0) && btSession->isDownloadPathEnabled());
    if (useDownloadPath)
        m_ui->comboDownloadPath->setPlaceholder(btSession->categoryDownloadPath(categoryName, categoryOptions()));

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!categoryName.isEmpty());
}

void TorrentCategoryDialog::useDownloadPathChanged(const int index)
{
    if (const Path selectedPath = m_ui->comboDownloadPath->selectedPath(); !selectedPath.isEmpty())
        m_lastEnteredDownloadPath = selectedPath;

    m_ui->labelDownloadPath->setEnabled(index == 1);
    m_ui->comboDownloadPath->setEnabled(index == 1);
    m_ui->comboDownloadPath->setSelectedPath((index == 1) ? m_lastEnteredDownloadPath : Path());

    const auto *btSession = BitTorrent::Session::instance();
    const QString categoryName = m_ui->textCategoryName->text();
    const bool useDownloadPath = (index == 1) || ((index == 0) && btSession->isDownloadPathEnabled());
    const Path categoryPath = btSession->categoryDownloadPath(categoryName, categoryOptions());
    m_ui->comboDownloadPath->setPlaceholder(useDownloadPath ? categoryPath : Path());
}
