#ifndef UPDOWNRATIODLG_H
#define UPDOWNRATIODLG_H

#include <QtGui/QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
    class UpDownRatioDlg;
}
QT_END_NAMESPACE

class UpDownRatioDlg : public QDialog
{
    Q_OBJECT

public:
    explicit UpDownRatioDlg(bool useDefault, qreal initialValue, qreal maxValue,
        QWidget *parent = 0);
    ~UpDownRatioDlg();

    bool useDefault() const;
    qreal ratio() const;

private slots:
    void handleRatioTypeChanged();

private:
    Ui::UpDownRatioDlg *ui;
};

#endif // UPDOWNRATIODLG_H
