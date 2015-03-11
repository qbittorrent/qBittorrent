#include "signalblocker.h"

SignalBlocker::SignalBlocker(QObject *p, bool block)
    :m_object(p), m_previousState(m_object->blockSignals(block))
{

}

SignalBlocker::SignalBlocker(QObject &p, bool block)
    :m_object(&p), m_previousState(m_object->blockSignals(block))
{

}

SignalBlocker::~SignalBlocker()
{
    m_object->blockSignals(m_previousState);
}
