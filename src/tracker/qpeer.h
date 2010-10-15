#ifndef QPEER_H
#define QPEER_H

#include <libtorrent/entry.hpp>
#include <QString>

using namespace libtorrent;

struct QPeer {

  bool operator!=(const QPeer &other) const {
    return qhash() != other.qhash();
  }

  bool operator==(const QPeer &other) const {
    return qhash() == other.qhash();
  }

  QString qhash() const {
    return ip+":"+QString::number(port);
  }

  entry toEntry(bool no_peer_id) const {
    entry::dictionary_type peer_map;
    if(!no_peer_id)
      peer_map["id"] = entry(peer_id.toStdString());
    peer_map["ip"] = entry(ip.toStdString());
    peer_map["port"] = entry(port);
    return entry(peer_map);
  }

  QString ip;
  QString peer_id;
  int port;
};

#endif // QPEER_H
