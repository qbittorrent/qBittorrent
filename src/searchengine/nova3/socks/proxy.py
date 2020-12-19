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

class Proxy:

    PROXY_TYPE_SOCKS4 = 1
    PROXY_TYPE_SOCKS5 = 2
    PROXY_TYPE_HTTP = 3

    default = False
    default_proxytype = None
    default_address = None
    default_port = None
    default_reverse_dns = None
    default_username = None
    default_password = None

    def __init__(self, proxytype, addr, port, rdns, username, password):
        self.proxytype = proxytype
        self.address = addr
        self.port = port
        self.reverse_dns = rdns
        self.username = username
        self.password = password

    @staticmethod
    def init_unconfigured():
        return Proxy(None, None, None, None, None, None)

    @classmethod
    def set_defaults(cls, proxytype=None, addr=None, port=None, rdns=None, username=None, password=None):
        cls.default = True
        if proxytype:
            cls.default_proxytype = proxytype
        if addr:
            cls.default_address = addr
        if port:
            cls.default_port = port
        if rdns:
            cls.default_reverse_dns = rdns
        if username:
            cls.default_username = username
        if password:
            cls.default_password = password

    @classmethod
    def init_default(cls):
        return Proxy(
            cls.default_proxytype,
            cls.default_address,
            cls.default_port,
            cls.default_reverse_dns,
            cls.default_username,
            cls.default_password
        )
