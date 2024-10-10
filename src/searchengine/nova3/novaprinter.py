#VERSION: 1.52

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
from typing import TypedDict, Union

SearchResults = TypedDict('SearchResults', {
    'link': str,
    'name': str,
    'size': Union[float, int, str],  # TODO: use `float | int | str` when using Python >= 3.10
    'seeds': int,
    'leech': int,
    'engine_url': str,
    'desc_link': str,  # Optional  # TODO: use `NotRequired[str]` when using Python >= 3.11
    'pub_date': int  # Optional  # TODO: use `NotRequired[int]` when using Python >= 3.11
})


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


# TODO: use `float | int | str` when using Python >= 3.10
def anySizeToBytes(size_string: Union[float, int, str]) -> int:
    """
    Convert a string like '1 KB' to '1024' (bytes)

    The canonical type for `size_string` is `str`. However numeric types are also accepted in order to
    accommodate poorly written plugins.
    """

    if isinstance(size_string, int):
        return size_string
    if isinstance(size_string, float):
        return round(size_string)

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
