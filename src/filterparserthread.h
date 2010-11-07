/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 *
 * Contact : chris@qbittorrent.org
 */

#ifndef FILTERPARSERTHREAD_H
#define FILTERPARSERTHREAD_H

#include <QThread>
#include <QFile>
#include <QDataStream>
#include <QStringList>

#include <libtorrent/session.hpp>
#include <libtorrent/ip_filter.hpp>

using namespace libtorrent;
using namespace std;

// P2B Stuff
#include <string.h>
#ifdef Q_WS_WIN
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#endif
// End of P2B stuff

class FilterParserThread : public QThread  {
  Q_OBJECT

private:
  session *s;
  ip_filter filter;
  bool abort;
  QString filePath;

protected:
  void run(){
    qDebug("Processing filter file");
    if(filePath.endsWith(".dat", Qt::CaseInsensitive)) {
      // eMule DAT file
      parseDATFilterFile(filePath);
    } else {
      if(filePath.endsWith(".p2p", Qt::CaseInsensitive)) {
        // PeerGuardian p2p file
        parseP2PFilterFile(filePath);
      } else {
        if(filePath.endsWith(".p2b", Qt::CaseInsensitive)) {
          // PeerGuardian p2b file
          parseP2BFilterFile(filePath);
        } else {
          // Default: eMule DAT format
          parseDATFilterFile(filePath);
        }
      }
    }
    s->set_ip_filter(filter);
    qDebug("IP Filter thread: finished parsing, filter applied");
  }

public:
  FilterParserThread(QObject* parent, session *s) : QThread(parent), s(s), abort(false) {

  }

  ~FilterParserThread(){
    abort = true;
    wait();
  }

