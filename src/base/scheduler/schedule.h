#include <QList>
#include <QObject>

namespace Scheduler
{
    class Day;

    class Schedule : public QObject
    {
    public:
        QList<Day *> days() const;

    private:
        QList<Day *> m_days;
    };
}
