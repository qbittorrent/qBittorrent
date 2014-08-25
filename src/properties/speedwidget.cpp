
#include "propertieswidget.h"
#include "speedwidget.h"

SpeedWidget::SpeedWidget(PropertiesWidget *parent):
    QWidget(parent), m_properties(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0,0,0,0);

    m_hlayout = new QHBoxLayout();
    m_hlayout->setContentsMargins(0,0,0,0);

    m_label = new QLabel("<b>"+tr("Period:")+"</b>");

    m_combobox = new QComboBox();
    m_combobox->addItem(tr("1 min"));
    m_combobox->addItem(tr("5 min"));
    m_combobox->addItem(tr("30 min"));
    m_combobox->addItem(tr("6 hours"));

    m_hlayout->addWidget(m_label);
    m_hlayout->addWidget(m_combobox);
    m_hlayout->addStretch();

    connect(m_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_period_change(int)));

    m_plot = new QCustomPlot();

    m_layout->addLayout(m_hlayout);
    m_layout->addWidget(m_plot);

    m_plot->addGraph();
    QPen greenPen;
    greenPen.setColor(QColor(134, 196, 63));
    m_plot->graph(0)->setPen(greenPen);
    m_plot->graph(0)->setName(tr("download"));

    m_plot->addGraph();
    QPen bluePen;
    bluePen.setColor(QColor(50, 153, 255));
    m_plot->graph(1)->setPen(bluePen);
    m_plot->graph(1)->setName(tr("upload"));

    m_plot->addGraph();
    greenPen.setStyle(Qt::DashLine);
    m_plot->graph(2)->setPen(greenPen);
    m_plot->graph(2)->setName(tr("payload download"));

    m_plot->addGraph();
    bluePen.setStyle(Qt::DashLine);
    m_plot->graph(3)->setPen(bluePen);
    m_plot->graph(3)->setName(tr("payload upload"));

    m_plot->addGraph();
    greenPen.setStyle(Qt::DashDotLine);
    m_plot->graph(4)->setPen(greenPen);
    m_plot->graph(4)->setName(tr("overhead download"));

    m_plot->addGraph();
    bluePen.setStyle(Qt::DashDotLine);
    m_plot->graph(5)->setPen(bluePen);
    m_plot->graph(5)->setName(tr("overhead upload"));

    m_plot->addGraph();
    greenPen.setStyle(Qt::DashDotDotLine);
    m_plot->graph(6)->setPen(greenPen);
    m_plot->graph(6)->setName(tr("DHT download"));

    m_plot->addGraph();
    bluePen.setStyle(Qt::DashDotDotLine);
    m_plot->graph(7)->setPen(bluePen);
    m_plot->graph(7)->setName(tr("DHT upload"));

    m_plot->addGraph();
    greenPen.setStyle(Qt::DotLine);
    m_plot->graph(8)->setPen(greenPen);
    m_plot->graph(8)->setName(tr("tracker download"));

    m_plot->addGraph();
    bluePen.setStyle(Qt::DotLine);
    m_plot->graph(9)->setPen(bluePen);
    m_plot->graph(9)->setName(tr("tracker upload"));

    m_plot->xAxis->setTickLabelType(QCPAxis::ltDateTime);
    m_plot->xAxis->setAutoTickStep(false);

    m_plot->yAxis->setLabel(tr("KiB/s"));

    m_plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignLeft|Qt::AlignTop);
    m_plot->legend->setVisible(true);
    m_plot->legend->setBorderPen(Qt::NoPen);
    m_plot->legend->setBrush(Qt::NoBrush);
    QFont legendFont = font();
    legendFont.setPointSize(8);
    m_plot->legend->setFont(legendFont);

    loadSettings();

    isUpdating = true;
    connect(this, SIGNAL(speedChanged()), this, SLOT(graphUpdate()));
    updateRunner = QtConcurrent::run(this, &SpeedWidget::update);
}

SpeedWidget::~SpeedWidget()
{
    isUpdating = false;
    updateRunner.waitForFinished();

    saveSettings();

    delete m_plot;
    delete m_combobox;
    delete m_label;
    delete m_hlayout;
    delete m_layout;
}

