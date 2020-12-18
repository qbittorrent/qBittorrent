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
