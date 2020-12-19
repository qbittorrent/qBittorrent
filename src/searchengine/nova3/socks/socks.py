"""SocksiPy - Python SOCKS module.
Version 1.02

Copyright 2006 Dan-Haim. All rights reserved.
Various fixes by Christophe DUMEZ <chris@qbittorrent.org> - 2010
Refactoring and optimization by Georgi KANCHEV <zkscpqm@daum.net> - 2010

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of Dan Haim nor the names of his contributors may be used
   to endorse or promote products derived from this software without specific
   prior written permission.

THIS SOFTWARE IS PROVIDED BY DAN HAIM "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL DAN HAIM OR HIS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMANGE.


This module provides a standard socket-like interface for Python
for tunneling connections through SOCKS proxies.

"""
from socket import socket, AF_INET, SOCK_STREAM, inet_aton, error as socket_error, gethostbyname, inet_ntoa
import struct

from socks.proxy import Proxy
from socks.socks_error_mapping import GeneralStatus, Socks5AuthStatus, Socks5Status, Socks4Status


class ProxyError(Exception):

    _error_map = None

    def __init__(self, code=None, status=None):
        if code and not status:
            _value = code
        elif status and not code:
            _value = status
        elif status and code:
            _value = (status, code)
        else:
            _value = None

        self.value = _value

    def __str__(self):
        return repr(self.value)


class GeneralProxyError(ProxyError):
    pass


class Socks5AuthError(ProxyError):
    pass


class Socks5Error(ProxyError):
    pass


class Socks4Error(ProxyError):
    pass


class HTTPError(ProxyError):
    pass


def set_default_proxy(proxytype=None, addr=None, port=None, rdns=True, username=None, password=None):
    """
    set_default_proxy(proxytype, addr[, port[, rdns[, username[, password]]]])
    Sets a default proxy which all further socksocket objects will use,
    unless explicitly changed.
    """
    Proxy.set_defaults(proxytype, addr, port, rdns, username, password)

