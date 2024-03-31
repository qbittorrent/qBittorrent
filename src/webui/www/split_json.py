#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# split_json.py - script for splitting the TS files from
# Transifex into their public/private JSON parts
#
# Copyright (C) 2024   sledgehammer999 <hammered999@gmail.com>
#
# License: Public domain.

import argparse
import glob
import json
import os
from pathlib import PurePath
import sys
import xml.etree.ElementTree as ET


def updateJson(json_file: str, ts_file: str, context_separator: str) -> None:
    trimmed_path: str = json_file
    path_parts = PurePath(json_file).parts
    if len(path_parts) >= 3:
        trimmed_path = f"{path_parts[-3]}/{path_parts[-2]}/{path_parts[-1]}"

    print(f"\tUpdating {trimmed_path}")
    shouldDelete: bool = False
    with open(json_file, mode="r+", encoding='utf-8') as file:
        target: dict[str, str] = json.load(file)
        if not isinstance(target, dict):
            print(f"\tWARNIG: {trimmed_path} is malormed")
            shouldDelete = True
            return

        tree = ET.parse(ts_file)
        root = tree.getroot()

        for tsContext in root.findall('context'):
            context = tsContext.find('name').text
            for message in tsContext.findall('message'):
                source = message.find('source').text
                translation = message.find('translation').text
                if not translation:
                    continue
                keyWithContext = f"{source}{'{context_separator}{context}' if context else ''}"
                if keyWithContext in target:
                    target[keyWithContext] = translation

        file.seek(0)
        json.dump(target, file, ensure_ascii=False, indent=2, sort_keys=True)
        file.write("\n")
        file.truncate()

    if (shouldDelete):
        os.remove(json_file)


def splitJson(transifex_dir: str, json_public_dir: str, json_private_dir: str, context_separator: str) -> None:
    locales: list[str] = glob.glob("*.ts", root_dir=transifex_dir)
    locales = [x for x in locales if x != "webui_en.ts"]
    for locale in locales:
        print(f"Processing lang {locale}")
        transifex_file: str = f"{transifex_dir}/{locale}"
        localeNoExt: str = locale.removesuffix(".ts").removeprefix("webui_")
        public_file: str = f"{json_public_dir}/{localeNoExt}.json"
        private_file: str = f"{json_private_dir}/{localeNoExt}.json"

        updateJson(public_file, transifex_file, context_separator)
        updateJson(private_file, transifex_file, context_separator)


def main() -> int:
    script_path: str = os.path.dirname(os.path.realpath(__file__))
    transifex_dir: str = f"{script_path}/translations"
    json_public_dir: str = f"{script_path}/public/lang"
    json_private_dir: str = f"{script_path}/private/lang"
    context_separator: str = "_"

    parser = argparse.ArgumentParser()
    parser.add_argument("--transifex-dir", default=transifex_dir, nargs=1, help=f"directory of WebUI transifex file (default: '{transifex_dir}')")
    parser.add_argument("--json-public-dir", default=json_public_dir, nargs=1, help=f"directory of WebUI public JSON translation files(default: '{json_public_dir}')")
    parser.add_argument("--json-private-dir", default=json_private_dir, nargs=1, help=f"directory of WebUI private JSON translation files(default: '{json_private_dir}')")
    parser.add_argument("--context-separator", default=context_separator, nargs=1, help=f"context separator for i18next(default: '{context_separator}')")

    args = parser.parse_args()
    transifex_dir = args.transifex_dir
    json_public_dir = args.json_public_dir
    json_private_dir = args.json_private_dir
    context_separator = args.context_separator

    if not os.path.isdir(transifex_dir):
        raise RuntimeError(f"'{transifex_dir}' isn't a directory")

    if not os.path.isdir(json_public_dir):
        raise RuntimeError(f"'{json_public_dir}' isn't a directory")

    if not os.path.isdir(json_private_dir):
        raise RuntimeError(f"'{json_private_dir}' isn't a directory")

    splitJson(transifex_dir, json_public_dir, json_private_dir, context_separator)

    print("Done.")

    return 0


if __name__ == '__main__':
    sys.exit(main())
