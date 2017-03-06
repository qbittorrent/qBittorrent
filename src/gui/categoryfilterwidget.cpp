/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
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

#include "categoryfilterwidget.h"

#include <QAction>
#include <QHeaderView>
#include <QLayout>
#include <QMenu>
#include <QMessageBox>

#include "base/bittorrent/session.h"
#include "base/utils/misc.h"
#include "autoexpandabledialog.h"
#include "categoryfiltermodel.h"
#include "guiiconprovider.h"

namespace
{
    QString getCategoryFilter(const CategoryFilterModel *const model, const QModelIndex &index)
    {
        QString categoryFilter; // Defaults to All
        if (index.isValid()) {
            if (!index.parent().isValid() && (index.row() == 1))
                categoryFilter = ""; // Uncategorized
            else if (index.parent().isValid() || (index.row() > 1))
                categoryFilter = model->categoryName(index);
        }

        return categoryFilter;
    }

    bool isSpecialItem(const QModelIndex &index)
    {
        // the first two items at first level are special items:
        // 'All' and 'Uncategorized'
        return (!index.parent().isValid() && (index.row() <= 1));
    }
}

CategoryFilterWidget::CategoryFilterWidget(QWidget *parent)
    : QTreeView(parent)
{
    setModel(new CategoryFilterModel(this));
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setUniformRowHeights(true);
    setHeaderHidden(true);
    setIconSize(Utils::Misc::smallIconSize());
#if defined(Q_OS_MAC)
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
    setContextMenuPolicy(Qt::CustomContextMenu);
    setCurrentIndex(model()->index(0, 0));

    connect(this, SIGNAL(collapsed(QModelIndex)), SLOT(callUpdateGeometry()));
    connect(this, SIGNAL(expanded(QModelIndex)), SLOT(callUpdateGeometry()));
    connect(this, SIGNAL(customContextMenuRequested(QPoint)), SLOT(showMenu(QPoint)));
    connect(selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex))
            , SLOT(onCurrentRowChanged(QModelIndex,QModelIndex)));
    connect(model(), SIGNAL(modelReset()), SLOT(callUpdateGeometry()));
}

QString CategoryFilterWidget::currentCategory() const
{
    QModelIndex current;
    auto selectedRows = selectionModel()->selectedRows();
    if (!selectedRows.isEmpty())
        current = selectedRows.first();

    return getCategoryFilter(static_cast<CategoryFilterModel *>(model()), current);
}

void CategoryFilterWidget::onCurrentRowChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);

    emit categoryChanged(getCategoryFilter(static_cast<CategoryFilterModel *>(model()), current));
}

void CategoryFilterWidget::showMenu(QPoint)
{
    QMenu menu(this);

    QAction *addAct = menu.addAction(
                          GuiIconProvider::instance()->getIcon("list-add")
                          , tr("Add category..."));
    connect(addAct, SIGNAL(triggered()), SLOT(addCategory()));

    auto selectedRows = selectionModel()->selectedRows();
    if (!selectedRows.empty() && !isSpecialItem(selectedRows.first())) {
        if (BitTorrent::Session::instance()->isSubcategoriesEnabled()) {
            QAction *addSubAct = menu.addAction(
                        GuiIconProvider::instance()->getIcon("list-add")
                        , tr("Add subcategory..."));
            connect(addSubAct, SIGNAL(triggered()), SLOT(addSubcategory()));
        }

        QAction *removeAct = menu.addAction(
                        GuiIconProvider::instance()->getIcon("list-remove")
                        , tr("Remove category"));
        connect(removeAct, SIGNAL(triggered()), SLOT(removeCategory()));
    }

    QAction *removeUnusedAct = menu.addAction(
                                   GuiIconProvider::instance()->getIcon("list-remove")
                                   , tr("Remove unused categories"));
    connect(removeUnusedAct, SIGNAL(triggered()), SLOT(removeUnusedCategories()));

    menu.addSeparator();

    QAction *startAct = menu.addAction(
                            GuiIconProvider::instance()->getIcon("media-playback-start")
                            , tr("Resume torrents"));
    connect(startAct, SIGNAL(triggered()), SIGNAL(actionResumeTorrentsTriggered()));

    QAction *pauseAct = menu.addAction(
                            GuiIconProvider::instance()->getIcon("media-playback-pause")
                            , tr("Pause torrents"));
    connect(pauseAct, SIGNAL(triggered()), SIGNAL(actionPauseTorrentsTriggered()));

    QAction *deleteTorrentsAct = menu.addAction(
                                     GuiIconProvider::instance()->getIcon("edit-delete")
                                     , tr("Delete torrents"));
    connect(deleteTorrentsAct, SIGNAL(triggered()), SIGNAL(actionDeleteTorrentsTriggered()));

    menu.exec(QCursor::pos());
}

