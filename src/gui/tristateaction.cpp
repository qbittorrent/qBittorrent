/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Mike Tzou (Chocobo1)
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

#include "tristateaction.h"

#include <QString>
#include <QWidget>

#include "tristatewidget.h"

TriStateAction::TriStateAction(const QString &text, QWidget *parent)
    : QWidgetAction {parent}
    , m_triStateWidget {new TriStateWidget {text, parent}}
{
    setCheckable(true);

    // required for QAction::setChecked(bool) to work
    connect(this, &QAction::toggled, this, [this](const bool checked)
    {
        m_triStateWidget->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    });

    connect(m_triStateWidget, &TriStateWidget::triggered, this, &QAction::toggled);
    setDefaultWidget(m_triStateWidget);
}

void TriStateAction::setCheckState(const Qt::CheckState checkState)
{
    QWidgetAction::setChecked((checkState == Qt::Checked) ? true : false);
    m_triStateWidget->setCheckState(checkState);
}

void TriStateAction::setCloseOnInteraction(const bool enabled)
{
    m_triStateWidget->setCloseOnInteraction(enabled);
}
