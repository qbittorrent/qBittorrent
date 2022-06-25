/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015 Anton Lashkov <lenton_91@mail.ru>
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

#include "speedwidget.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QVBoxLayout>

#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/preferences.h"
#include "propertieswidget.h"
#include "speedplotview.h"

ComboBoxMenuButton::ComboBoxMenuButton(QWidget *parent, QMenu *menu)
    : QComboBox(parent)
    , m_menu(menu)
{
}

void ComboBoxMenuButton::showPopup()
{
    const QPoint p = mapToGlobal(QPoint(0, height()));
    m_menu->popup(p);

    QComboBox::hidePopup();
}

SpeedWidget::SpeedWidget(PropertiesWidget *parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(3);

    m_hlayout = new QHBoxLayout();
    m_hlayout->setContentsMargins(0, 0, 0, 0);

    m_periodLabel = new QLabel(u"<b>" + tr("Period:") + u"</b>");

    m_periodCombobox = new QComboBox();
    m_periodCombobox->addItem(tr("1 Minute"));
    m_periodCombobox->addItem(tr("5 Minutes"));
    m_periodCombobox->addItem(tr("30 Minutes"));
    m_periodCombobox->addItem(tr("3 Hours"));
    m_periodCombobox->addItem(tr("6 Hours"));
    m_periodCombobox->addItem(tr("12 Hours"));
    m_periodCombobox->addItem(tr("24 Hours"));

    connect(m_periodCombobox, qOverload<int>(&QComboBox::currentIndexChanged)
        , this, &SpeedWidget::onPeriodChange);

    m_graphsMenu = new QMenu(this);
    m_graphsMenu->addAction(tr("Total Upload"));
    m_graphsMenu->addAction(tr("Total Download"));
    m_graphsMenu->addAction(tr("Payload Upload"));
    m_graphsMenu->addAction(tr("Payload Download"));
    m_graphsMenu->addAction(tr("Overhead Upload"));
    m_graphsMenu->addAction(tr("Overhead Download"));
    m_graphsMenu->addAction(tr("DHT Upload"));
    m_graphsMenu->addAction(tr("DHT Download"));
    m_graphsMenu->addAction(tr("Tracker Upload"));
    m_graphsMenu->addAction(tr("Tracker Download"));

    m_graphsMenuActions = m_graphsMenu->actions();

    for (int id = SpeedPlotView::UP; id < SpeedPlotView::NB_GRAPHS; ++id)
    {
        QAction *action = m_graphsMenuActions.at(id);
        action->setCheckable(true);
        action->setChecked(true);
        connect(action, &QAction::changed, this, [this, id]() { onGraphChange(id); });
    }

    m_graphsButton = new ComboBoxMenuButton(this, m_graphsMenu);
    m_graphsButton->addItem(tr("Select Graphs"));

    m_hlayout->addWidget(m_periodLabel);
    m_hlayout->addWidget(m_periodCombobox);
    m_hlayout->addStretch();
    m_hlayout->addWidget(m_graphsButton);

    m_plot = new SpeedPlotView(this);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::statsUpdated, this, &SpeedWidget::update);

    m_layout->addLayout(m_hlayout);
    m_layout->addWidget(m_plot);

    loadSettings();

    m_plot->show();
}

SpeedWidget::~SpeedWidget()
{
    qDebug("SpeedWidget::~SpeedWidget() ENTER");
    saveSettings();
    qDebug("SpeedWidget::~SpeedWidget() EXIT");
}

void SpeedWidget::update()
{
    const BitTorrent::SessionStatus &btStatus = BitTorrent::Session::instance()->status();

    SpeedPlotView::SampleData sampleData;
    sampleData[SpeedPlotView::UP] = btStatus.uploadRate;
    sampleData[SpeedPlotView::DOWN] = btStatus.downloadRate;
    sampleData[SpeedPlotView::PAYLOAD_UP] = btStatus.payloadUploadRate;
    sampleData[SpeedPlotView::PAYLOAD_DOWN] = btStatus.payloadDownloadRate;
    sampleData[SpeedPlotView::OVERHEAD_UP] = btStatus.ipOverheadUploadRate;
    sampleData[SpeedPlotView::OVERHEAD_DOWN] = btStatus.ipOverheadDownloadRate;
    sampleData[SpeedPlotView::DHT_UP] = btStatus.dhtUploadRate;
    sampleData[SpeedPlotView::DHT_DOWN] = btStatus.dhtDownloadRate;
    sampleData[SpeedPlotView::TRACKER_UP] = btStatus.trackerUploadRate;
    sampleData[SpeedPlotView::TRACKER_DOWN] = btStatus.trackerDownloadRate;

    m_plot->pushPoint(sampleData);
}

void SpeedWidget::onPeriodChange(int period)
{
    m_plot->setPeriod(static_cast<SpeedPlotView::TimePeriod>(period));
}

void SpeedWidget::onGraphChange(int id)
{
    QAction *action = m_graphsMenuActions.at(id);
    m_plot->setGraphEnable(static_cast<SpeedPlotView::GraphID>(id), action->isChecked());
}

void SpeedWidget::loadSettings()
{
    Preferences *preferences = Preferences::instance();

    int periodIndex = preferences->getSpeedWidgetPeriod();
    m_periodCombobox->setCurrentIndex(periodIndex);
    onPeriodChange(static_cast<SpeedPlotView::TimePeriod>(periodIndex));

    for (int id = SpeedPlotView::UP; id < SpeedPlotView::NB_GRAPHS; ++id)
    {
        QAction *action = m_graphsMenuActions.at(id);
        bool enable = preferences->getSpeedWidgetGraphEnable(id);

        action->setChecked(enable);
        m_plot->setGraphEnable(static_cast<SpeedPlotView::GraphID>(id), enable);
    }
}

void SpeedWidget::saveSettings() const
{
    Preferences *preferences = Preferences::instance();

    preferences->setSpeedWidgetPeriod(m_periodCombobox->currentIndex());

    for (int id = SpeedPlotView::UP; id < SpeedPlotView::NB_GRAPHS; ++id)
    {
        QAction *action = m_graphsMenuActions.at(id);
        preferences->setSpeedWidgetGraphEnable(id, action->isChecked());
    }
}
