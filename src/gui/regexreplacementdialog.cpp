/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021  Welp Wazowski
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

#include "regexreplacementdialog.h"

RegexReplacementDialog::RegexReplacementDialog(QWidget *parent, const QString &title)
    : QDialog(parent)
    , m_ui(new Ui::RegexReplacementDialog)
{
    m_ui->setupUi(this);
    setWindowTitle(title);
    m_ui->labelInvalidRegex->setVisible(false);
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &RegexReplacementDialog::okClicked);
}

bool RegexReplacementDialog::prompt()
{
    bool ok = exec();
    if (!ok)
    {
        return false;
    }

    return true;
}

void RegexReplacementDialog::okClicked()
{
    m_isRegex = m_ui->checkRegularExpression->isChecked();
    m_isCaseInsensitive = m_ui->checkIgnoreCase->isChecked();

    m_replace = m_ui->textReplacement->text();
    if (m_isRegex)
    {
        m_regex = QRegularExpression(m_ui->textRegex->text()
                                     , m_isCaseInsensitive ? QRegularExpression::PatternOption::CaseInsensitiveOption
                                                           : QRegularExpression::PatternOption::NoPatternOption);
        if (!m_regex.isValid())
        {
            m_ui->labelInvalidRegex->setVisible(true);
            return;
        }
    }
    else
    {
        m_find = m_ui->textRegex->text();
    }

    accept();
}

QString RegexReplacementDialog::replace(QString arg) const
{
    if (m_isRegex)
    {
        return arg.replace(m_regex, m_replace);
    }
    else
    {
        return arg.replace(m_find, m_replace
                           , m_isCaseInsensitive ? Qt::CaseInsensitive : Qt::CaseSensitive);
    }
}
