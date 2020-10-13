#pragma once

#include <QDialog>
#include <QDateTime>

namespace Ui
{
    class TimeRangeDialog;
}

class TimeRangeDialog final : public QDialog
{
    Q_OBJECT

public:
    TimeRangeDialog(QWidget *parent, int initialRatioValue = 1000, int maxRatioValue = 10240000);
    ~TimeRangeDialog() override;

    QTime timeFrom() const;
    QTime timeTo() const;
    int downloadRatio() const;
    int uploadRatio() const;

public slots:
    void accept() override;

private:
    Ui::TimeRangeDialog *m_ui;
};
