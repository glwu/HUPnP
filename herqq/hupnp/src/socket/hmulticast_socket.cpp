/*
 *  Copyright (C) 2010, 2011 Tuomo Penttinen, all rights reserved.
 *
 *  Author: Tuomo Penttinen <tp@herqq.org>
 *
 *  This file is part of Herqq UPnP (HUPnP) library.
 *
 *  Herqq UPnP is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Herqq UPnP is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with Herqq UPnP. If not, see <http://www.gnu.org/licenses/>.
 */

#include "hmulticast_socket.h"
#include "../general/hlogger_p.h"

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#elif defined(Q_OS_SYMBIAN) || defined(Q_OS_FREEBSD)
#include <netinet/in.h>
#include <sys/socket.h>
#elif defined(Q_OS_ANDROID)
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#else
#include <arpa/inet.h>
#endif

#include <QtNetwork/QNetworkProxy>
#include <QNetworkInterface>

namespace Herqq
{

namespace Upnp
{

//
//
//
class HMulticastSocketPrivate
{
};

HMulticastSocket::HMulticastSocket(QObject* parent) :
    QUdpSocket(parent), h_ptr(new HMulticastSocketPrivate())
{
    setProxy(QNetworkProxy::NoProxy);
}

HMulticastSocket::~HMulticastSocket()
{
    delete h_ptr;
}

HMulticastSocket::HMulticastSocket(
    HMulticastSocketPrivate& dd, QObject* parent) :
        QUdpSocket(parent), h_ptr(&dd)
{
    setProxy(QNetworkProxy::NoProxy);
}

bool HMulticastSocket::bind(quint16 port)
{
    //return QUdpSocket::bind( port, QUdpSocket::ReuseAddressHint | QUdpSocket::ShareAddress);
	return QUdpSocket::bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
}

bool HMulticastSocket::joinMulticastGroup(const QHostAddress& groupAddress)
{
	foreach (const QNetworkInterface& iface, QNetworkInterface::allInterfaces())
    {
        if (iface.flags() & QNetworkInterface::IsUp && (iface.flags() & QNetworkInterface::CanMulticast) &&
                !(iface.flags() & QNetworkInterface::IsLoopBack))
        {
           QUdpSocket::joinMulticastGroup(groupAddress, iface);
        }
    }
    //return  QUdpSocket::joinMulticastGroup(groupAddress);
}

bool HMulticastSocket::joinMulticastGroup(
    const QHostAddress& groupAddress, const QHostAddress& localAddress)
{
    HLOG(H_AT, H_FUN);

    if (groupAddress.protocol() != QAbstractSocket::IPv4Protocol)
    {
        // TODO: IPv6 multicast
        HLOG_WARN("IPv6 is not supported.");
        setSocketError(QAbstractSocket::UnknownSocketError);
        return false;
    }

    if (proxy().type() != QNetworkProxy::NoProxy)
    {
        // TODO: Proxied multicast
        HLOG_WARN("Proxied multicast is not supported.");
        setSocketError(QAbstractSocket::UnknownSocketError);
        return false;
    }

    if (socketDescriptor() == -1)
    {
        HLOG_WARN("Socket descriptor is invalid.");
        setSocketError(QAbstractSocket::UnknownSocketError);
        return false;
    }

    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(ip_mreq));

    mreq.imr_multiaddr.s_addr = inet_addr(groupAddress.toString().toUtf8());

    if (!localAddress.isNull())
    {
        mreq.imr_interface.s_addr = inet_addr(localAddress.toString().toUtf8());
    }
    else
    {
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    }

    if (setsockopt(
            socketDescriptor(),
            IPPROTO_IP,
            IP_ADD_MEMBERSHIP,
            reinterpret_cast<char*>(&mreq),
            sizeof(mreq)) < 0)
    {
        HLOG_WARN(QString(
            "Failed to join the group [%1] using local address: [%2].").arg(
                groupAddress.toString(), localAddress.toString()));

        setSocketError(QAbstractSocket::UnknownSocketError);
        return false;
    }

