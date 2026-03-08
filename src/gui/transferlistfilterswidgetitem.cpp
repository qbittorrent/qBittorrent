/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Vladimir Golovnev <glassez@yandex.ru>
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

#include "transferlistfilterswidgetitem.h"

#include <QCheckBox>
#include <QFont>
#include <QPainter>
#include <QString>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>

namespace
{
    class ArrowCheckBox final : public QCheckBox
    {
    public:
        using QCheckBox::QCheckBox;

    private:
        void paintEvent(QPaintEvent *) override
        {
            QPainter painter {this};

            QStyleOptionViewItem indicatorOption;
            indicatorOption.initFrom(this);
            indicatorOption.rect = style()->subElementRect(QStyle::SE_CheckBoxIndicator, &indicatorOption, this);
            indicatorOption.state |= (QStyle::State_Children
                | (isChecked() ? QStyle::State_Open : QStyle::State_None));
            style()->drawPrimitive(QStyle::PE_IndicatorBranch, &indicatorOption, &painter, this);

            QStyleOptionButton labelOption;
            initStyleOption(&labelOption);
            labelOption.rect = style()->subElementRect(QStyle::SE_CheckBoxContents, &labelOption, this);
            style()->drawControl(QStyle::CE_CheckBoxLabel, &labelOption, &painter, this);
        }
    };
}

TransferListFiltersWidgetItem::TransferListFiltersWidgetItem(const QString &caption, QWidget *filterWidget, QWidget *parent)
    : QWidget(parent)
    , m_caption {new ArrowCheckBox(caption, this)}
    , m_filterWidget {filterWidget}
{
    QFont font;
    font.setBold(true);
    font.setCapitalization(QFont::AllUppercase);
    m_caption->setFont(font);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 2, 0, 0);
    layout->setSpacing(2);
    layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(m_caption);
    layout->addWidget(m_filterWidget);

    m_filterWidget->setVisible(m_caption->isChecked());

    connect(m_caption, &QCheckBox::toggled, m_filterWidget, &QWidget::setVisible);
    connect(m_caption, &QCheckBox::toggled, this, &TransferListFiltersWidgetItem::toggled);
}

bool TransferListFiltersWidgetItem::isChecked() const
{
    return m_caption->isChecked();
}

void TransferListFiltersWidgetItem::setChecked(const bool value)
{
    m_caption->setChecked(value);
}
