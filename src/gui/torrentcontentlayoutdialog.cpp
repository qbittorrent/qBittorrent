/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include "torrentcontentlayoutdialog.h"

#include <QPushButton>
#include <QStyledItemDelegate>

#include "base/bittorrent/torrentcontenthandler.h"
#include "torrentcontentlayoutmodel.h"
#include "uithememanager.h"
#include "utils.h"
#include "ui_torrentcontentlayoutdialog.h"

#define SETTINGS_KEY(name) u"GUI/TorrentContentLayoutDialog/" name

using namespace Qt::Literals::StringLiterals;

class TorrentContentLayoutItemDelegate final : public QStyledItemDelegate
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TorrentContentLayoutItemDelegate)

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);

        if (index.column() == TorrentContentLayoutModel::COL_PATH)
        {
            option->font.setItalic(index.data(TorrentContentLayoutModel::PATH_CHANGED_ROLE).toBool());
        }
    }
};

TorrentContentLayoutDialog::TorrentContentLayoutDialog(BitTorrent::TorrentContentHandler *contentHandler, QWidget *parent)
    : QDialog(parent)
    , m_ui {new Ui::TorrentContentLayoutDialog}
    , m_storeDialogSize {SETTINGS_KEY(u"Size"_s)}
    , m_storeViewState {SETTINGS_KEY(u"ViewState"_s)}
{
    m_ui->setupUi(this);

    m_applyButton = new QPushButton(tr("Apply"), this);
    m_ui->buttonBox->addButton(m_applyButton, QDialogButtonBox::ApplyRole);
    m_applyButton->setEnabled(false);

    auto *closeButton = new QPushButton(tr("Close"), this);
    m_ui->buttonBox->addButton(closeButton, QDialogButtonBox::RejectRole);

    connect(m_ui->buttonBox, &QDialogButtonBox::clicked, this, [this, closeButton](QAbstractButton *button)
    {
        (button == closeButton) ? reject() : apply();
    });

    setWindowIcon(UIThemeManager::instance()->getIcon(u"edit-rename"_s));

    m_model = new TorrentContentLayoutModel(contentHandler, this);
    m_ui->treeView->setModel(m_model);
    if (m_model->rowCount() > 0)
    {
        m_ui->treeView->selectionModel()->setCurrentIndex(m_model->index(0, 0)
                , (QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows));
    }

    connect(m_model, &QAbstractItemModel::dataChanged, this, [this]
    {
        m_applyButton->setEnabled(m_model->haveChangedFilePaths());
    });

    m_ui->treeView->setItemDelegate(new TorrentContentLayoutItemDelegate(this));

    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);

    m_ui->treeView->header()->restoreState(m_storeViewState);
}

TorrentContentLayoutDialog::~TorrentContentLayoutDialog()
{
    m_storeDialogSize = size();
    m_storeViewState = m_ui->treeView->header()->saveState();
    delete m_ui;
}

void TorrentContentLayoutDialog::apply()
{
    m_model->applyChangedFilePaths();
}

#include "torrentcontentlayoutdialog.moc"