  // Parser for eMule ip filter in DAT format
  void parseDATFilterFile(QString filePath) {
    QFile file(filePath);
    if (file.exists()){
      if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        std::cerr << "I/O Error: Could not open ip filer file in read mode." << std::endl;
        return;
      }
      unsigned int nbLine = 0;
      while (!file.atEnd() && !abort) {
        ++nbLine;
        QByteArray line = file.readLine();
        // Ignoring empty lines
        line = line.trimmed();
        if(line.isEmpty()) continue;
        // Ignoring commented lines
        if(line.startsWith('#') || line.startsWith("//")) continue;

        // Line should be splitted by commas
        QList<QByteArray> partsList = line.split(',');
        const uint nbElem = partsList.size();

        // IP Range should be splitted by a dash
        QList<QByteArray> IPs = partsList.first().split('-');
        if(IPs.size() != 2) {
          qDebug("Ipfilter.dat: line %d is malformed.", nbLine);
          qDebug("Line was %s", line.constData());
          continue;
        }

        boost::system::error_code ec;
        const QString strStartIP = IPs.at(0).trimmed();
        libtorrent::address startAddr = libtorrent::address::from_string(qPrintable(strStartIP), ec);
        if(ec) {
          qDebug("Ipfilter.dat: line %d is malformed.", nbLine);
          qDebug("Start IP of the range is malformated: %s", qPrintable(strStartIP));
          continue;
        }
        const QString strEndIP = IPs.at(1).trimmed();
        libtorrent::address endAddr = libtorrent::address::from_string(qPrintable(strEndIP), ec);
        if(ec) {
          qDebug("Ipfilter.dat: line %d is malformed.", nbLine);
          qDebug("End IP of the range is malformated: %s", qPrintable(strEndIP));
          continue;
        }
        if(startAddr.is_v4() != endAddr.is_v4()) {
          qDebug("Ipfilter.dat: line %d is malformed.", nbLine);
          qDebug("One IP is IPv4 and the other is IPv6!");
          continue;
        }

        // Check if there is an access value (apparently not mandatory)
        int nbAccess = 0;
        if(nbElem > 1) {
          // There is possibly one
          nbAccess = partsList.at(1).trimmed().toInt();
        }

        if(nbAccess > 127) {
          // Ignoring this rule because access value is too high
          continue;
        }
        // Now Add to the filter
        try {
          filter.add_rule(startAddr, endAddr, ip_filter::blocked);
        }catch(exception){
          qDebug("Bad line in filter file, avoided crash...");
        }
      }
      file.close();
    }
  }

  // Parser for PeerGuardian ip filter in p2p format
  void parseP2PFilterFile(QString filePath) {
    QFile file(filePath);
    QStringList IP;
    if (file.exists()){
      if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        std::cerr << "I/O Error: Could not open ip filer file in read mode." << std::endl;
        return;
      }
      unsigned int nbLine = 0;
      while (!file.atEnd() && !abort) {
        ++nbLine;
        QByteArray line = file.readLine().trimmed();
        if(line.isEmpty()) continue;
        // Ignoring commented lines
        if(line.startsWith('#') || line.startsWith("//")) continue;
        // Line is splitted by :
        QList<QByteArray> partsList = line.split(':');
        if(partsList.size() < 2){
          qDebug("p2p file: line %d is malformed.", nbLine);
          continue;
        }
        // Get IP range
        QList<QByteArray> IPs = partsList.last().split('-');
        if(IPs.size() != 2) {
          qDebug("p2p file: line %d is malformed.", nbLine);
          qDebug("line was: %s", line.constData());
          continue;
        }
        boost::system::error_code ec;
        QString strStartIP = IPs.at(0).trimmed();
        libtorrent::address startAddr = libtorrent::address::from_string(qPrintable(strStartIP), ec);
        if(ec) {
          qDebug("p2p file: line %d is malformed.", nbLine);
          qDebug("Start IP is invalid: %s", qPrintable(strStartIP));
          continue;
        }
        QString strEndIP = IPs.at(1).trimmed();
        libtorrent::address endAddr = libtorrent::address::from_string(qPrintable(strEndIP), ec);
        if(ec) {
          qDebug("p2p file: line %d is malformed.", nbLine);
          qDebug("End IP is invalid: %s", qPrintable(strStartIP));
          continue;
        }
        if(startAddr.is_v4() != endAddr.is_v4()) {
          qDebug("p2p file: line %d is malformed.", nbLine);
          qDebug("Line was: %s", line.constData());
          continue;
        }
        try {
          filter.add_rule(startAddr, endAddr, ip_filter::blocked);
        } catch(std::exception&) {
          qDebug("p2p file: line %d is malformed.", nbLine);
          qDebug("Line was: %s", line.constData());
          continue;
        }
      }
      file.close();
    }
  }

  int getlineInStream(QDataStream& stream, string& name, char delim) {
    char c;
    int total_read = 0;
    int read;
    do {
      read = stream.readRawData(&c, 1);
      total_read += read;
      if(read > 0) {
        if(c != delim) {
          name += c;
        } else {
          // Delim found
          return total_read;
        }
      }
    } while(read > 0);
    return total_read;
  }

  // Parser for PeerGuardian ip filter in p2p format
  void parseP2BFilterFile(QString filePath) {
    QFile file(filePath);
    if (file.exists()){
      if(!file.open(QIODevice::ReadOnly)){
        std::cerr << "I/O Error: Could not open ip filer file in read mode." << std::endl;
        return;
      }
      QDataStream stream(&file);
      // Read header
      char buf[7];
      unsigned char version;
      if(
          !stream.readRawData(buf, sizeof(buf)) ||
          memcmp(buf, "\xFF\xFF\xFF\xFFP2B", 7) ||
          !stream.readRawData((char*)&version, sizeof(version))
          ) {
        std::cerr << "Parsing Error: The filter file is not a valid PeerGuardian P2B file." << std::endl;
        return;
      }

      if(version==1 || version==2) {
        qDebug ("p2b version 1 or 2");
        unsigned int start, end;

        string name;
        while(getlineInStream(stream, name, '\0') && !abort) {
          if(
              !stream.readRawData((char*)&start, sizeof(start)) ||
              !stream.readRawData((char*)&end, sizeof(end))
              ) {
            std::cerr << "Parsing Error: The filter file is not a valid PeerGuardian P2B file." << std::endl;
            return;
          }
          // Network byte order to Host byte order
          // asio address_v4 contructor expects it
          // that way
          address_v4 first(ntohl(start));
          address_v4 last(ntohl(end));
          // Apply to bittorrent session
          filter.add_rule(first, last, ip_filter::blocked);
        }
      }
      else if(version==3) {
        qDebug ("p2b version 3");
        unsigned int namecount;
        if(!stream.readRawData((char*)&namecount, sizeof(namecount))) {
          std::cerr << "Parsing Error: The filter file is not a valid PeerGuardian P2B file." << std::endl;
          return;
        }
        namecount=ntohl(namecount);
        // Reading names although, we don't really care about them
        for(unsigned int i=0; i<namecount; i++) {
          string name;
          if(!getlineInStream(stream, name, '\0')) {
            std::cerr << "Parsing Error: The filter file is not a valid PeerGuardian P2B file." << std::endl;
            return;
          }
          if(abort) return;
        }
        // Reading the ranges
        unsigned int rangecount;
        if(!stream.readRawData((char*)&rangecount, sizeof(rangecount))) {
          std::cerr << "Parsing Error: The filter file is not a valid PeerGuardian P2B file." << std::endl;
          return;
        }
        rangecount=ntohl(rangecount);

        unsigned int name, start, end;

        for(unsigned int i=0; i<rangecount; i++) {
          if(
              !stream.readRawData((char*)&name, sizeof(name)) ||
              !stream.readRawData((char*)&start, sizeof(start)) ||
              !stream.readRawData((char*)&end, sizeof(end))
              ) {
            std::cerr << "Parsing Error: The filter file is not a valid PeerGuardian P2B file." << std::endl;
            return;
          }
          // Network byte order to Host byte order
          // asio address_v4 contructor expects it
          // that way
          address_v4 first(ntohl(start));
          address_v4 last(ntohl(end));
          // Apply to bittorrent session
          filter.add_rule(first, last, ip_filter::blocked);
          if(abort) return;
        }
      } else {
        std::cerr << "Parsing Error: The filter file is not a valid PeerGuardian P2B file." << std::endl;
        return;
      }
      file.close();
    }
  }

  // Process ip filter file
  // Supported formats:
  //  * eMule IP list (DAT): http://wiki.phoenixlabs.org/wiki/DAT_Format
  //  * PeerGuardian Text (P2P): http://wiki.phoenixlabs.org/wiki/P2P_Format
  //  * PeerGuardian Binary (P2B): http://wiki.phoenixlabs.org/wiki/P2B_Format
  void processFilterFile(QString _filePath){
    // First, import current filter
    filter = s->get_ip_filter();
    if(isRunning()) {
      // Already parsing a filter, abort first
      abort = true;
      wait();
    }
    abort = false;
    filePath = _filePath;
    // Run it
    start();
  }

  static void processFilterList(session *s, QStringList IPs) {
    // First, import current filter
    ip_filter filter = s->get_ip_filter();
    foreach(const QString &ip, IPs) {
      qDebug("Manual ban of peer %s", ip.toLocal8Bit().constData());
      boost::system::error_code ec;
      address_v4 addr = address_v4::from_string(ip.toLocal8Bit().constData(), ec);
      Q_ASSERT(!ec);
      if(!ec)
        filter.add_rule(addr, addr, ip_filter::blocked);
    }
    s->set_ip_filter(filter);
  }

};

#endif
