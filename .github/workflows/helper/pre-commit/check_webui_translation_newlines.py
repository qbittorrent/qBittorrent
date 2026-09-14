#!/usr/bin/env python3

# A pre-commit hook for detecting problematic WebUI translations
# Copyright (C) 2026  qBittorrent project
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
from collections.abc import Sequence
from typing import Optional


MESSAGE_REGEX = re.compile(r"<message\b[^>]*>(?P<body>.*?)</message>", re.DOTALL)
SOURCE_REGEX = re.compile(r"<source\b[^>]*>(?P<body>.*?)</source>", re.DOTALL)
TRANSLATION_REGEX = re.compile(
    r"<translation\b(?P<attrs>[^>]*)>(?P<body>.*?)</translation>", re.DOTALL)
TRANSLATION_TYPE_REGEX = re.compile(r"\btype\s*=")


def _line_number(text: str, index: int) -> int:
    return text.count("\n", 0, index) + 1


def _preview(text: str) -> str:
    return text.replace("\n", "\\n")


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("filenames", nargs="*", help="Filenames to check")
    args = parser.parse_args(argv)

    errors = []

    for filename in args.filenames:
        try:
            with open(filename) as file:
                data = file.read()
        except UnicodeDecodeError:
            # not a text file, skip
            continue

        for message_match in MESSAGE_REGEX.finditer(data):
            message = message_match.group("body")
            source_match = SOURCE_REGEX.search(message)
            translation_match = TRANSLATION_REGEX.search(message)
            if ((source_match is None) or (translation_match is None)):
                continue

            source = source_match.group("body")
            translation = translation_match.group("body")
            has_raw_newline_in_source = ("\n" in source)
            has_raw_newline_in_translation = ("\n" in translation)
            has_translation_type = (
                TRANSLATION_TYPE_REGEX.search(translation_match.group("attrs")) is not None)
            if (has_translation_type
                    or (not has_raw_newline_in_translation)
                    or has_raw_newline_in_source):
                continue

            line_number = _line_number(
                data, message_match.start() + translation_match.start("body"))
            errors.append(
                f"Defect file: \"{filename}\"\n"
                f"Line: {line_number}\n"
                "Reason: <translation> contains raw newline characters "
                "but the matching <source> does not.\n"
                f"Source: \"{_preview(source)}\"\n"
                f"Translation: \"{_preview(translation)}\"\n")

    if len(errors) > 0:
        print("\n".join(errors))
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
