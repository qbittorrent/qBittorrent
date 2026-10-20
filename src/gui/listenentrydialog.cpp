/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  WordlessEcho <wordless@echo.moe>
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

#include "listenentrydialog.h"

#include <QPushButton>

#include "base/global.h"
#include "base/utils/random.h"
#include "ui_listenentrydialog.h"

#define SETTINGS_KEY(name) u"ListenEntryDialog/" name

ListenEntryDialog::ListenEntryDialog(const BitTorrent::ListenEntry &entry, QWidget *parent)
    : QDialog(parent)
    , m_ui {new Ui::ListenEntryDialog}
    , m_storeDialogSize {SETTINGS_KEY(u"Size"_s)}
{
    m_ui->setupUi(this);

    m_ui->treeAddresses->setSelection(entry.interfaces);
    m_ui->lineEditPorts->setText(BitTorrent::listenPortsToString(entry.ports));

    connect(m_ui->treeAddresses, &AddressSelectionTree::selectionChanged, this, &ListenEntryDialog::updateOKButton);
    connect(m_ui->lineEditPorts, &QLineEdit::textChanged, this, &ListenEntryDialog::updateOKButton);
    connect(m_ui->buttonRandomPort, &QPushButton::clicked, this, &ListenEntryDialog::setRandomPort);
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateOKButton();

    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);
}

ListenEntryDialog::~ListenEntryDialog()
{
    m_storeDialogSize = size();
    delete m_ui;
}

BitTorrent::ListenEntry ListenEntryDialog::listenEntry() const
{
    return {
        .interfaces = m_ui->treeAddresses->selection(),
        .ports = BitTorrent::parseListenPorts(m_ui->lineEditPorts->text()).value_or(QList<BitTorrent::ListenPort>())
    };
}

void ListenEntryDialog::setRandomPort()
{
    QList<BitTorrent::ListenPort> ports = BitTorrent::parseListenPorts(m_ui->lineEditPorts->text())
        .value_or(QList<BitTorrent::ListenPort>());
    ports.removeIf([](const BitTorrent::ListenPort &port) { return !port.isSSL; });

    const auto randomPortNumber = [] { return static_cast<int>(Utils::Random::rand(1024, 65535)); };
    BitTorrent::ListenPort randomPort {.port = randomPortNumber()};
    while (ports.contains(BitTorrent::ListenPort {.port = randomPort.port, .isSSL = true}))
        randomPort.port = randomPortNumber();
    ports.prepend(randomPort);

    m_ui->lineEditPorts->setText(BitTorrent::listenPortsToString(ports));
}

void ListenEntryDialog::updateOKButton()
{
    const std::optional<QList<BitTorrent::ListenPort>> ports = BitTorrent::parseListenPorts(m_ui->lineEditPorts->text());
    const bool isPortsValid = ports && !ports->isEmpty();
    m_ui->lineEditPorts->setStyleSheet(isPortsValid ? QString() : u"QLineEdit { color: red; }"_s);

    const bool isValid = isPortsValid && !m_ui->treeAddresses->selection().isEmpty();
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(isValid);
}
