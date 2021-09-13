#pragma once

#include "apicontroller.h"

class ScheduleController : public APIController
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ScheduleController)

public:
    using APIController::APIController;

private slots:
    void isEntryValidAction();
    void addEntryAction();
    void removeEntryAction();
    void clearDayAction();
    void pasteAction();
    void copyToOtherDaysAction();
    void getJsonAction();
    void loadFromDiskAction();
};
