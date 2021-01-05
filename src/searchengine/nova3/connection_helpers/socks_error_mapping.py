#VERSION: 1.00

# Author:
#  Georgi KANCHEV (zkscpqm@daum.net)
# Licence: BSD

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#    * Redistributions of source code must retain the above copyright notice,
#      this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#    * Neither the name of the author nor the names of its contributors may be
#      used to endorse or promote products derived from this software without
#      specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

class BaseStatus:
    SUCCESS = "Success"
    UNKNOWN = "Unknown error"

    @classmethod
    def map(cls, status_code):
        raise NotImplementedError()


class GeneralStatus(BaseStatus):
    CONN_CLOSED = "Connection closed unexpectedly"
    BAD_DATA    = "Invalid data"
    NO_CONN     = "Not connected"
    NOT_AVAIL   = "Not available"
    BAD_PROX    = "Bad proxy type"
    BAD_INPUT   = "Bad input"

    @classmethod
    def map(cls, status_code):
        # Only use if the status code is unknown, else use the property directly
        _map = {
            0: cls.SUCCESS,
            1: cls.CONN_CLOSED,
            2: cls.BAD_DATA,
            3: cls.NO_CONN,
            4: cls.NOT_AVAIL,
            5: cls.BAD_PROX,
            6: cls.BAD_INPUT,
            7: cls.UNKNOWN
        }
        return _map.get(status_code)


class Socks5Status(BaseStatus):
    SRV_FAIL         = "General SOCKS server failure"
    FORBID_CONN      = "Connection ot allowed by ruleset"
    NETW_UNREACH     = "Network unreachable"
    HOST_UNREACH     = "Host unreachable"
    CONN_REFUSED     = "Connection refused"
    TTL_EXPIRED      = "TTL expired"
    UNSUPP_COMND     = "Command not supported"
    UNSUPP_ADDR_TYPE = "Address type not supported"

    @classmethod
    def map(cls, status_code):
        # Only use if the status code is unknown, else use the property directly
        _map = {
            0: cls.SUCCESS,
            1: cls.SRV_FAIL,
            2: cls.FORBID_CONN,
            3: cls.NETW_UNREACH,
            4: cls.HOST_UNREACH,
            5: cls.CONN_REFUSED,
            6: cls.TTL_EXPIRED,
            7: cls.UNSUPP_COMND,
            8: cls.UNSUPP_ADDR_TYPE,
            9: cls.UNKNOWN
        }
        return _map.get(status_code)


class Socks5AuthStatus(BaseStatus):
    AUTH_REQ    = "Authentication is required"
    AUTH_REJECT = "All offered authentication methods were rejected"
    BAD_USER_PW = "Unknown username or invalid password"

    @classmethod
    def map(cls, status_code):
        # Only use if the status code is unknown, else use the property directly
        _map = {
            0: cls.SUCCESS,
            1: cls.AUTH_REQ,
            2: cls.AUTH_REJECT,
            3: cls.BAD_USER_PW,
            4: cls.UNKNOWN
        }
        return _map.get(status_code)


class Socks4Status(BaseStatus):
    REQ_REJECT     = "Request rejected or failed"
    NOCONN_IDENTD  = "Request rejected because SOCKS server cannot connect to identd on the client"
    NOCONN_DIFF_ID = "Request rejected because the client program and identd report different user-ids"

    @classmethod
    def map(cls, status_code):
        # Only use if the status code is unknown, else use the property directly
        _map = {
            0: cls.SUCCESS,
            1: cls.REQ_REJECT,
            2: cls.NOCONN_IDENTD,
            3: cls.NOCONN_DIFF_ID,
            4: cls.UNKNOWN
        }
        return _map.get(status_code)


class Status(GeneralStatus, Socks5Status, Socks5AuthStatus, Socks4Status):
    MAP = None
