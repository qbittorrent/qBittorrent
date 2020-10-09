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
    TimeRangeDialog(qreal initialRatioValue, qreal maxRatioValue, QWidget *parent = nullptr);
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
