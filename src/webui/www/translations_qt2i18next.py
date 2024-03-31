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


def getTsStrings(ts_file: str, key: str, jsonContext: str) -> list[str]:
    tr_strings: list[str] = []
    candidates: list[str] = []
    tree = ET.parse(ts_file)
    root = tree.getroot()

    for tsContext in root.findall('context'):
        context = tsContext.find('name').text
        for message in tsContext.findall('message'):
            source = message.find('source').text
            if key != source:
                continue

            translation = message.find('translation').text
            if not translation:
                continue

            if jsonContext != context:
                if translation not in candidates:
                    candidates.append(translation)
                continue

            if translation not in tr_strings:
                tr_strings.append(translation)

    return tr_strings + candidates


def migrate2Json(ts_dir: str, json_dir: str, locale: str, contextSeparator: str) -> None:
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

            keyAndContext: list[str] = key.split(contextSeparator)
            ts_strings: list[str] = getTsStrings(ts_file, keyAndContext[0], keyAndContext[1] if 1 < len(keyAndContext) else "")
            ts_len: int = len(ts_strings)
            if ts_len == 0:
                print(f"\tWARNING: Translation for '{keyAndContext[0]}' not found.")
                continue

            if ts_len > 1:
                print(f"\tWARNING: Multiple translations for '{keyAndContext[0]}' found.")
                continue

            # We want the full key here (context included)
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
    context_separator: str = "_"

    parser = argparse.ArgumentParser()
    parser.add_argument("--ts-dir", default=ts_dir, nargs=1, help=f"directory of WebUI .ts translation files (default: '{ts_dir}')")
    parser.add_argument("--json-dir", default=json_dir, nargs=1, help=f"directory of WebUI .json translation files(default: '{json_dir}')")
    parser.add_argument("--context-separator", default=context_separator, nargs=1, help=f"context separator for i18next(default: '{context_separator}')")

    args = parser.parse_args()
    ts_dir = args.ts_dirs
    json_dir = args.json_dir
    context_separator = args.context_separator

    if not os.path.isdir(ts_dir):
        raise RuntimeError(f"'{ts_dir}' isn't a directory")

    if not os.path.isdir(json_dir):
        raise RuntimeError(f"'{json_dir}' isn't a directory")

    locales: list[str] = glob.glob("*.json", root_dir=json_dir)
    locales = [x.removesuffix(".json") for x in locales if x != "en.json"]
    for locale in locales:
        migrate2Json(ts_dir, json_dir, locale, context_separator)

    print("Done.")

    return 0


if __name__ == '__main__':
    sys.exit(main())
