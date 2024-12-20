#!/usr/bin/env python3

# A pre-commit hook for checking items order in grid layouts
# Copyright (C) 2024  Mike Tzou (Chocobo1)
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
# In addition, as a special exception, the copyright holders give permission to
# link this program with the OpenSSL project's "OpenSSL" library (or with
# modified versions of it that use the same license as the "OpenSSL" library),
# and distribute the linked executables. You must obey the GNU General Public
# License in all respects for all of the code used other than "OpenSSL".  If you
# modify file(s), you may extend this exception to your version of the file(s),
# but you are not obligated to do so. If you do not wish to do so, delete this
# exception statement from your version.

from collections.abc import Callable, Sequence
from typing import Optional
import argparse
import re
import xml.etree.ElementTree as ElementTree
import sys


def traversePostOrder(root: ElementTree.Element, visitFunc: Callable[[ElementTree.Element], None]) -> None:
    stack = [(root, False)]

    while len(stack) > 0:
        (element, visit) = stack.pop()
        if visit:
            visitFunc(element)
        else:
            stack.append((element, True))
            stack.extend((child, False) for child in reversed(element))


def modifyElement(element: ElementTree.Element) -> None:
    def getSortKey(e: ElementTree.Element) -> tuple[int, int]:
        if e.tag == 'item':
            return (int(e.attrib['row']), int(e.attrib['column']))
        return (-1, -1)  # don't care

    if element.tag == 'layout' and element.attrib['class'] == 'QGridLayout' and len(element) > 0:
        element[:] = sorted(element, key=getSortKey)

    # workaround_2a: ElementTree will unescape `&quot;` and we need to escape it back
    if element.tag == 'string' and element.text is not None:
        element.text = element.text.replace('"', '&quot;')


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('filenames', nargs='*', help='Filenames to check')
    args = parser.parse_args(argv)

    for filename in args.filenames:
        with open(filename, 'r+') as f:
            orig = f.read()
            root = ElementTree.fromstring(orig)
            traversePostOrder(root, modifyElement)
            ElementTree.indent(root, ' ')

            # workaround_1: cannot use `xml_declaration=True` since it uses single quotes instead of Qt preferred double quotes
            ret = f'<?xml version="1.0" encoding="UTF-8"?>\n{ElementTree.tostring(root, 'unicode')}\n'

            # workaround_2b: ElementTree will turn `&quot;` into `&amp;quot;`, so revert it back
            ret = ret.replace('&amp;quot;', '&quot;')

            # workaround_3: Qt prefers no whitespaces in self-closing tags
            ret = re.sub('<(.+) +/>', r'<\1/>', ret)

            if ret != orig:
                print(f'Tip: run this script to apply the fix: `python {__file__} {filename}`', file=sys.stderr)

                f.seek(0)
                f.write(ret)
                f.truncate()

    return 0


if __name__ == '__main__':
    sys.exit(main())
