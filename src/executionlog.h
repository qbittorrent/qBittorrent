#ifndef EXECUTIONLOG_H
#define EXECUTIONLOG_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class ExecutionLog;
}
QT_END_NAMESPACE

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
