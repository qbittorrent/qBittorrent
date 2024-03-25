#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# translations_qt2i18next.py - script for migrating translations from Qt's .ts files
# to i18next compatible JSON files.
# Copyright (C) 2024   sledgehammer999 <hammered999@gmail.com>
#
# License: Public domain.

import argparse
import glob
import json
import os
import sys
import xml.etree.ElementTree as ET


def getTsStrings(ts_file: str, key: str) -> list[str]:
    tr_strings: list[str] = []
    tree = ET.parse(ts_file)
    root = tree.getroot()

    for context in root.findall('context'):
        for message in context.findall('message'):
            source = message.find('source').text
            if key != source:
                continue

            translation = message.find('translation').text
            if not translation:
                continue
            if translation not in tr_strings:
                tr_strings.append(translation)

    return tr_strings


def migrate2Json(ts_dir: str, json_dir: str, locale: str) -> None:
    ts_file: str = f"{ts_dir}/webui_{locale}.ts"
    js_file: str = f"{json_dir}/{locale}.json"

    print(f"Processing lang {locale}")
    if not os.path.exists(ts_file):
        print(f"\tERROR: {ts_file} doesn't exist.")
        return

    with open(js_file, mode="r+", encoding='utf-8') as file:
        translations = json.load(file)
        if not isinstance(translations, dict):
            print(f"\tERROR: {js_file} is malformed.")
            return

        for key, value in translations.items():
            if not (isinstance(key, str) and isinstance(value, str) and not value):
                continue

            ts_strings: list[str] = getTsStrings(ts_file, key)
            ts_len: int = len(ts_strings)
            if ts_len == 0:
                print(f"\tWARNING: Translation for '{key}' not found.")
                continue

            if ts_len > 1:
                print(f"\tWARNING: Multiple translations for '{key}' found.")
                continue

            translations[key] = ts_strings[0]

        file.seek(0)
        json.dump(translations, file, ensure_ascii=False, indent=2, sort_keys=True)
        file.write("\n")
        file.truncate()

    print("\tFinished.")


def main() -> int:
    script_path: str = os.path.dirname(os.path.realpath(__file__))
    ts_dir: str = f"{script_path}/translations"
    json_dir: str = f"{script_path}/public/lang"

    parser = argparse.ArgumentParser()
    parser.add_argument("--ts-dir", default=ts_dir, nargs=1, help=f"directory of WebUI .ts translation files (default: '{ts_dir}')")
    parser.add_argument("--json-dir", default=json_dir, nargs=1, help=f"directory of WebUI .json translation files(default: '{json_dir}')")

    args = parser.parse_args()

    if not os.path.isdir(args.ts_dir):
        raise RuntimeError(f"'{args.ts_dir}' isn't a directory")

    if not os.path.isdir(args.json_dir):
        raise RuntimeError(f"'{args.json_dir}' isn't a directory")

    locales: list[str] = glob.glob("*.json", root_dir=json_dir)
    locales = [x.removesuffix(".json") for x in locales if x != "en.json"]
    for locale in locales:
        migrate2Json(ts_dir, json_dir, locale)

    print("Done.")

    return 0


if __name__ == '__main__':
    sys.exit(main())
