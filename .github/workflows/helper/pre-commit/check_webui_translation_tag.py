#!/usr/bin/env python3

# A pre-commit hook for detecting problematic <translation> tags
# Copyright (C) 2026  Mike Tzou (Chocobo1)
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

import argparse
import re
import sys
import xml.etree.ElementTree as ElementTree
from collections.abc import Sequence
from typing import Optional


def escapeNewlineCharacters(element: ElementTree.Element) -> None:
    source = element.find('source')
    if (source is None) or (source.text is None):
        return

    translation = element.find('translation')
    if (translation is None) or (translation.text is None):
        return

    newlines = {
        '\n': '\\n',
        '\r': '\\r'
    }

    for (unescapedChar, escapedChar) in newlines.items():
        if unescapedChar not in source.text:
            # `translation` probably didn't escape the characters properly
            if (escapedChar in source.text) and (unescapedChar in translation.text):
                translation.text = translation.text.replace(unescapedChar, escapedChar)

            # `translation` introduced unnecessary characters
            if (escapedChar not in source.text) and (unescapedChar in translation.text):
                translation.text = translation.text.replace(unescapedChar, '')


def defuseTranslationPayload(element: ElementTree.Element) -> None:
    source = element.find('source')
    if (source is None) or (source.text is None):
        return

    translation = element.find('translation')
    if (translation is None) or (translation.text is None):
        return

    escapePatterns = {
        '${',  # JS template interpolation. Should not be used.
        '`',  # JS Template Literal. Should be escaped.
        # '"' JS String Literal. Should be escaped, but doing so might add unnecessary backslashes.
        # '\'' JS String Literal. Do not need to be escaped since ESLint prohibits it.
    }

    for entity in escapePatterns:
        if entity not in source.text:
            pattern = rf"(?<!\\){re.escape(entity)}"
            translation.text = re.sub(pattern, f'\\{entity}', translation.text)  # pyright: ignore[reportArgumentType, reportCallIssue]

    removePatterns = {
        '\u2028',  # Line Separator. Not in use.
        '\u2029'  # Paragraph Separator. Not in use.
    }

    for entity in removePatterns:
        if entity not in source.text:
            pattern = rf"(?<!\\){re.escape(entity)}"
            translation.text = re.sub(pattern, '', translation.text)  # pyright: ignore[reportArgumentType, reportCallIssue]


def fixTrailingSpaces(element: ElementTree.Element) -> None:
    source = element.find('source')
    if (source is None) or (source.text is None) or (len(source.text) == 0):
        return

    translation = element.find('translation')
    if (translation is None) or (translation.text is None) or (len(translation.text) == 0):
        return

    sourceLastChar = source.text[-1]
    translationLastChar = translation.text[-1]

    if sourceLastChar.isspace():
        if sourceLastChar != translationLastChar:
            # the last character may be a space or a newline character, but they do not match between `source` and `translation`.
            if translationLastChar.isspace():
                translation.text = translation.text[:-1] + sourceLastChar
            else:
                # for locale using full-width character set, an empty space is not strictly required here
                if sourceLastChar != ' ':
                    translation.text += sourceLastChar
    else:
        if translationLastChar.isspace():
            # `source` didn't end with space so `translation` shouldn't either
            translation.text = translation.text.rstrip()


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('filenames', nargs='*', help='Filenames to check')
    args = parser.parse_args(argv)

    for filename in args.filenames:
        with open(filename, 'r+', encoding='utf-8') as f:
            orig = f.read()
            root = ElementTree.fromstring(orig)

            for context in root.findall('context'):
                for message in context.findall('message'):
                    escapeNewlineCharacters(message)
                    fixTrailingSpaces(message)
                    defuseTranslationPayload(message)

            ElementTree.indent(root, '')
            for context in root.findall('context'):
                ElementTree.indent(context, '    ')

            # workaround_1: cannot use `xml_declaration=True` since it uses single quotes instead of Qt preferred double quotes
            ret = f'<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>\n{ElementTree.tostring(root, "unicode")}'

            if ret != orig:
                print(f'Tip: run this script to apply the fix: `python {__file__} {filename}`', file=sys.stderr)

                f.seek(0)
                f.write(ret)
                f.truncate()

    return 0


if __name__ == '__main__':
    sys.exit(main())
