#include <QList>
#include <QObject>

namespace Scheduler
{
    class Day;

    class Schedule
    {
    public:
        QList<Day *> days() const;

    private:
        QList<Day *> m_days;
    };
}
