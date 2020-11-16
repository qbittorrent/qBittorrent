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

QString TorrentCategoryDialog::createCategory(QWidget *parent, const QString &parentCategoryName)
{
    using BitTorrent::Session;

    QString newCategoryName {parentCategoryName};
    if (!newCategoryName.isEmpty())
        newCategoryName += QLatin1Char('/');
    newCategoryName += tr("New Category");

    TorrentCategoryDialog dialog(parent);
    dialog.setCategoryName(newCategoryName);
    while (dialog.exec() == TorrentCategoryDialog::Accepted)
    {
        newCategoryName = dialog.categoryName();

        if (!BitTorrent::Session::isValidCategoryName(newCategoryName))
        {
            QMessageBox::critical(
                        parent, tr("Invalid category name")
                        , tr("Category name cannot contain '\\'.\n"
                             "Category name cannot start/end with '/'.\n"
                             "Category name cannot contain '//' sequence."));
        }
        else if (BitTorrent::Session::instance()->categories().contains(newCategoryName))
        {
            QMessageBox::critical(
                        parent, tr("Category creation error")
                        , tr("Category with the given name already exists.\n"
                             "Please choose a different name and try again."));
        }
        else
        {
            Session::instance()->addCategory(newCategoryName, dialog.savePath());
            return newCategoryName;
        }
    }

    return {};
}

void TorrentCategoryDialog::editCategory(QWidget *parent, const QString &categoryName)
{
    using BitTorrent::Session;

    Q_ASSERT(Session::instance()->categories().contains(categoryName));

    auto dialog = new TorrentCategoryDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setCategoryNameEditable(false);
    dialog->setCategoryName(categoryName);
    dialog->setSavePath(Session::instance()->categories()[categoryName]);
    connect(dialog, &TorrentCategoryDialog::accepted, parent, [dialog, categoryName]()
    {
        Session::instance()->editCategory(categoryName, dialog->savePath());
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
}

QString TorrentCategoryDialog::savePath() const
{
    return m_ui->comboSavePath->selectedPath();
}

void TorrentCategoryDialog::setSavePath(const QString &savePath)
{
    m_ui->comboSavePath->setSelectedPath(savePath);
}
