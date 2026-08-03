/*
 * CGNAT Manager implementation — see header for architecture overview.
 */

#include "cgnatmanager.h"

#include <QNetworkDatagram>
#include <QRandomGenerator>
#include <QHostInfo>
#include <QtEndian>
#include <QCoreApplication>

CGNATManager::CGNATManager(QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &CGNATManager::sendStunRequest);
    connect(m_socket, &QUdpSocket::readyRead, this, &CGNATManager::readStunResponse);
}

CGNATManager::~CGNATManager()
{
    m_timer->stop();
    if (m_socket->state() == QAbstractSocket::BoundState)
        m_socket->close();
}

void CGNATManager::setListenAddress(const QHostAddress &addr, quint16 port)
{
    m_listenAddr = addr;
    m_listenPort = port;
}

void CGNATManager::setStunServer(const QString &host, quint16 port)
{
    m_stunHost = host;
    m_stunPort = port;

    if (host.isEmpty())
    {
        m_resolvedStunAddr.clear();
        return;
    }

    // Resolve hostname now so we don't block on each STUN request
    const QHostInfo info = QHostInfo::fromName(host);
    if (!info.addresses().isEmpty())
    {
        for (const QHostAddress &addr : info.addresses())
        {
            if (addr.protocol() == QAbstractSocket::IPv4Protocol)
            {
                m_resolvedStunAddr = addr;
                break;
            }
        }
        if (m_resolvedStunAddr.isNull() && !info.addresses().isEmpty())
            m_resolvedStunAddr = info.addresses().first();
    }
}

void CGNATManager::setInterval(int msecs)
{
    m_interval = qMax(5000, msecs);
    if (m_enabled)
        m_timer->start(m_interval);
}

bool CGNATManager::isEnabled() const
{
    return m_enabled;
}

void CGNATManager::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;

    if (enabled)
    {
        // Bind UDP socket to the same port as the TCP listener.
        // TCP and UDP ports are independent — no conflict.
        if (m_socket->state() == QAbstractSocket::BoundState)
            m_socket->close();

        m_socket->bind(m_listenAddr, m_listenPort,
                       QAbstractSocket::ReuseAddressHint | QAbstractSocket::ShareAddress);

        if (m_socket->state() == QAbstractSocket::BoundState)
        {
            // Send first STUN request immediately
            sendStunRequest();
            // Then keep sending periodically
            m_timer->start(m_interval);
        }

        // Start TCP proxy if backend port is configured
        if (m_backendPort > 0 && m_listenPort > 0)
        {
            startProxyProcess();
        }
    }
    else
    {
        m_timer->stop();
        m_socket->close();
        m_lastMappedPort = 0;

        // Stop TCP proxy
        if (m_proxyProcess && m_proxyProcess->state() != QProcess::NotRunning)
        {
            m_proxyProcess->terminate();
            m_proxyProcess->waitForFinished(3000);
        }
    }
}

void CGNATManager::setBackendPort(quint16 port)
{
    m_backendPort = port;
}

int CGNATManager::lastMappedPort() const
{
    return m_lastMappedPort;
}

void CGNATManager::sendStunRequest()
{
    if (m_stunHost.isEmpty() || m_resolvedStunAddr.isNull())
        return;

    // Build STUN Binding Request (RFC 5389)
    // Message Type: 0x0001 (Binding Request)
    // Message Length: 0
    // Magic Cookie: 0x2112A442
    // Transaction ID: 12 random bytes

    QByteArray packet;
    packet.reserve(20);

    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << BINDING_REQUEST;      // message type
    stream << quint16(0);           // message length (no attributes)
    stream << STUN_MAGIC;           // magic cookie

    // 12-byte transaction ID
    for (int i = 0; i < 12; ++i)
        stream << quint8(QRandomGenerator::global()->bounded(256));

    m_socket->writeDatagram(packet, m_resolvedStunAddr, m_stunPort);
}

void CGNATManager::readStunResponse()
{
    while (m_socket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        const QByteArray data = datagram.data();

        if (data.size() < 20)
            continue;

        // Parse header
        QDataStream stream(data);
        stream.setByteOrder(QDataStream::BigEndian);
        quint16 msgType, msgLen;
        quint32 magic;
        stream >> msgType >> msgLen >> magic;

        if (msgType != BINDING_RESPONSE)
            continue;
        if (magic != STUN_MAGIC)
            continue;

        // Skip 12-byte transaction ID
        stream.skipRawData(12);

        // Parse attributes
        while (!stream.atEnd())
        {
            quint16 attrType, attrLen;
            stream >> attrType >> attrLen;

            QByteArray attrValue = data.mid(stream.device()->pos(), attrLen);
            stream.skipRawData(attrLen);

            // Align to 4-byte boundary
            int padding = (4 - (attrLen % 4)) % 4;
            stream.skipRawData(padding);

            if (attrType == ATTR_XOR_MAPPED_ADDRESS)
            {
                // Convert magic cookie to big-endian for XOR calculation
                const quint32 magicBE = qToBigEndian(magic);
                const QByteArray magicBytes(reinterpret_cast<const char *>(&magicBE), sizeof(magicBE));
                quint16 port = parseXorMappedAddress(magicBytes, attrValue);

                if (port > 0 && port != m_lastMappedPort)
                {
                    m_lastMappedPort = port;
                    emit mappedPortDiscovered(port);
                }
                return;  // Got what we need
            }
        }
    }
}

quint16 CGNATManager::parseXorMappedAddress(const QByteArray &magicBE, const QByteArray &attrValue)
{
    if (magicBE.size() < 4 || attrValue.size() < 8)
        return 0;

    const quint8 family = static_cast<quint8>(attrValue.at(1));
    if (family != 0x01)
        return 0;

    quint32 magic;
    memcpy(&magic, magicBE.constData(), sizeof(magic));

    quint16 xport;
    memcpy(&xport, attrValue.constData() + 2, sizeof(xport));
    xport = qFromBigEndian(xport);

    return xport ^ (magic >> 16);
}

void CGNATManager::startProxyProcess()
{
    if (m_proxyProcess && m_proxyProcess->state() != QProcess::NotRunning)
        return;

    if (!m_proxyProcess)
    {
        m_proxyProcess = new QProcess(this);
        connect(m_proxyProcess, &QProcess::readyReadStandardOutput, this, [this]()
        {
            while (m_proxyProcess->canReadLine())
            {
                const QByteArray line = m_proxyProcess->readLine().trimmed();
                if (line.startsWith("PORT="))
                {
                    bool ok = false;
                    const int port = line.mid(5).toInt(&ok);
                    if (ok && port > 0 && port <= 65535)
                        emit tcpPortDiscovered(port);
                }
            }
        });
    }

    const QString proxyPath = QCoreApplication::applicationDirPath() + QStringLiteral("/cgnat_proxy");
    m_proxyProcess->start(proxyPath,
        {QString::number(m_listenPort),
         QString::number(m_backendPort),
         QStringLiteral("http://ifconfig.io/port")});
}
