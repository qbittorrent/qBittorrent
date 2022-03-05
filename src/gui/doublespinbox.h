#ifndef DOUBLESPINBOX_H
#define DOUBLESPINBOX_H

#include <QWidget>
#include <QDoubleSpinBox>

class DoubleSpinBox : public QDoubleSpinBox
{
    Q_OBJECT
public:
    explicit DoubleSpinBox(QWidget *parent = nullptr);

protected:
    QString textFromValue(double val) const override;
};

#endif // DOUBLESPINBOX_H
