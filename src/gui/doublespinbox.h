#ifndef DOUBLESPINBOX_H
#define DOUBLESPINBOX_H

#include <QWidget>
#include <QDoubleSpinBox>

class DoubleSpinBox : public QDoubleSpinBox
{
    Q_OBJECT
public:
    explicit DoubleSpinBox(QWidget *parent = nullptr);
    ~DoubleSpinBox();

protected:
    QString textFromValue(double val) const override;

private:
    Q_DISABLE_COPY(DoubleSpinBox)
};

#endif // DOUBLESPINBOX_H
