#ifndef EXECUTIONLOG_H
#define EXECUTIONLOG_H

#include <QWidget>

namespace Ui {
    class ExecutionLog;
}

class ExecutionLog : public QWidget
{
    Q_OBJECT

public:
    explicit ExecutionLog(QWidget *parent = 0);
    ~ExecutionLog();

public slots:
  void addLogMessage(const QString &msg);
  void addBanMessage(const QString &msg);

private:
    Ui::ExecutionLog *ui;
};

#endif // EXECUTIONLOG_H
