#include "schedulecontroller.h"

#include "base/bittorrent/scheduler/bandwidthscheduler.h"
#include "webui/api/apierror.h"

void ScheduleController::removeScheduleEntryAction()
{
    requireParams({"day", "index"});

    const int day = params()["day"].toInt();
    const int index = params()["index"].toInt();

    if (day < 0 || day > 6)
        throw APIError(APIErrorType::BadParams, tr("Invalid schedule day index"));

    if (!BandwidthScheduler::instance()->scheduleDay(day)->removeEntryAt(index))
        throw APIError(APIErrorType::Conflict, tr("Invalid schedule entry index"));
}
