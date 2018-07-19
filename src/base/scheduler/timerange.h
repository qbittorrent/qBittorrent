#include <QTime>

class QJsonObject;

namespace Scheduler
{
    class TimeRange
    {
        TimeRange();
        TimeRange(int startHours, int startMinutes, int endHours, int endMinutes, int downloadRate, int uploadRate);

    public:
        QTime startTime() const;
        bool setStartTime(int hours, int minutes);

        QTime endTime() const;
        bool setEndTime(int hours, int minutes);

        int downloadRate() const;
        void setDownloadRate(int downloadRate);

        int uploadRate() const;
        void setUploadRate(int uploadRate);

        bool isValid() const;
        QJsonObject toJsonObject() const;

    private:
        QTime m_startTime = QTime(0, 0);
        QTime m_endTime = QTime(0, 0);
        int m_downloadRate = -1;
        int m_uploadRate = -1;
    };
}
