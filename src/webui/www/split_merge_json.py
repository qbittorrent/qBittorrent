#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# split_merge_json.py - script for splitting the JSON files from
# Transifex into their public/private parts and to merging the
# private/public en.json files to form the source file for Transifex
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

def updateJson(json_file: str, source: dict[str, str]) -> None:
    trimmed_path: str = json_file
    path_parts = PurePath(json_file).parts
    if len(path_parts) >= 3:
        trimmed_path = f"{path_parts[-3]}/{path_parts[-2]}/{path_parts[-1]}"

    if not os.path.exists(json_file):
        print(f"\tWARNIG: {trimmed_path} doesn't exist")
        return

    print(f"\tUpdating {trimmed_path}")
    with open(json_file, mode="r+", encoding='utf-8') as file:
        target: dict[str, str] = json.load(file)
        if not isinstance(target, dict):
            print(f"\tWARNIG: {trimmed_path} is malormed")
            return

        for key in target.keys():
            value = source.get(key)
            if value:
                target[key] = value
        file.seek(0)
        json.dump(target, file, ensure_ascii=False, indent=2, sort_keys=True)
        file.write("\n")
        file.truncate()

def splitJson(transifex_dir: str, json_public_dir: str, json_private_dir: str) -> None:
    locales: list[str] = glob.glob("*.json", root_dir=transifex_dir)
    locales = [x for x in locales if x != "en.json"]
    for locale in locales:
        print(f"Processing lang {locale}")
        transifex_file: str = f"{transifex_dir}/{locale}"
        public_file: str = f"{json_public_dir}/{locale}"
        private_file: str = f"{json_private_dir}/{locale}"

        transifex_json: dict[str, str] = {}
        with open(transifex_file, mode="r", encoding='utf-8') as file:
            transifex_json = json.load(file)
            if not isinstance(transifex_json, dict):
                trimmed_path: str = transifex_file
                path_parts = PurePath(transifex_file).parts
                if len(path_parts) >= 2:
                    trimmed_path = f"{path_parts[-2]}/{path_parts[-1]}"
                print(f"\tERROR: {trimmed_path} is malformed.")
                continue

        updateJson(public_file, transifex_json)
        updateJson(private_file, transifex_json)

def mergeJson(transifex_dir: str, json_public_dir: str, json_private_dir: str) -> None:
    transifex_en_file: str = f"{transifex_dir}/en.json"
    public_en_file: str = f"{json_public_dir}/en.json"
    private_en_file: str = f"{json_private_dir}/en.json"

    transifex_en_json: dict[str, str] = {}
    public_en_json: dict[str, str] = {}
    private_en_json: dict[str, str] = {}

    print("Merging source en.json file")
    if os.path.exists(public_en_file):
        with open(public_en_file, mode="r", encoding='utf-8') as file:
            public_en_json = json.load(file)
            if not isinstance(public_en_json, dict):
                public_en_json = {}

    if os.path.exists(private_en_file):
        with open(private_en_file, mode="r", encoding='utf-8') as file:
            private_en_json = json.load(file)
            if not isinstance(private_en_json, dict):
                private_en_json = {}

    transifex_en_json = public_en_json | private_en_json
    with open(transifex_en_file, mode="w", encoding='utf-8') as file:
        json.dump(transifex_en_json, file, ensure_ascii=False, indent=2, sort_keys=True)
        file.write("\n")

def main() -> int:
    script_path: str = os.path.dirname(os.path.realpath(__file__))
    transifex_dir: str = f"{script_path}/transifex"
    json_public_dir: str = f"{script_path}/public/lang"
    json_private_dir: str = f"{script_path}/private/lang"

    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", required=True, choices=["split", "merge"], help="SPLIT the translations files from Transifex into their public/private JSON counterpart. MERGE to merge the public and private en.json files into the 'transifex/en.json' file")
    parser.add_argument("--transifex-dir", default=transifex_dir, nargs=1, help=f"directory of WebUI transifex file (default: '{transifex_dir}')")
    parser.add_argument("--json-public-dir", default=json_public_dir, nargs=1, help=f"directory of WebUI public JSON translation files(default: '{json_public_dir}')")
    parser.add_argument("--json-private-dir", default=json_private_dir, nargs=1, help=f"directory of WebUI private JSON translation files(default: '{json_private_dir}')")

    args = parser.parse_args()

    if not os.path.isdir(args.transifex_dir):
        raise RuntimeError(f"'{args.transifex_dir}' isn't a directory")

    if not os.path.isdir(args.json_public_dir):
        raise RuntimeError(f"'{args.json_public_dir}' isn't a directory")

    if not os.path.isdir(args.json_private_dir):
        raise RuntimeError(f"'{args.json_private_dir}' isn't a directory")

    if args.mode == "merge":
        mergeJson(transifex_dir, json_public_dir, json_private_dir)
    else:
        splitJson(transifex_dir, json_public_dir, json_private_dir)

    print("Done.")

    return 0

if __name__ == '__main__':
    sys.exit(main())
