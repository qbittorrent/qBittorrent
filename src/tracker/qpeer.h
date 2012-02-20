#ifndef QPEER_H
#define QPEER_H

#include <libtorrent/entry.hpp>
#include <QString>

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

  libtorrent::entry toEntry(bool no_peer_id) const {
    libtorrent::entry::dictionary_type peer_map;
    if (!no_peer_id)
      peer_map["id"] = libtorrent::entry(peer_id.toStdString());
    peer_map["ip"] = libtorrent::entry(ip.toStdString());
    peer_map["port"] = libtorrent::entry(port);
    return libtorrent::entry(peer_map);
  }

  QString ip;
  QString peer_id;
  int port;
};

#endif // QPEER_H
