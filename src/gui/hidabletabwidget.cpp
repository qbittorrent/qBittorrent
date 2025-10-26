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

#include "hidabletabwidget.h"

#include <QTabBar>

#ifdef Q_OS_MACOS
#include <QPaintEvent>
#include <QStyle>
#endif

HidableTabWidget::HidableTabWidget(QWidget *parent)
    : QTabWidget(parent)
{
    // Skip single tab in keyboard navigation (no point navigating to it)
    tabBar()->setFocusPolicy(Qt::NoFocus);
}

void HidableTabWidget::tabInserted(const int index)
{
    QTabWidget::tabInserted(index);
    tabsCountChanged();
}

void HidableTabWidget::tabRemoved(const int index)
{
    QTabWidget::tabRemoved(index);
    tabsCountChanged();
}

void HidableTabWidget::tabsCountChanged()
{
    const qsizetype tabsCount = count();
    tabBar()->setVisible(tabsCount != 1);
    // Skip single tab in keyboard navigation (no point navigating to it)
    tabBar()->setFocusPolicy((tabsCount > 1) ? Qt::StrongFocus : Qt::NoFocus);
}

#ifdef Q_OS_MACOS
void HidableTabWidget::paintEvent(QPaintEvent *event)
{
    // Hide the pane for macintosh style
    if (!style()->inherits("QMacStyle"))
        QTabWidget::paintEvent(event);
}
#endif
