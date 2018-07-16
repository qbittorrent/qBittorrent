#include <QObject>
#include <QTime>

namespace Scheduler
{
    class TimeRange : public QObject
    {
        TimeRange(const QTime &startTime, const QTime &endTime, int dlRate, int ulRate);

    public:
        QTime startTime() const;
        QTime endTime() const;
        int dlRate() const;
        int ulRate() const;

    private:
        QTime m_startTime = QTime(0, 0);
        QTime m_endTime = QTime(23, 59);
        int m_dlRate = -1;
        int m_ulRate = -1;
    };
}
