#include "apicontroller.h"

class TransmissionRPCController : public APIController
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TransmissionRPCController)

public:
    using APIController::APIController;

private slots:
    void rpcAction();
};
