
#pragma once

#include <QObject>

class StreamingManager : public QObject
{
public:
    void initInstance();
    void freeInstance();
    StreamingManager *instance();

private:
    StreamingManager(QObject *parent = nullptr);

    static StreamingManager *m_instance;
};