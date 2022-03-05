#include "doublespinbox.h"
#include "base/utils/string.h"

DoubleSpinBox::DoubleSpinBox(QWidget *parent) : QDoubleSpinBox(parent) {}

QString DoubleSpinBox::textFromValue(qreal val) const
{
    return Utils::String::fromDouble(val, 2);
}
