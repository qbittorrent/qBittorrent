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
#include "raisedmessagebox.h"
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
    : TorrentContentLayoutDialog(contentHandler, {}, parent)
{
}

TorrentContentLayoutDialog::TorrentContentLayoutDialog(BitTorrent::TorrentContentHandler *contentHandler, const QSet<int> &selectedIndexes, QWidget *parent)
    : QDialog(parent)
    , m_ui {new Ui::TorrentContentLayoutDialog}
    , m_storeDialogSize {SETTINGS_KEY(u"Size"_s)}
    , m_storeViewState {SETTINGS_KEY(u"ViewState"_s)}
{
    m_ui->setupUi(this);

    m_ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

    connect(m_ui->buttonBox, &QDialogButtonBox::clicked, this, [this](const QAbstractButton *button)
    {
        (button == m_ui->buttonBox->button(QDialogButtonBox::Apply))
                ? apply() : reject();
    });

    setWindowIcon(UIThemeManager::instance()->getIcon(u"edit-rename"_s));

    m_model = new TorrentContentLayoutModel(contentHandler, this);
    m_ui->pathListView->setModel(m_model);
    if (m_model->rowCount() > 0)
    {
        if (!selectedIndexes.isEmpty())
        {
            for (const int fileIndex : selectedIndexes)
            {
                const QModelIndex itemIndex = m_model->index(fileIndex, 0);
                m_ui->pathListView->selectionModel()->select(itemIndex
                        , (QItemSelectionModel::Select | QItemSelectionModel::Rows));
            }
        }
        else
        {
            m_ui->pathListView->selectionModel()->setCurrentIndex(m_model->index(0, 0)
                    , (QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows));
        }
    }

    populateCommonPath();

    connect(m_model, &QAbstractItemModel::dataChanged, this, [this]
    {
        m_ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(m_model->haveChangedFilePaths());
    });

    connect(m_model, &TorrentContentLayoutModel::renameFailed, this
            , [this](const QString &firstErrorMessage, const qsizetype errorsCount)
    {
        QString errorMessage = firstErrorMessage;
        if (errorsCount > 1)
        {
            errorMessage.append(u'\n' + tr("There are %n more renaming error(s)."
                    , "There are 5 more renaming errors.", (errorsCount - 1)));
        }

        RaisedMessageBox::warning(this, tr("Rename error"), errorMessage, QMessageBox::Ok);
    });

    m_ui->pathListView->setItemDelegate(new TorrentContentLayoutItemDelegate(this));

    connect(m_ui->pathListView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TorrentContentLayoutDialog::populateCommonPath);
    connect(m_ui->commonPathEdit, &QLineEdit::textEdited, this, &TorrentContentLayoutDialog::onCommonPathEdited);

    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);

    m_ui->pathListView->header()->restoreState(m_storeViewState);
}

TorrentContentLayoutDialog::~TorrentContentLayoutDialog()
{
    m_storeDialogSize = size();
    m_storeViewState = m_ui->pathListView->header()->saveState();
    delete m_ui;
}

PathList TorrentContentLayoutDialog::selectedPaths() const
{
    const QModelIndexList selectedRows = m_ui->pathListView->selectionModel()->selectedRows(TorrentContentLayoutModel::COL_PATH);
    if (selectedRows.isEmpty())
        return {};

    PathList selectedPaths;
    selectedPaths.reserve(selectedRows.size());
    for (const QModelIndex &rowIndex : selectedRows)
        selectedPaths.append(Path(rowIndex.data().toString()));

    return selectedPaths;
}

void TorrentContentLayoutDialog::populateCommonPath()
{
    if (const PathList paths = selectedPaths(); !paths.isEmpty())
    {
        m_commonPath = Path::commonPath(paths);
        m_ui->commonPathEdit->setText(m_commonPath.toString());
        m_ui->commonPathLabel->setEnabled(true);
        m_ui->commonPathEdit->setEnabled(true);
    }
    else
    {
        m_commonPath = {};
        m_ui->commonPathEdit->clear();
        m_ui->commonPathLabel->setEnabled(false);
        m_ui->commonPathEdit->setEnabled(false);
    }
}

void TorrentContentLayoutDialog::onCommonPathEdited(const QString &pathStr)
{
    const Path oldCommonPath = m_commonPath;
    m_commonPath = Path(pathStr);

    const QModelIndexList selectedRows = m_ui->pathListView->selectionModel()->selectedRows(TorrentContentLayoutModel::COL_PATH);
    if (selectedRows.isEmpty())
        return;

    for (const QModelIndex &rowIndex : selectedRows)
    {
        const Path oldPath {rowIndex.data().toString()};
        const Path newPath = m_commonPath / oldCommonPath.relativePathOf(oldPath);
        m_model->setData(rowIndex, newPath.toString());
    }
}

void TorrentContentLayoutDialog::apply()
{
    m_model->applyChangedFilePaths();
}

#include "torrentcontentlayoutdialog.moc"
