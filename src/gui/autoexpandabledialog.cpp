/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2013  Nick Tiskov <daymansmail@gmail.com>
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

#include "autoexpandabledialog.h"

#include "ui_autoexpandabledialog.h"
#include "utils.h"

AutoExpandableDialog::AutoExpandableDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::AutoExpandableDialog)
{
    m_ui->setupUi(this);
}

AutoExpandableDialog::~AutoExpandableDialog()
{
    delete m_ui;
}

QString AutoExpandableDialog::getText(QWidget *parent, const QString &title, const QString &label,
                                      QLineEdit::EchoMode mode, const QString &text,
                                      bool *ok, const bool excludeExtension, Qt::InputMethodHints inputMethodHints)
{
    AutoExpandableDialog d(parent);
    d.setWindowTitle(title);
    d.m_ui->textLabel->setText(label);
    d.m_ui->textEdit->setText(text);
    d.m_ui->textEdit->setEchoMode(mode);
    d.m_ui->textEdit->setInputMethodHints(inputMethodHints);

    d.m_ui->textEdit->selectAll();
    if (excludeExtension)
    {
        int lastDotIndex = text.lastIndexOf('.');
        if ((lastDotIndex > 3) && (text.mid(lastDotIndex - 4, 4).toLower() == ".tar"))
            lastDotIndex -= 4;
        // Select file name without extension, except dot files like .gitignore
        if (lastDotIndex > 0)
            d.m_ui->textEdit->setSelection(0, lastDotIndex);
    }

    bool res = d.exec();
    if (ok)
        *ok = res;

    if (!res) return {};

    return d.m_ui->textEdit->text();
}

void AutoExpandableDialog::showEvent(QShowEvent *e)
{
    // Overriding showEvent is required for consistent UI with fixed size under custom DPI
    QDialog::showEvent(e);

    // Show dialog and resize textbox to fit the text
    // NOTE: For unknown reason QFontMetrics gets more accurate when called from showEvent.
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
    int wd = m_ui->textEdit->fontMetrics().horizontalAdvance(m_ui->textEdit->text()) + 4;
#else
    int wd = m_ui->textEdit->fontMetrics().width(m_ui->textEdit->text()) + 4;
#endif

    if (!windowTitle().isEmpty())
    {
        // not really the font metrics in window title, so we enlarge it a bit,
        // including the small icon and close button width
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
        int w = fontMetrics().horizontalAdvance(windowTitle()) * 1.8;
#else
        int w = fontMetrics().width(windowTitle()) * 1.8;
#endif
        wd = std::max(wd, w);
    }

    if (!m_ui->textLabel->text().isEmpty())
    {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
        int w = m_ui->textLabel->fontMetrics().horizontalAdvance(m_ui->textLabel->text());
#else
        int w = m_ui->textLabel->fontMetrics().width(m_ui->textLabel->text());
#endif
        wd = std::max(wd, w);
    }

    // Now resize the dialog to fit the contents
    // max width of text from either of: label, title, textedit
    // If the value is less than dialog default size, default size is used
    if (wd > width())
    {
        QSize size = {width() - m_ui->verticalLayout->sizeHint().width() + wd, height()};
        Utils::Gui::resize(this, size);
    }
}
