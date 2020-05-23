#include "streamingmanager.h"

StreamingManager *StreamingManager::m_instance = nullptr;

void StreamingManager::initInstance()
{
    if (!m_instance)
        m_instance = new StreamingManager;
}
void StreamingManager::freeInstance()
{
    if (m_instance)
        delete m_instance;
}

StreamingManager *StreamingManager::instance()
{
    return m_instance;
}

StreamingManager::StreamingManager(QObject *parent)
    : QObject(parent)
{
}