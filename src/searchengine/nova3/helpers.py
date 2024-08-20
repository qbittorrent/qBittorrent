#VERSION: 1.48

# Author:
#  Christophe DUMEZ (chris@qbittorrent.org)

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

import gzip
import html.entities
import io
import os
import re
import socket
import socks
import sys
import tempfile
import urllib.error
import urllib.request
from collections.abc import Mapping
from datetime import date, datetime, timedelta
from typing import Any, Optional


def getBrowserUserAgent() -> str:
    """ Disguise as browser to circumvent website blocking """

    # Firefox release calendar
    # https://whattrainisitnow.com/calendar/
    # https://wiki.mozilla.org/index.php?title=Release_Management/Calendar&redirect=no

    baseDate = date(2024, 4, 16)
    baseVersion = 125

    nowDate = date.today()
    nowVersion = baseVersion + ((nowDate - baseDate).days // 30)

    return f"Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:{nowVersion}.0) Gecko/20100101 Firefox/{nowVersion}.0"


headers: dict[str, Any] = {'User-Agent': getBrowserUserAgent()}

# SOCKS5 Proxy support
if "sock_proxy" in os.environ and len(os.environ["sock_proxy"].strip()) > 0:
    proxy_str = os.environ["sock_proxy"].strip()
    m = re.match(r"^(?:(?P<username>[^:]+):(?P<password>[^@]+)@)?(?P<host>[^:]+):(?P<port>\w+)$",
                 proxy_str)
    if m is not None:
        socks.setdefaultproxy(socks.PROXY_TYPE_SOCKS5, m.group('host'),
                              int(m.group('port')), True, m.group('username'), m.group('password'))
        socket.socket = socks.socksocket  # type: ignore[misc]


def htmlentitydecode(s: str) -> str:
    # First convert alpha entities (such as &eacute;)
    # (Inspired from http://mail.python.org/pipermail/python-list/2007-June/443813.html)
    def entity2char(m: re.Match[str]) -> str:
        entity = m.group(1)
        if entity in html.entities.name2codepoint:
            return chr(html.entities.name2codepoint[entity])
        return " "  # Unknown entity: We replace with a space.
    t = re.sub('&(%s);' % '|'.join(html.entities.name2codepoint), entity2char, s)

    # Then convert numerical entities (such as &#233;)
    t = re.sub(r'&#(\d+);', lambda x: chr(int(x.group(1))), t)

    # Then convert hexa entities (such as &#x00E9;)
    return re.sub(r'&#x(\w+);', lambda x: chr(int(x.group(1), 16)), t)


def retrieve_url(url: str, custom_headers: Mapping[str, Any] = {}) -> str:
    """ Return the content of the url page as a string """

    request = urllib.request.Request(url, headers={**headers, **custom_headers})
    try:
        response = urllib.request.urlopen(request)
    except urllib.error.URLError as errno:
        print(f"Connection error: {errno.reason}", file=sys.stderr)
        return ""
    data: bytes = response.read()

    # Check if it is gzipped
    if data[:2] == b'\x1f\x8b':
        # Data is gzip encoded, decode it
        with io.BytesIO(data) as compressedStream, gzip.GzipFile(fileobj=compressedStream) as gzipper:
            data = gzipper.read()

    charset = 'utf-8'
    try:
        charset = response.getheader('Content-Type', '').split('charset=', 1)[1]
    except IndexError:
        pass

    dataStr = data.decode(charset, 'replace')
    dataStr = htmlentitydecode(dataStr)
    return dataStr


def download_file(url: str, referer: Optional[str] = None) -> str:
    """ Download file at url and write it to a file, return the path to the file and the url """

    # Download url
    request = urllib.request.Request(url, headers=headers)
    if referer is not None:
        request.add_header('referer', referer)
    response = urllib.request.urlopen(request)
    data = response.read()

    # Check if it is gzipped
    if data[:2] == b'\x1f\x8b':
        # Data is gzip encoded, decode it
        with io.BytesIO(data) as compressedStream, gzip.GzipFile(fileobj=compressedStream) as gzipper:
            data = gzipper.read()

    # Write it to a file
    fileHandle, path = tempfile.mkstemp()
    with os.fdopen(fileHandle, "wb") as file:
        file.write(data)

    # return file path
    return f"{path} {url}"


def parse_date(date_string: str, fallback_format: Optional[str] = None) -> int:
    """ Try to parse a date by detecting unambiguous formats or falling back to provided format """

    date_string = re.sub(r"\s+", " ", date_string).strip().lower()
    today = datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
    months = ["jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"]
    date_parsers = {
        r"today": lambda m: today,
        r"yesterday": lambda m: today - timedelta(days=1),
        r"last week": lambda m: today - timedelta(days=7),
        r"last month": lambda m: today - timedelta(days=30),
        r"last year": lambda m: today - timedelta(days=365),
        r"(\d+) (day)s?": lambda m: today - timedelta(days=int(m[1])),
        r"(\d+) (week)s?": lambda m: today - timedelta(days=int(m[1]) * 7),
        r"(\d+) (mo|month)s?": lambda m: today - timedelta(days=int(m[1]) * 30),
        r"(\d+) (yr|year)s?": lambda m: today - timedelta(days=int(m[1]) * 365),
        r"(\d\d\d\d)[-/.](\d\d)[-/.](\d\d) (\d\d):(\d\d)": lambda m: datetime(
            int(m[1]), int(m[2]), int(m[3]), int(m[4]), int(m[5])
        ),
        r"(\d\d\d\d)[-/.](\d\d)[-/.](\d\d)": lambda m: datetime(
            int(m[1]), int(m[2]), int(m[3])
        ),
        rf"(\d+) ({'|'.join(months)})\w*,? (\d\d\d\d)": lambda m: datetime(
            int(m[3]), int(months.index(m[2]) + 1), int(m[1])
        ),
        rf"({'|'.join(months)})\w* (\d+),? (\d\d\d\d)": lambda m: datetime(
            int(m[3]), int(months.index(m[1]) + 1), int(m[2])
        ),
    }

    for pattern, calc in date_parsers.items():
        try:
            m = re.match(pattern, date_string)
            if m:
                return int(calc(m).timestamp())
        except Exception:
            pass

    if fallback_format is not None:
        try:
            return int(datetime.strptime(date_string, fallback_format).timestamp())
        except Exception:
            pass

    return -1
