#include "executionlog.h"
#include "ui_executionlog.h"
#include "qbtsession.h"
#include "iconprovider.h"

ExecutionLog::ExecutionLog(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ExecutionLog)
{
    ui->setupUi(this);
    ui->tabConsole->setTabIcon(0, IconProvider::instance()->getIcon("view-calendar-journal"));
    ui->tabConsole->setTabIcon(1, IconProvider::instance()->getIcon("view-filter"));
    ui->textConsole->setHtml(QBtSession::instance()->getConsoleMessages().join("<br>"));
    connect(QBtSession::instance(), SIGNAL(newConsoleMessage(QString)), SLOT(addLogMessage(QString)));
    ui->textBannedPeers->setHtml(QBtSession::instance()->getPeerBanMessages().join("<br>"));
    connect(QBtSession::instance(), SIGNAL(newBanMessage(QString)), SLOT(addBanMessage(QString)));
}

ExecutionLog::~ExecutionLog()
{
  delete ui;
}

void ExecutionLog::addLogMessage(const QString &msg)
{
   ui->textConsole->setHtml(msg+ui->textConsole->toHtml());
}

void ExecutionLog::addBanMessage(const QString &msg)
{
   ui->textBannedPeers->setHtml(msg+ui->textBannedPeers->toHtml());
}