class SockSocket(socket):
    """SockSocket([family[, type[, proto]]]) -> socket object

    Open a SOCKS enabled socket. The parameters are the same as
    those of the standard socket init. In order for SOCKS to work,
    you must specify family=AF_INET, type=SOCK_STREAM and proto=0.
    """
    SOCKS_DEFAULT_PORT = 1080
    HTTP_DEFAULT_PORT = 8080
    ALLOWED_HTTP_PROTOCOLS = ("HTTP/1.0", "HTTP/1.1")

    def __init__(self, family=AF_INET, type=SOCK_STREAM, proto=0, _sock=None):
        socket.__init__(self, family, type, proto, _sock)
        self.__proxy = Proxy.init_default() if Proxy.default else Proxy.init_unconfigured()
        self.__proxysockname = None
        self.__proxypeername = None

    def __recvall(self, bytes):
        """__recvall(bytes) -> data
        Receive EXACTLY the number of bytes requested from the socket.
        Blocks until the required number of bytes have been received.
        """
        data = b""
        while len(data) < bytes:
            _data_stream = self.recv(bytes - len(data))
            if not _data_stream:
                raise GeneralProxyError(GeneralStatus.CONN_CLOSED)
            data += _data_stream
        return data

    def setproxy(self, proxytype=None, addr=None, port=None, rdns=True, username=None, password=None):
        """setproxy(proxytype, addr[, port[, rdns[, username[, password]]]])
        Sets the proxy to be used.
        proxytype - The type of the proxy to be used. Three types
                are supported: PROXY_TYPE_SOCKS4 (including socks4a),
                PROXY_TYPE_SOCKS5 and PROXY_TYPE_HTTP
        addr -      The address of the server (IP or DNS).
        port -      The port of the server. Defaults to 1080 for SOCKS
                servers and 8080 for HTTP proxy servers.
        rdns -      Should DNS queries be preformed on the remote side
                (rather than the local side). The default is True.
                Note: This has no effect with SOCKS4 servers.
        username -  Username to authenticate with to the server.
                The default is no authentication.
        password -  Password to authenticate with to the server.
                Only relevant when username is also provided.
        """
        self.__proxy = Proxy(proxytype, addr, port, rdns, username, password)

    @property
    def proxy(self):
        return self.__proxy

    def _get_chosen_auth(self):
        # First we'll send the authentication packages we support.
        if self.proxy.username and self.proxy.password:
            # The username/password details were supplied to the
            # setproxy method so we support the USERNAME/PASSWORD
            # authentication (in addition to the standard none).
            self.sendall(b"\x05\x02\x00\x02")
        else:
            # No username/password were entered, therefore we
            # only support connections with no authentication.
            self.sendall(b"\x05\x01\x00")
        # We'll receive the server's response to determine which
        # method was selected
        return self.__recvall(2)

    def _check_auth_method(self, auth_method):
        if auth_method[0] != "\x05":
            self.close()
            raise GeneralProxyError(1, GeneralStatus.BAD_DATA)
        # Check the chosen authentication method
        if auth_method[1] == b"\x00":
            # No authentication is required
            pass
        elif auth_method[1] == b"\x02":
            # Okay, we need to perform a basic username/password
            # authentication.
            authstat = self._authenticate()
            if authstat[0] != b"\x01":
                # Bad response
                self.close()
                raise GeneralProxyError(1, GeneralStatus.BAD_DATA)
            if authstat[1] != b"\x00":
                # Authentication failed
                self.close()
                raise Socks5AuthError(3, Socks5AuthStatus.BAD_USER_PW)
            # Authentication succeeded
        else:
            # Reaching here is always bad
            self.close()
            if auth_method[1] == b"\xFF":
                raise Socks5AuthError(2, Socks5AuthStatus.AUTH_REJECT)
            else:
                raise GeneralProxyError(1, GeneralStatus.BAD_DATA)

    def _authenticate(self):

        auth_str = "\x01" + chr(len(self.proxy.username)) + self.proxy.username \
        + chr(len(self.proxy.password)) + self.proxy.password

        self.sendall(auth_str.encode("utf-8"))
        return self.__recvall(2)

    def _build_socks5_request(self, destaddr, destport):
        request = b"\x05\x01\x00"
        # If the given destination address is an IP address, we'll
        # use the IPv4 address request even if remote resolving was specified.
        try:
            ipaddr = inet_aton(destaddr)
            _req_addon = b"\x01" + ipaddr
            request += _req_addon
        except socket_error:
            # Well it's not an IP number,  so it's probably a DNS name.
            if self.proxy.reverse_dns:
                # Resolve remotely
                ipaddr = None
                _req_addon = b"\x03" + chr(len(destaddr)).encode('utf-8') + destaddr.encode('utf-8')
                request += _req_addon
            else:
                # Resolve locally
                ipaddr = inet_aton(gethostbyname(destaddr))
                _req_addon = b"\x01" + ipaddr
                request += _req_addon
        request += struct.pack(">H", destport)
        return request, ipaddr

    def _establish_socks5_connection(self, request):

        self.sendall(request)
        resp = self.__recvall(4)

        if resp[0] != b"\x05":
            self.close()
            raise GeneralProxyError(1, GeneralStatus.BAD_DATA)

        elif resp[1] != b"\x00":
            # Connection failed
            self.close()
            _code = resp[1]
            if _code <= 8:
                raise Socks5Error(_code, GeneralStatus.map(_code) or Socks5Status.map(_code))
            else:
                raise Socks5Error(9, Socks5Status.UNKNOWN)
        # Get the bound address/port
        elif resp[3] == b"\x01":
            return self._get_bound_address_and_port()

        elif resp[3] == b"\x03":
            resp += self.recv(1)
            return self._get_bound_address_and_port(addr_len=resp[4])

        self.close()
        raise GeneralProxyError(1, GeneralStatus.BAD_DATA)

    def _get_bound_address_and_port(self, addr_len=4):
        boundaddr = self.__recvall(addr_len)
        boundport = struct.unpack(">H", self.__recvall(2))[0]
        return boundaddr, boundport

    def __negotiatesocks5(self, destaddr, destport):
        """__negotiatesocks5(self,destaddr,destport)
        Negotiates a connection through a SOCKS5 server.
        """
        chosenauth = self._get_chosen_auth()
        self._check_auth_method(chosenauth)
        # Now we can request the actual connection
        req, ipaddr = self._build_socks5_request(destaddr, destport)
        boundaddr, boundport = self._establish_socks5_connection(req)
        self.__proxysockname = (boundaddr, boundport)
        self.__proxypeername = (inet_ntoa(ipaddr) if ipaddr else destaddr, destport)

    @property
    def getproxysockname(self):
        """getsockname -> address info
        Returns the bound IP address and port number at the proxy.
        """
        return self.__proxysockname

    def getpeername(self):
        """getpeername() -> address info
        Returns the IP address and port number of the destination
        machine (note: getproxypeername returns the proxy)
        """
        return socket.getpeername(self)

    @property
    def getproxypeername(self):
        """getproxypeername -> address info
        Returns the IP and port number of the proxy.
        """
        return self.__proxypeername

    def _resolve_ip_address_socks4(self, address):
        try:
            return inet_aton(address)
        except socket_error:
            # It's a DNS name. Check where it should be resolved.
            return inet_aton(gethostbyname(address)) \
                if self.proxy.reverse_dns \
                else b"\x00\x00\x00\x01"

    def _build_socks4_request(self, destaddr, destport, ipaddr):
        req = b"\x04\x01" + struct.pack(">H", destport) + ipaddr
        # The username parameter is considered userid for SOCKS4
        if self.proxy.username:
            req += self.proxy.username
        req += b"\x00"
        # DNS name if remote resolving is required
        # NOTE: This is actually an extension to the SOCKS4 protocol
        # called SOCKS4A and may not be supported in all cases.
        if self.proxy.reverse_dns:
            req = req + destaddr.encode() + b"\x00"
        return req

    def _establish_socks4_connection(self, request):
        self.sendall(request)
        # Get the response from the server
        resp = self.__recvall(8)
        if resp[0] != b"\x00":
            # Bad data
            self.close()
            raise GeneralProxyError(1, GeneralStatus.BAD_DATA)
        if resp[1] != "\x5A":
            # Server returned an error
            self.close()
            _code = resp[1]
            if _code in (91, 92, 93):
                self.close()
                raise Socks4Error(_code, Socks4Status.map(_code - 90))
            else:
                raise Socks4Error(94, Socks4Status.UNKNOWN)
        # Get the bound address/port
        boundaddr = inet_ntoa(resp[4:])
        boundport = struct.unpack(">H", resp[2:4])[0]
        return boundaddr, boundport

    def _interpret_http_response(self, http_response_bytes):
        response_line_one = http_response_bytes.splitlines()[0]
        statusline = response_line_one.split(b" ", 2)
        protocol = statusline[0]
        try:
            http_status_code = int(statusline[1])
            http_error = statusline[2] if http_status_code != 200 else None
            return protocol, http_status_code, http_error
        except ValueError:
            raise GeneralProxyError(9, GeneralStatus.UNKNOWN)

    def _try_http_connection(self, connection_string):
        self.sendall(connection_string)
        # We read the response until we get the string "\r\n\r\n"
        resp = self.recv(1)
        while resp.find(b"\r\n\r\n") == -1:
            resp += self.recv(1)
        # We just need the first line to check if the connection
        # was successful
        protocol, http_status_code, http_error = self._interpret_http_response(resp)
        if protocol not in self.ALLOWED_HTTP_PROTOCOLS:
            self.close()
            raise GeneralProxyError(1, GeneralStatus.BAD_DATA)

        if http_status_code != 200:
            self.close()
            raise HTTPError(http_status_code, http_error)


    def __negotiatesocks4(self, destaddr, destport):
        """__negotiatesocks4(self,destaddr,destport)
        Negotiates a connection through a SOCKS4 server.
        """
        # Check if the destination address provided is an IP address

        ipaddr = self._resolve_ip_address_socks4(destaddr)
        # Construct the request packet
        req = self._build_socks4_request(destaddr, destport, ipaddr)

        boundaddr, boundport = self._establish_socks4_connection(req)
        self.__proxysockname = (boundaddr, boundport)
        self.__proxypeername = (inet_ntoa(ipaddr) if self.proxy.reverse_dns else destaddr, destport)


    def __negotiatehttp(self, destaddr, destport):
        """__negotiatehttp(self,destaddr,destport)
        Negotiates a connection through an HTTP server.
        """
        # If we need to resolve locally, we do this now
        addr = gethostbyname(destaddr) if not self.proxy.reverse_dns else destaddr

        req = "CONNECT " \
              + addr \
              + ":" \
              + str(destport) \
              + " HTTP/1.1\r\n" \
              + "Host: " \
              + destaddr \
              + "\r\n\r\n"

        self._try_http_connection(connection_string=req)
        self.__proxysockname = ("0.0.0.0", 0)
        self.__proxypeername = (addr, destport)

    def _connect(self, destaddr, destport):
        if self.proxy.proxytype == self.proxy.PROXY_TYPE_SOCKS5:
            socket.connect(self, (self.proxy.address, self.proxy.port or self.SOCKS_DEFAULT_PORT))
            self.__negotiatesocks5(destaddr, destport)
        elif self.proxy.proxytype == self.proxy.PROXY_TYPE_SOCKS4:
            socket.connect(self, (self.proxy.address, self.proxy.port or self.SOCKS_DEFAULT_PORT))
            self.__negotiatesocks4(destaddr, destport)
        elif self.proxy.proxytype == self.proxy.PROXY_TYPE_HTTP:
            socket.connect(self, (self.proxy.address, self.proxy.port or self.HTTP_DEFAULT_PORT))
            self.__negotiatehttp(destaddr, destport)
        elif not self.proxy.proxytype:
            socket.connect(self, (destaddr, destport))
        else:
            raise GeneralProxyError(4, GeneralStatus.NOT_AVAIL)

    def connect(self, destpair):
        """connect(self,despair)
        Connects to the specified destination through a proxy.
        destpar - A tuple of the IP/DNS address and the port number.
        (identical to socket's connect).
        To select the proxy server use setproxy().
        """
        try:
            self._connect(*destpair)
        except:
            raise GeneralProxyError(5, GeneralStatus.BAD_PROX)
