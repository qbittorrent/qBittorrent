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

#include "torrentcategorydialog.h"

#include <QMessageBox>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrentcategory.h"
#include "ui_torrentcategorydialog.h"

TorrentCategoryDialog::TorrentCategoryDialog(QWidget *parent)
    : QDialog {parent}
    , m_ui {new Ui::TorrentCategoryDialog}
{
    m_ui->setupUi(this);
    m_ui->comboSavePath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->comboSavePath->setDialogCaption(tr("Choose save path"));
}

TorrentCategoryDialog::~TorrentCategoryDialog()
{
    delete m_ui;
}

QString TorrentCategoryDialog::createCategory(
        QWidget *parent, const QString &parentCategoryName)
{
    QString newCategoryName {parentCategoryName.trimmed()};
    if (!newCategoryName.isEmpty())
        newCategoryName += QLatin1Char('/');
    newCategoryName += tr("New Category");

    TorrentCategoryDialog dialog(parent);
    dialog.setCategoryName(newCategoryName);
    while (dialog.exec() == TorrentCategoryDialog::Accepted) {
        BitTorrent::TorrentCategory *newCategory = BitTorrent::Session::instance()->createCategory(
                    dialog.categoryName());
        if (newCategory) {
            newCategory->setSavePath(dialog.savePath());
            return newCategory->name();
        }

        QMessageBox::warning(parent, tr("Couldn't create category")
                             , tr("Category with given name already exists."));
    }

    return {};
}

void TorrentCategoryDialog::editCategory(QWidget *parent, const QString &categoryName)
{
    BitTorrent::TorrentCategory *const category {BitTorrent::Session::instance()->findCategory(categoryName)};
    Q_ASSERT(category);

    TorrentCategoryDialog dialog(parent);
    dialog.setCategoryNameEditable(false);
    dialog.setCategoryName(category->name());
    dialog.setSavePath(category->savePath());
    if (dialog.exec() == TorrentCategoryDialog::Accepted) {
        category->setSavePath(dialog.savePath());
    }
}

QString TorrentCategoryDialog::categoryName() const
{
    return m_ui->textCategoryName->text();
}

void TorrentCategoryDialog::setCategoryName(const QString &categoryName)
{
    m_ui->textCategoryName->setText(categoryName);
}

QString TorrentCategoryDialog::savePath() const
{
    return m_ui->comboSavePath->selectedPath();
}

void TorrentCategoryDialog::setSavePath(const QString &savePath)
{
    m_ui->comboSavePath->setSelectedPath(savePath);
}

void TorrentCategoryDialog::setCategoryNameEditable(bool editable)
{
    m_ui->textCategoryName->setEnabled(editable);
}
