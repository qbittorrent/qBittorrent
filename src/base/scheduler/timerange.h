#include <QJsonObject>
#include <QObject>
#include <QTime>

namespace Scheduler
{
    class TimeRange : public QObject
    {
        TimeRange(const QTime &startTime, const QTime &endTime, int downloadRate, int uploadRate);

    public:
        QTime startTime() const;
        QTime endTime() const;
        int downloadRate() const;
        int uploadRate() const;

        QJsonObject toJsonObject() const;

    private:
        QTime m_startTime = QTime(0, 0);
        QTime m_endTime = QTime(23, 59);
        int m_downloadRate = -1;
        int m_uploadRate = -1;
    };
}
