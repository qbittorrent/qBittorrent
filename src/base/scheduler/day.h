#include <QList>
#include <QObject>

namespace Scheduler
{
    class TimeRange;

    class Day : public QObject
    {
    public:
        bool addTimeRange(const TimeRange &timeRange);
        QList<TimeRange *> timeRanges() const;

    private:
        QList<TimeRange *> m_timeRanges;
    };
}
