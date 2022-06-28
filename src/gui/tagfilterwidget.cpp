/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Tony Gregerson <tony.gregerson@gmail.com>
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

#include "tagfilterwidget.h"

#include <QMenu>
#include <QMessageBox>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "autoexpandabledialog.h"
#include "tagfiltermodel.h"
#include "tagfilterproxymodel.h"
#include "uithememanager.h"
#include "utils.h"

namespace
{
    QString getTagFilter(const TagFilterProxyModel *const model, const QModelIndex &index)
    {
        QString tagFilter; // Defaults to All
        if (index.isValid())
        {
            if (index.row() == 1)
                tagFilter = u""_qs;  // Untagged
            else if (index.row() > 1)
                tagFilter = model->tag(index);
        }
        return tagFilter;
    }
}

TagFilterWidget::TagFilterWidget(QWidget *parent)
    : QTreeView(parent)
{
    auto *proxyModel = new TagFilterProxyModel(this);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setSourceModel(new TagFilterModel(this));
    setModel(proxyModel);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setUniformRowHeights(true);
    setHeaderHidden(true);
    setIconSize(Utils::Gui::smallIconSize());
#if defined(Q_OS_MACOS)
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
    setIndentation(0);
    setContextMenuPolicy(Qt::CustomContextMenu);
    sortByColumn(0, Qt::AscendingOrder);
    setCurrentIndex(model()->index(0, 0));

    connect(this, &TagFilterWidget::collapsed, this, &TagFilterWidget::callUpdateGeometry);
    connect(this, &TagFilterWidget::expanded, this, &TagFilterWidget::callUpdateGeometry);
    connect(this, &TagFilterWidget::customContextMenuRequested, this, &TagFilterWidget::showMenu);
    connect(selectionModel(), &QItemSelectionModel::currentRowChanged, this
            , &TagFilterWidget::onCurrentRowChanged);
    connect(model(), &QAbstractItemModel::modelReset, this, &TagFilterWidget::callUpdateGeometry);
}

QString TagFilterWidget::currentTag() const
{
    QModelIndex current;
    const auto selectedRows = selectionModel()->selectedRows();
    if (!selectedRows.isEmpty())
        current = selectedRows.first();

    return getTagFilter(static_cast<TagFilterProxyModel *>(model()), current);
}

void TagFilterWidget::onCurrentRowChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);

    emit tagChanged(getTagFilter(static_cast<TagFilterProxyModel *>(model()), current));
}

void TagFilterWidget::showMenu()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(UIThemeManager::instance()->getIcon(u"list-add"_qs), tr("Add tag...")
        , this, &TagFilterWidget::addTag);

    const auto selectedRows = selectionModel()->selectedRows();
    if (!selectedRows.empty() && !TagFilterModel::isSpecialItem(selectedRows.first()))
    {
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-clear"_qs), tr("Remove tag")
            , this, &TagFilterWidget::removeTag);
    }

    menu->addAction(UIThemeManager::instance()->getIcon(u"edit-clear"_qs), tr("Remove unused tags")
        , this, &TagFilterWidget::removeUnusedTags);
    menu->addSeparator();
    menu->addAction(UIThemeManager::instance()->getIcon(u"media-playback-start"_qs), tr("Resume torrents")
        , this, &TagFilterWidget::actionResumeTorrentsTriggered);
    menu->addAction(UIThemeManager::instance()->getIcon(u"media-playback-pause"_qs), tr("Pause torrents")
        , this, &TagFilterWidget::actionPauseTorrentsTriggered);
    menu->addAction(UIThemeManager::instance()->getIcon(u"list-remove"_qs), tr("Delete torrents")
        , this, &TagFilterWidget::actionDeleteTorrentsTriggered);

    menu->popup(QCursor::pos());
}

void TagFilterWidget::callUpdateGeometry()
{
    updateGeometry();
}

QSize TagFilterWidget::sizeHint() const
{
    return
    {
        // Width should be exactly the width of the content
        sizeHintForColumn(0),
        // Height should be exactly the height of the content
        static_cast<int>(sizeHintForRow(0) * (model()->rowCount() + 0.5)),
    };
}

QSize TagFilterWidget::minimumSizeHint() const
{
    QSize size = sizeHint();
    size.setWidth(6);
    return size;
}

void TagFilterWidget::rowsInserted(const QModelIndex &parent, int start, int end)
{
    QTreeView::rowsInserted(parent, start, end);
    updateGeometry();
}

QString TagFilterWidget::askTagName()
{
    bool ok = false;
    QString tag = u""_qs;
    bool invalid = true;
    while (invalid)
    {
        invalid = false;
        tag = AutoExpandableDialog::getText(
            this, tr("New Tag"), tr("Tag:"), QLineEdit::Normal, tag, &ok).trimmed();
        if (ok && !tag.isEmpty())
        {
            if (!BitTorrent::Session::isValidTag(tag))
            {
                QMessageBox::warning(
                    this, tr("Invalid tag name")
                    , tr("Tag name '%1' is invalid").arg(tag));
                invalid = true;
            }
        }
    }

    return ok ? tag : QString();
}

void TagFilterWidget::addTag()
{
    const QString tag = askTagName();
    if (tag.isEmpty()) return;

    if (BitTorrent::Session::instance()->tags().contains(tag))
        QMessageBox::warning(this, tr("Tag exists"), tr("Tag name already exists."));
    else
        BitTorrent::Session::instance()->addTag(tag);
}

void TagFilterWidget::removeTag()
{
    const auto selectedRows = selectionModel()->selectedRows();
    if (!selectedRows.empty() && !TagFilterModel::isSpecialItem(selectedRows.first()))
    {
        BitTorrent::Session::instance()->removeTag(
            static_cast<TagFilterProxyModel *>(model())->tag(selectedRows.first()));
        updateGeometry();
    }
}

void TagFilterWidget::removeUnusedTags()
{
    auto session = BitTorrent::Session::instance();
    for (const QString &tag : asConst(session->tags()))
        if (model()->data(static_cast<TagFilterProxyModel *>(model())->index(tag), Qt::UserRole) == 0)
            session->removeTag(tag);
    updateGeometry();
}
