/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021  Vladimir Golovnev <glassez@yandex.ru>
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

#include "indexeroptionsdialog.h"

#include <QPushButton>
#include <QUrl>

#include "base/global.h"
#include "base/settingsstorage.h"
#include "gui/utils.h"
#include "ui_indexeroptionsdialog.h"

#define SETTINGS_KEY(name) u"IndexerOptionsDialog/" name

IndexerOptionsDialog::IndexerOptionsDialog(QWidget *parent)
    : QDialog {parent}
    , m_ui {new Ui::IndexerOptionsDialog}
    , m_storeDialogSize {SETTINGS_KEY(u"DialogSize"_qs)}
{
    m_ui->setupUi(this);
    loadState();

    connect(m_ui->lineName, &QLineEdit::textChanged, this, &IndexerOptionsDialog::validate);
    connect(m_ui->lineURL, &QLineEdit::textChanged, this, &IndexerOptionsDialog::validate);
    connect(m_ui->lineAPIKey, &QLineEdit::textChanged, this, &IndexerOptionsDialog::validate);

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
}

IndexerOptionsDialog::~IndexerOptionsDialog()
{
    saveState();
    delete m_ui;
}

QString IndexerOptionsDialog::indexerName() const
{
    return m_ui->lineName->text();
}

void IndexerOptionsDialog::setIndexerName(const QString &name)
{
    m_ui->lineName->setText(name);
}

IndexerOptions IndexerOptionsDialog::indexerOptions() const
{
    IndexerOptions indexerOptions;
    indexerOptions.url = m_ui->lineURL->text();
    indexerOptions.apiKey = m_ui->lineAPIKey->text();

    return indexerOptions;
}

void IndexerOptionsDialog::setIndexerOptions(const IndexerOptions &indexerOptions)
{
    m_ui->lineURL->setText(indexerOptions.url);
    m_ui->lineAPIKey->setText(indexerOptions.apiKey);
}

void IndexerOptionsDialog::loadState()
{
    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);
}

void IndexerOptionsDialog::saveState()
{
    m_storeDialogSize = size();
}

void IndexerOptionsDialog::validate()
{
    const QUrl testUrl {m_ui->lineURL->text()};
    const bool hasAcceptableURL = testUrl.isValid() && !testUrl.isRelative()
            && !testUrl.isLocalFile() && !testUrl.host().isEmpty();

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
                !m_ui->lineName->text().isEmpty()
                && !m_ui->lineAPIKey->text().isEmpty()
                && hasAcceptableURL);
}
