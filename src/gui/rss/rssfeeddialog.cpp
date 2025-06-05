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

#include "rssfeeddialog.h"

#include <QPushButton>

#include "ui_rssfeeddialog.h"

RSSFeedDialog::RSSFeedDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui {new Ui::RSSFeedDialog}
{
    m_ui->setupUi(this);

    m_ui->spinRefreshInterval->setMaximum(std::numeric_limits<int>::max());
    m_ui->spinRefreshInterval->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    m_ui->spinRefreshInterval->setSuffix(tr(" sec"));
    m_ui->spinRefreshInterval->setSpecialValueText(tr("Default"));

    // disable Ok button
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_ui->textFeedURL, &QLineEdit::textChanged, this, &RSSFeedDialog::feedURLChanged);
}

RSSFeedDialog::~RSSFeedDialog()
{
    delete m_ui;
}

QString RSSFeedDialog::feedURL() const
{
    return m_ui->textFeedURL->text();
}

void RSSFeedDialog::setFeedURL(const QString &feedURL)
{
    m_ui->textFeedURL->setText(feedURL);
}

std::chrono::seconds RSSFeedDialog::refreshInterval() const
{
    return std::chrono::seconds(m_ui->spinRefreshInterval->value());
}

void RSSFeedDialog::setRefreshInterval(const std::chrono::seconds refreshInterval)
{
    m_ui->spinRefreshInterval->setValue(refreshInterval.count());
}

void RSSFeedDialog::feedURLChanged(const QString &feedURL)
{
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!feedURL.isEmpty());
}
