#!/usr/bin/env python3

import argparse
import glob
import os
import shutil
import sys
from typing import List


def isNotStub(path: str) -> bool:
    return (os.path.getsize(path) >= (10 * 1024))


def main() -> int:
    parser = argparse.ArgumentParser(description='Gather valid Qt translations for NSIS packaging.')
    parser.add_argument("qt_translations_folder", help="Qt's translations folder")
    parser.add_argument("nsis_packaging_folder", help="NSIS packaging translations folder")
    args = parser.parse_args()

    tmp_translations: List[str] = glob.glob(f'{args.qt_translations_folder}/qt_??.qm')
    tmp_translations += glob.glob(f'{args.qt_translations_folder}/qt_??_??.qm')
    tmp_translations += glob.glob(f'{args.qt_translations_folder}/qtbase_??.qm')
    tmp_translations += glob.glob(f'{args.qt_translations_folder}/qtbase_??_??.qm')

    filtered = filter(isNotStub, tmp_translations)
    for file in filtered:
        shutil.copy2(file, args.nsis_packaging_folder)

    return 0


if __name__ == '__main__':
    sys.exit(main())