void CategoryFilterWidget::callUpdateGeometry()
{
    updateGeometry();
}

QSize CategoryFilterWidget::sizeHint() const
{
    return viewportSizeHint();
}

QSize CategoryFilterWidget::minimumSizeHint() const
{
    QSize size = sizeHint();
    size.setWidth(6);
    return size;
}

void CategoryFilterWidget::rowsInserted(const QModelIndex &parent, int start, int end)
{
    QTreeView::rowsInserted(parent, start, end);

    // Expand all parents if the parent(s) of the node are not expanded.
    QModelIndex p = parent;
    while (p.isValid()) {
        if (!isExpanded(p))
            expand(p);
        p = model()->parent(p);
    }

    updateGeometry();
}

QString CategoryFilterWidget::askCategoryName()
{
    bool ok;
    QString category = "";
    bool invalid;
    do {
        invalid = false;
        category = AutoExpandableDialog::getText(
                    this, tr("New Category"), tr("Category:"), QLineEdit::Normal, category, &ok);
        if (ok && !category.isEmpty()) {
            if (!BitTorrent::Session::isValidCategoryName(category)) {
                QMessageBox::warning(
                            this, tr("Invalid category name")
                            , tr("Category name must not contain '\\'.\n"
                                 "Category name must not start/end with '/'.\n"
                                 "Category name must not contain '//' sequence."));
                invalid = true;
            }
        }
    } while (invalid);

    return ok ? category : QString();
}

void CategoryFilterWidget::addCategory()
{
    const QString category = askCategoryName();
    if (category.isEmpty()) return;

    if (BitTorrent::Session::instance()->categories().contains(category))
        QMessageBox::warning(this, tr("Category exists"), tr("Category name already exists."));
    else
        BitTorrent::Session::instance()->addCategory(category);
}

void CategoryFilterWidget::addSubcategory()
{
    const QString subcat = askCategoryName();
    if (subcat.isEmpty()) return;

    const QString category = QString(QStringLiteral("%1/%2")).arg(currentCategory()).arg(subcat);

    if (BitTorrent::Session::instance()->categories().contains(category))
        QMessageBox::warning(this, tr("Category exists")
                             , tr("Subcategory name already exists in selected category."));
    else
        BitTorrent::Session::instance()->addCategory(category);
}

void CategoryFilterWidget::removeCategory()
{
    auto selectedRows = selectionModel()->selectedRows();
    if (!selectedRows.empty() && !isSpecialItem(selectedRows.first())) {
        BitTorrent::Session::instance()->removeCategory(
                    static_cast<CategoryFilterModel *>(model())->categoryName(selectedRows.first()));
        updateGeometry();
    }
}

void CategoryFilterWidget::removeUnusedCategories()
{
    auto session = BitTorrent::Session::instance();
    foreach (const QString &category, session->categories())
        if (model()->data(static_cast<CategoryFilterModel *>(model())->index(category), Qt::UserRole) == 0)
            session->removeCategory(category);
    updateGeometry();
}