void SpeedWidget::update()
{
    while (isUpdating) {

        if (graphTime.size() > MAX_POINTS)
        {
            graphTime.pop_front();
            graphSpeedUp.pop_front();
            graphSpeedDown.pop_front();
            graphSpeedPayloadUp.pop_front();
            graphSpeedPayloadDown.pop_front();
            graphSpeedOverheadUp.pop_front();
            graphSpeedOverheadDown.pop_front();
            graphSpeedDHTUp.pop_front();
            graphSpeedDHTDown.pop_front();
            graphSpeedTrackerUp.pop_front();
            graphSpeedTrackerDown.pop_front();
        }

        libtorrent::session_status btStatus = QBtSession::instance()->getSessionStatus();

        graphTime.push_back(QDateTime::currentDateTime().toTime_t());
        graphSpeedUp.push_back(btStatus.upload_rate/1000.);
        graphSpeedDown.push_back(btStatus.download_rate/1000.);
        graphSpeedPayloadUp.push_back(btStatus.payload_upload_rate/1000.);
        graphSpeedPayloadDown.push_back(btStatus.payload_download_rate/1000.);
        graphSpeedOverheadUp.push_back(btStatus.ip_overhead_upload_rate/1000.);
        graphSpeedOverheadDown.push_back(btStatus.ip_overhead_download_rate/1000.);
        graphSpeedDHTUp.push_back(btStatus.dht_upload_rate/1000.);
        graphSpeedDHTDown.push_back(btStatus.dht_download_rate/1000.);
        graphSpeedTrackerUp.push_back(btStatus.tracker_upload_rate/1000.);
        graphSpeedTrackerDown.push_back(btStatus.tracker_download_rate/1000.);

        maxSpeed = 0;

        double lowTimelimit = graphTime.back()-timePeriod;

        for (int i = graphSpeedUp.size()-1; i >= 0; --i)
        {
            if (graphTime[i] < lowTimelimit) break;

            if (graphSpeedUp[i] > maxSpeed)
                maxSpeed = graphSpeedUp[i];
            if (graphSpeedDown[i] > maxSpeed)
                maxSpeed = graphSpeedDown[i];

            if (graphSpeedPayloadUp[i] > maxSpeed)
                maxSpeed = graphSpeedPayloadUp[i];
            if (graphSpeedPayloadDown[i] > maxSpeed)
                maxSpeed = graphSpeedPayloadDown[i];

            if (graphSpeedOverheadUp[i] > maxSpeed)
                maxSpeed = graphSpeedOverheadUp[i];
            if (graphSpeedOverheadDown[i] > maxSpeed)
                maxSpeed = graphSpeedOverheadDown[i];

            if (graphSpeedDHTUp[i] > maxSpeed)
                maxSpeed = graphSpeedDHTUp[i];
            if (graphSpeedDHTDown[i] > maxSpeed)
                maxSpeed = graphSpeedDHTDown[i];

            if (graphSpeedTrackerUp[i] > maxSpeed)
                maxSpeed = graphSpeedTrackerUp[i];
            if (graphSpeedTrackerDown[i] > maxSpeed)
                maxSpeed = graphSpeedTrackerDown[i];
        }

        maxSpeed = ceil(maxSpeed); // +5% to upper bound

        emit speedChanged();
        sleep(1);
    }
}

void SpeedWidget::graphUpdate()
{    
    m_plot->graph(0)->setData(graphTime, graphSpeedDown);
    m_plot->graph(1)->setData(graphTime, graphSpeedUp);

    m_plot->graph(2)->setData(graphTime, graphSpeedPayloadDown);
    m_plot->graph(3)->setData(graphTime, graphSpeedPayloadUp);

    m_plot->graph(4)->setData(graphTime, graphSpeedOverheadDown);
    m_plot->graph(5)->setData(graphTime, graphSpeedOverheadUp);

    m_plot->graph(6)->setData(graphTime, graphSpeedDHTDown);
    m_plot->graph(7)->setData(graphTime, graphSpeedDHTUp);

    m_plot->graph(8)->setData(graphTime, graphSpeedTrackerDown);
    m_plot->graph(9)->setData(graphTime, graphSpeedTrackerUp);

    m_plot->xAxis->setRange(graphTime.back()-timePeriod, graphTime.back());
    m_plot->yAxis->setRange(0, maxSpeed);

    m_plot->yAxis->setAutoTicks(false);

    QVector<double> ticks;
    ticks.push_back(0.0);
    ticks.push_back(maxSpeed);

    m_plot->yAxis->setTickVector(ticks);

    m_plot->replot();
}

void SpeedWidget::on_period_change(int period)
{
    switch (period) {
    case MIN1:
        timePeriod = 60;
        m_plot->xAxis->setDateTimeFormat("hh:mm:ss");
        m_plot->xAxis->setTickStep(6);
        m_plot->xAxis->setSubTickCount(6);
        break;
    case MIN5:
        timePeriod = 300;
        m_plot->xAxis->setDateTimeFormat("hh:mm:ss");
        m_plot->xAxis->setTickStep(30);
        m_plot->xAxis->setSubTickCount(6);
        break;
    case MIN30:
        timePeriod = 1800;
        m_plot->xAxis->setDateTimeFormat("hh:mm");
        m_plot->xAxis->setTickStep(180);
        m_plot->xAxis->setSubTickCount(6);
        break;
    case HOUR6:
        timePeriod = MAX_POINTS;
        m_plot->xAxis->setTickStep(1800);
        m_plot->xAxis->setSubTickCount(6);
        m_plot->xAxis->setDateTimeFormat("hh:mm");
        break;
    default:
        break;
    }
}

void SpeedWidget::loadSettings()
{
    int periodIndex = Preferences::instance()->getSpeedWidgetPeriod();
    m_combobox->setCurrentIndex(periodIndex);
    on_period_change(periodIndex);
}

void SpeedWidget::saveSettings() const
{
    Preferences::instance()->setSpeedWidgetPeriod(m_combobox->currentIndex());
}



