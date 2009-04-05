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
#include <QMessageBox>

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
            if(filePath.endsWith(".p2p", Qt::CaseInsensitive)) {
              // PeerGuardian p2p file
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
      const QRegExp is_ipv6(QString::fromUtf8("^[0-9a-f]{4}(:[0-9a-f]{4}){7}$"), Qt::CaseInsensitive, QRegExp::RegExp);
      const QRegExp is_ipv4(QString::fromUtf8("^(([0-1]?[0-9]?[0-9])|(2[0-4][0-9])|(25[0-5]))(\\.(([0-1]?[0-9]?[0-9])|(2[0-4][0-9])|(25[0-5]))){3}$"), Qt::CaseInsensitive, QRegExp::RegExp);
      QString strStartIP, strEndIP;
      bool IPv4 = true;
      QFile file(filePath);
      if (file.exists()){
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
          QMessageBox::critical(0, tr("I/O Error", "Input/Output Error"), tr("Couldn't open %1 in read mode.").arg(filePath));
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
          // Line is not commented
          QList<QByteArray> partsList = line.split(',');
          unsigned int nbElem = partsList.size();
          // IP Range can be splitted by a dash or a comma...
          // Check if there is a dash in first part
          QByteArray firstPart = partsList.at(0);
          int nbAccess = 0;
          if(firstPart.contains('-')) {
            // Range is splitted by a dash
            QList<QByteArray> IPs = firstPart.split('-');
            if(IPs.size() != 2) {
              qDebug("Ipfilter.dat: line %d is malformed.", nbLine);
              continue;
            }
            strStartIP = IPs.at(0).trimmed();
            strEndIP = IPs.at(1).trimmed();
            // Check if IPs are correct
            if(strStartIP.contains(is_ipv4) && strEndIP.contains(is_ipv4)) {
              IPv4 = true;
            } else {
              if(strStartIP.contains(is_ipv6) && strEndIP.contains(is_ipv6)) {
                IPv4 = false;
              } else {
                // Could not determine IP format
                qDebug("Ipfilter.dat: line %d is malformed.", nbLine);
                continue;
              }
            }
            // Check if there is an access value (apparently not mandatory)
            if(nbElem > 1) {
              // There is possibly one
              bool ok;
              nbAccess = partsList.at(1).trimmed().toInt(&ok);
              if(!ok){
                nbAccess = 0;
              }
            }
          } else {
            // Range is probably splitted by a comma
            unsigned int nbElem = partsList.size();
            if(nbElem > 1) {
              strStartIP = firstPart.trimmed();
              strEndIP = partsList.at(1).trimmed();
              // Check if IPs are correct
              if(strStartIP.contains(is_ipv4) && strEndIP.contains(is_ipv4)) {
                IPv4 = true;
              } else {
                if(strStartIP.contains(is_ipv6) && strEndIP.contains(is_ipv6)) {
                  IPv4 = false;
                } else {
                  // Could not determine IP format
                  qDebug("Ipfilter.dat: line %d is malformed.", nbLine);
                  continue;
                }
              }
              // Check if there is an access value (apparently not mandatory)
              if(nbElem > 2) {
                // There is possibly one
                bool ok;
                nbAccess = partsList.at(2).trimmed().toInt(&ok);
                if(!ok){
                  nbAccess = 0;
                }
              }
            }
          }
          if(nbAccess > 127) {
            // Ignoring this rule because access value is too high
            continue;
          }
          // Now Add to the filter
          QStringList IP;
          try {
              if(IPv4) {
                //IPv4 addresses
                IP = strStartIP.split('.');
                address_v4 start((IP.at(0).toInt() << 24) + (IP.at(1).toInt() << 16) + (IP.at(2).toInt() << 8) + IP.at(3).toInt());
                IP = strEndIP.split('.');
                address_v4 last((IP.at(0).toInt() << 24) + (IP.at(1).toInt() << 16) + (IP.at(2).toInt() << 8) + IP.at(3).toInt());
                // Apply to bittorrent session
                filter.add_rule(start, last, ip_filter::blocked);
              } else {
                // IPv6, ex :   1fff:0000:0a88:85a3:0000:0000:ac1f:8001
                IP = strStartIP.split(':');
                address_v6 start = address_v6::from_string(strStartIP.remove(':', 0).toUtf8().data());
                IP = strEndIP.split(':');
                address_v6 last = address_v6::from_string(strEndIP.remove(':', 0).toUtf8().data());
                // Apply to bittorrent session
                filter.add_rule(start, last, ip_filter::blocked);
              }
          }catch(exception){
                qDebug("Bad line in filter file, avoided crash...");
          }
        }
        file.close();
      }
    }
    
    // Parser for PeerGuardian ip filter in p2p format
    void parseP2PFilterFile(QString filePath) {
      const QRegExp is_ipv4(QString::fromUtf8("^(([0-1]?[0-9]?[0-9])|(2[0-4][0-9])|(25[0-5]))(\\.(([0-1]?[0-9]?[0-9])|(2[0-4][0-9])|(25[0-5]))){3}$"), Qt::CaseInsensitive, QRegExp::RegExp);
      QFile file(filePath);
      QStringList IP;
      if (file.exists()){
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
          QMessageBox::critical(0, tr("I/O Error", "Input/Output Error"), tr("Couldn't open %1 in read mode.").arg(filePath));
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
          // Line is not commented
          QList<QByteArray> partsList = line.split(':');
          if(partsList.size() < 2){
            qDebug("p2p file: line %d is malformed.", nbLine);
            continue;
          }
          // Get IP range
          QList<QByteArray> IPs = partsList.last().split('-');
          if(IPs.size() != 2) {
            qDebug("p2p file: line %d is malformed.", nbLine);
            continue;
          }
          QString strStartIP = IPs.at(0).trimmed();
          QString strEndIP = IPs.at(1).trimmed();
          // Check IPs format (IPv4 only)
          if(strStartIP.contains(is_ipv4) && strEndIP.contains(is_ipv4)) {
            // IPv4
            IP = strStartIP.split('.');
            address_v4 start((IP.at(0).toInt() << 24) + (IP.at(1).toInt() << 16) + (IP.at(2).toInt() << 8) + IP.at(3).toInt());
            IP = strEndIP.split('.');
            address_v4 last((IP.at(0).toInt() << 24) + (IP.at(1).toInt() << 16) + (IP.at(2).toInt() << 8) + IP.at(3).toInt());
            // Apply to bittorrent session
            filter.add_rule(start, last, ip_filter::blocked);
          } else {
              qDebug("p2p file: line %d is malformed.", nbLine);
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
          QMessageBox::critical(0, tr("I/O Error", "Input/Output Error"), tr("Couldn't open %1 in read mode.").arg(filePath));
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
          QMessageBox::critical(0, tr("I/O Error", "Input/Output Error"), tr("%1 is not a valid PeerGuardian P2B file.").arg(filePath));
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
              QMessageBox::critical(0, tr("I/O Error", "Input/Output Error"), tr("%1 is not a valid PeerGuardian P2B file.").arg(filePath));
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
            QMessageBox::critical(0, tr("I/O Error", "Input/Output Error"), tr("%1 is not a valid PeerGuardian P2B file.").arg(filePath));
            return;
          }
          namecount=ntohl(namecount);
          // Reading names although, we don't really care about them
          for(unsigned int i=0; i<namecount; i++) {
            string name;
            if(!getlineInStream(stream, name, '\0')) {
              QMessageBox::critical(0, tr("I/O Error", "Input/Output Error"), tr("%1 is not a valid PeerGuardian P2B file.").arg(filePath));
              return;
            }
            if(abort) return;
          }
          // Reading the ranges
          unsigned int rangecount;
          if(!stream.readRawData((char*)&rangecount, sizeof(rangecount))) {
            QMessageBox::critical(0, tr("I/O Error", "Input/Output Error"), tr("%1 is not a valid PeerGuardian P2B file.").arg(filePath));
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
              QMessageBox::critical(0, tr("I/O Error", "Input/Output Error"), tr("%1 is not a valid PeerGuardian P2B file.").arg(filePath));
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
          QMessageBox::critical(0, tr("I/O Error", "Input/Output Error"), tr("%1 is not a valid PeerGuardian P2B file.").arg(filePath));
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

};

#endif
