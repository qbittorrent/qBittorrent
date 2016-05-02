#ifndef CATEGORYOPTIONSDIALOG_H
#define CATEGORYOPTIONSDIALOG_H

#include <QDialog>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QSlider>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class CategoryOptionsDialog;
}
QT_END_NAMESPACE

class CategoryOptionsDialog: public QDialog
{
    Q_OBJECT

public:
    explicit CategoryOptionsDialog(QString category, QWidget *parent = 0);
    ~CategoryOptionsDialog();

protected slots:
    void on_browseButton_clicked();
    void on_buttonBox_accepted();
    void updateSpinValue(QSpinBox *box, int val) const; // consider making a util method to share with speed dialog
    void updateSliderValue(QSpinBox *box, QSlider *slider, int val) const; // consider making a util method to share with speed dialog
    void updateDownloadSpinValue(int val) const;
    void updateDownloadSliderValue(int val) const;
    void updateUploadSpinValue(int val) const;
    void updateUploadSliderValue(int val) const;
    void ratioLimitTypeChanged(int id);

private:
    Ui::CategoryOptionsDialog *ui;
    QString m_path;
    QString m_category;
    int m_ratioLimitType;
};

#endif // CATEGORYOPTIONSDIALOG_H
