#!/usr/bin/env python3

# A pre-commit hook for detecting problematic <translation> tags
# Copyright (C) 2021  Mike Tzou (Chocobo1)
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

from collections.abc import Sequence
from typing import Optional
import argparse
import re
import sys


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('filenames', nargs='*', help='Filenames to check')
    args = parser.parse_args(argv)

    error_msg = ""
    regex = re.compile(r"\s*</translation>")

    for filename in args.filenames:
        line_counter = 1
        error_buffer = ""

        with open(filename) as file:
            try:
                for line in file:
                    if (match := regex.match(line)) is not None:
                        error_buffer += str(f"Defect file: \"{filename}\"\n"
                                            f"Line: {line_counter}\n"
                                            f"Column span: {match.span()}\n"
                                            f"Part: \"{match.group()}\"\n\n")
                    line_counter += 1

            except UnicodeDecodeError:
                # not a text file, skip
                continue

        error_msg += error_buffer

    if len(error_msg) > 0:
        print(error_msg)
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
