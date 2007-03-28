#include "upnp.h"

int callBackCPRegister( Upnp_EventType EventType, void* Event, void* Cookie ){
  switch(EventType){
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
    case UPNP_DISCOVERY_SEARCH_RESULT:
    {
      struct Upnp_Discovery *d_event = (struct Upnp_Discovery*) Event;
      IXML_Document *DescDoc=NULL;
      int ret = UpnpDownloadXmlDoc(d_event->Location, &DescDoc);
      if(ret != UPNP_E_SUCCESS){
        // silently ignore?
      }else{
        ((UPnPHandler*)Cookie)->addUPnPDevice(d_event);
//         TvCtrlPointAddDevice(DescDoc, d_event->Location, d_event->Expires);
      }
      if(DescDoc) ixmlDocument_free(DescDoc);
//       TvCtrlPointPrintList();
      break;
    }
    case UPNP_DISCOVERY_SEARCH_TIMEOUT:
      qDebug("UPnP devices discovery timed out...");
      break;
    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
    {
      struct Upnp_Discovery *d_event = (struct Upnp_Discovery*) Event;
      ((UPnPHandler*)Cookie)->removeUPnPDevice(QString(d_event->DeviceId));
//       TvCtrlPointPrintList();
      break;
    }
    default:
      qDebug("Debug: UPnP Unhandled event");
  }
  return 0;
}

void UPnPHandler::run(){
  // Initialize the SDK
  int ret = UpnpInit(NULL, 0);
  if(ret != UPNP_E_SUCCESS){
    qDebug("Fatal error: UPnP initialization failed");
    UpnpFinish();
    return;
  }
  qDebug("UPnP successfully initialized");
  qDebug("UPnP bind on IP %s:%d", UpnpGetServerIpAddress(), UpnpGetServerPort());
  // Register the UPnP control point
  ret = UpnpRegisterClient(callBackCPRegister, this, &UPnPClientHandle);
  if(ret != UPNP_E_SUCCESS){
    qDebug("Fatal error: UPnP control point registration failed");
    UpnpFinish();
    return;
  }
  qDebug("UPnP control point successfully registered");
  // Look for UPnP enabled routers (devices)
  ret = UpnpSearchAsync(UPnPClientHandle, WAIT_TIMEOUT, "upnp:rootdevice", this);
}