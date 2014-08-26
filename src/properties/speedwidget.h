#ifndef SPEEDWIDGET_H
#define SPEEDWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QDateTime>
#include <QtConcurrentRun>
#include <qcustomplot.h>

#include <qbtsession.h>
#include <preferences.h>

class PropertiesWidget;

#define MAX_POINTS 21600 //Мах 6 hours

enum TimePeriod {
    MIN1 = 0,
    MIN5,
    MIN30,
    HOUR6
};

class SpeedWidget : public QWidget
{
    Q_OBJECT
public:
    SpeedWidget(PropertiesWidget *parent);
    ~SpeedWidget();

    void update();

signals:
    void speedChanged();

public slots:
    void graphUpdate();
    void on_period_change(int period);
    void loadSettings();
    void saveSettings() const;

private:
    QVBoxLayout *m_layout;
    QHBoxLayout *m_hlayout;
    QLabel *m_label;
    QComboBox *m_combobox;
    QCustomPlot *m_plot;
    PropertiesWidget *m_properties;

    QVector<double> graphTime,
    graphSpeedUp, graphSpeedDown,
    graphSpeedPayloadUp, graphSpeedPayloadDown,
    graphSpeedOverheadUp, graphSpeedOverheadDown,
    graphSpeedDHTUp, graphSpeedDHTDown,
    graphSpeedTrackerUp, graphSpeedTrackerDown;
    double maxSpeed;

    QFuture<void> updateRunner;

    bool isUpdating;
    int timePeriod;
};

#endif // SPEEDWIDGET_H
