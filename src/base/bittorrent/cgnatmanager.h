/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  qBittorrent contributors
 *
 * CGNAT Manager — built-in STUN-based port discovery and NAT keepalive.
 * Eliminates the need for UPnP/NAT-PMP on carrier-grade NAT.
 *
 * Architecture:
 *   1. Binds UDP socket to the same port as the TCP listen (protocols can coexist)
 *   2. Sends periodic STUN Binding Requests to discover the CGNAT-mapped port
 *   3. Emits signal when the mapped port is discovered/changed
 *   4. The NAT mapping is maintained by the STUN traffic itself (UDP keepalive)
 *      and by normal peer tracker connections (TCP keepalive via outgoing_port)
 */

#pragma once

#include <QObject>
#include <QTimer>
#include <QHostAddress>
#include <QHostInfo>
#include <QUdpSocket>
#include <QProcess>

class CGNATManager : public QObject
{
    Q_OBJECT

public:
    explicit CGNATManager(QObject *parent = nullptr);
    ~CGNATManager() override;

    void setListenAddress(const QHostAddress &addr, quint16 port);
    void setBackendPort(quint16 port);
    void setStunServer(const QString &host, quint16 port = 3478);
    void setInterval(int msecs);

    bool isEnabled() const;
    void setEnabled(bool enabled);

    int lastMappedPort() const;

signals:
    void mappedPortDiscovered(int port);
    void tcpPortDiscovered(int port);

private slots:
    void sendStunRequest();
    void readStunResponse();

private:
    static quint16 parseXorMappedAddress(const QByteArray &magicBE, const QByteArray &attrValue);

    void resolveStunHost();
    void startProxyProcess();

    QUdpSocket *m_socket = nullptr;
    QTimer *m_timer = nullptr;
    QProcess *m_proxyProcess = nullptr;
    QHostAddress m_listenAddr;
    quint16 m_listenPort = 0;
    quint16 m_backendPort = 0;  // libtorrent listen port when proxy active
    QString m_stunHost;
    quint16 m_stunPort = 3478;
    QHostAddress m_resolvedStunAddr;
    int m_interval = 30000;  // 30 seconds
    bool m_enabled = false;
    int m_lastMappedPort = 0;
    int m_dnsLookupId = -1;

    // STUN magic cookie
    static constexpr quint32 STUN_MAGIC = 0x2112A442;

    // STUN message types
    static constexpr quint16 BINDING_REQUEST  = 0x0001;
    static constexpr quint16 BINDING_RESPONSE = 0x0101;
    static constexpr quint16 BINDING_ERROR    = 0x0111;

    // STUN attributes
    static constexpr quint16 ATTR_MAPPED_ADDRESS     = 0x0001;
    static constexpr quint16 ATTR_XOR_MAPPED_ADDRESS = 0x0020;
};
