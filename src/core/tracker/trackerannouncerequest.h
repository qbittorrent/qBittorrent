#ifndef TRACKERANNOUNCEREQUEST_H
#define TRACKERANNOUNCEREQUEST_H

#include <qpeer.h>

struct TrackerAnnounceRequest {
  QString info_hash;
  QString event;
  int numwant;
  QPeer peer;
  // Extensions
  bool no_peer_id;
};

#endif // TRACKERANNOUNCEREQUEST_H
