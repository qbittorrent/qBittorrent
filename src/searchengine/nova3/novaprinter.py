#VERSION: 1.49

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

import re
from collections.abc import Mapping
from typing import Any

# TODO: enable this when using Python >= 3.8
#SearchResults = TypedDict('SearchResults', {
#    'link': str,
#    'name': str,
#    'size': str,
#    'seeds': int,
#    'leech': int,
#    'engine_url': str,
#    'desc_link': str,  # Optional
#    'pub_date': int  # Optional
#})
SearchResults = Mapping[str, Any]


def prettyPrinter(dictionary: SearchResults) -> None:
    outtext = "|".join((
        dictionary["link"],
        dictionary["name"].replace("|", " "),
        str(anySizeToBytes(dictionary['size'])),
        str(dictionary["seeds"]),
        str(dictionary["leech"]),
        dictionary["engine_url"],
        dictionary.get("desc_link", ""),  # Optional
        str(dictionary.get("pub_date", -1))  # Optional
    ))

    # fd 1 is stdout
    with open(1, 'w', encoding='utf-8', closefd=False) as utf8stdout:
        print(outtext, file=utf8stdout)


sizeUnitRegex: re.Pattern[str] = re.compile(r"^(?P<size>\d*\.?\d+) *(?P<unit>[a-z]+)?", re.IGNORECASE)


def anySizeToBytes(size_string: str) -> int:
    """
    Convert a string like '1 KB' to '1024' (bytes)
    """

    match = sizeUnitRegex.match(size_string.strip())
    if match is None:
        return -1

    size = float(match.group('size'))  # need to match decimals
    unit = match.group('unit')

    if unit is not None:
        units_exponents = {'T': 40, 'G': 30, 'M': 20, 'K': 10}
        exponent = units_exponents.get(unit[0].upper(), 0)
        size *= 2**exponent

    return round(size)