    return true;
}


bool HMulticastSocket::joinMulticastGroup(const QHostAddress& groupAddress, quint16 port)
{
    HLOG(H_AT, H_FUN);

    if (groupAddress.protocol() != QAbstractSocket::IPv4Protocol)
    {
        // TODO: IPv6 multicast
        HLOG_WARN("IPv6 is not supported.");
        setSocketError(QAbstractSocket::UnknownSocketError);
        return false;
    }

    if (proxy().type() != QNetworkProxy::NoProxy)
    {
        // TODO: Proxied multicast
        HLOG_WARN("Proxied multicast is not supported.");
        setSocketError(QAbstractSocket::UnknownSocketError);
        return false;
    }

    if (socketDescriptor() == -1)
    {
		int socketid;

        #ifdef Q_OS_WIN
        WSAData ws;
        if (WSAStartup(MAKEWORD(2,2),&ws))
        {
			HLOG_WARN("WSAStartup failed.");
			return false;
        }
        #endif

        if ((socketid = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
        {
            HLOG_WARN("Socket descriptor is invalid.");
			setSocketError(QAbstractSocket::UnknownSocketError);
            return false;
        }
        setSocketDescriptor(socketid);
    }

    struct ip_mreq mreq;
    sockaddr_in m_sockLocalAddress;
    sockaddr_in m_sockReceiveAddress;
    memset(&mreq, 0, sizeof(ip_mreq));

	m_sockLocalAddress.sin_family = AF_INET;
    m_sockLocalAddress.sin_port = htons(port);
    m_sockLocalAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind (socketDescriptor(),
        (struct sockaddr *) &m_sockLocalAddress,
        sizeof (m_sockLocalAddress)) == -1)
    {
		HLOG_WARN("Binding socket failed!");
        return false;
    }

    mreq.imr_multiaddr.s_addr = inet_addr(groupAddress.toString().toUtf8());
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(socketDescriptor(),
        IPPROTO_IP,
        IP_ADD_MEMBERSHIP,
        reinterpret_cast<char*>(&mreq),
        sizeof (mreq)) == -1)
    {
		HLOG_WARN("setsockopt failed!");
        return false;
    }

    return true;
}


bool HMulticastSocket::leaveMulticastGroup(const QHostAddress& groupAddress)
{
    return leaveMulticastGroup(groupAddress, QHostAddress());
}

bool HMulticastSocket::leaveMulticastGroup(
    const QHostAddress& groupAddress, const QHostAddress& localAddress)
{
    HLOG(H_AT, H_FUN);

    if (groupAddress.protocol() != QAbstractSocket::IPv4Protocol)
    {
        // TODO: IPv6 multicast
        HLOG_WARN("IPv6 is not supported.");
        setSocketError(QAbstractSocket::UnknownSocketError);
        return false;
    }

    if (proxy().type() != QNetworkProxy::NoProxy)
    {
        // TODO: Proxied multicast
        HLOG_WARN("Proxied multicast is not supported.");
        setSocketError(QAbstractSocket::UnknownSocketError);
        return false;
    }

    if (socketDescriptor() == -1)
    {
        HLOG_WARN("Socket descriptor is invalid.");
        setSocketError(QAbstractSocket::UnknownSocketError);
        return false;
    }

    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(ip_mreq));

    mreq.imr_multiaddr.s_addr = inet_addr(groupAddress.toString().toUtf8());
    if (localAddress.isNull())
    {
        mreq.imr_interface.s_addr = htons(INADDR_ANY);
    }
    else
    {
        mreq.imr_interface.s_addr = inet_addr(localAddress.toString().toUtf8());
    }

    if (setsockopt(
            socketDescriptor(),
            IPPROTO_IP,
            IP_DROP_MEMBERSHIP,
            reinterpret_cast<char*>(&mreq),
            sizeof(mreq)) < 0)
    {
        HLOG_WARN("Failed to leave the specified group.");
        setSocketError(QAbstractSocket::UnknownSocketError);
        return false;
    }

    return true;
}

bool HMulticastSocket::setMulticastTtl(quint8 ttl)
{
    HLOG(H_AT, H_FUN);

    if (socketDescriptor() == -1)
    {
        HLOG_WARN("Socket descriptor is invalid.");
        setSocketError(QAbstractSocket::UnknownSocketError);
        return false;
    }

    if (setsockopt(
            socketDescriptor(),
            IPPROTO_IP,
            IP_MULTICAST_TTL,
            reinterpret_cast<char*>(&ttl),
            sizeof(ttl)) < 0)
    {
        HLOG_WARN("Could not set multicast TTL to the specified value.");
        setSocketError(QAbstractSocket::UnknownSocketError);
        return false;
    }

    return true;
}

}
}
