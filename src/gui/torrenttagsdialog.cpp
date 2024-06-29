/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include "torrenttagsdialog.h"

#include <QCheckBox>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "autoexpandabledialog.h"
#include "flowlayout.h"
#include "utils.h"

#include "ui_torrenttagsdialog.h"

#define SETTINGS_KEY(name) u"GUI/TorrentTagsDialog/" name

TorrentTagsDialog::TorrentTagsDialog(const TagSet &initialTags, QWidget *parent)
    : QDialog(parent)
    , m_ui {new Ui::TorrentTagsDialog}
    , m_storeDialogSize {SETTINGS_KEY(u"Size"_s)}
{
    m_ui->setupUi(this);

    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *tagsLayout = new FlowLayout(m_ui->scrollArea->widget());
    for (const Tag &tag : asConst(initialTags.united(BitTorrent::Session::instance()->tags())))
    {
        auto *tagWidget = new QCheckBox(Utils::Gui::tagToWidgetText(tag));
        if (initialTags.contains(tag))
            tagWidget->setChecked(true);
        tagsLayout->addWidget(tagWidget);
    }

    auto *addTagButton = new QPushButton(u"+"_s);
    connect(addTagButton, &QPushButton::clicked, this, &TorrentTagsDialog::addNewTag);
    tagsLayout->addWidget(addTagButton);

    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);
}

TorrentTagsDialog::~TorrentTagsDialog()
{
    m_storeDialogSize = size();
    delete m_ui;
}

TagSet TorrentTagsDialog::tags() const
{
    TagSet tags;
    auto *layout = m_ui->scrollArea->widget()->layout();
    for (int i = 0; i < (layout->count() - 1); ++i)
    {
        const auto *tagWidget = static_cast<QCheckBox *>(layout->itemAt(i)->widget());
        if (tagWidget->isChecked())
            tags.insert(Utils::Gui::widgetTextToTag(tagWidget->text()));
    }

    return tags;
}

void TorrentTagsDialog::addNewTag()
{
    bool done = false;
    Tag tag;
    while (!done)
    {
        bool ok = false;
        tag = Tag(AutoExpandableDialog::getText(this, tr("Add tag")
                , tr("Tag:"), QLineEdit::Normal, tag.toString(), &ok));
        if (!ok || tag.isEmpty())
            break;

        if (!tag.isValid())
        {
            QMessageBox::warning(this, tr("Invalid tag name"), tr("Tag name '%1' is invalid.").arg(tag.toString()));
        }
        else if (BitTorrent::Session::instance()->tags().contains(tag))
        {
            QMessageBox::warning(this, tr("Tag exists"), tr("Tag name already exists."));
        }
        else
        {
            auto *layout = m_ui->scrollArea->widget()->layout();
            auto *btn = layout->takeAt(layout->count() - 1);
            auto *tagWidget = new QCheckBox(Utils::Gui::tagToWidgetText(tag));
            tagWidget->setChecked(true);
            layout->addWidget(tagWidget);
            layout->addItem(btn);

            done = true;
        }
    }
}
