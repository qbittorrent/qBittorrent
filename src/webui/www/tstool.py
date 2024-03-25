#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# TSTool - script for update qBittorrent WebUI translation files
# Copyright (C) 2018  Vladimir Golovnev <glassez@yandex.ru>
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
import copy
import os
import os.path
import re
import sys
import xml.etree.ElementTree as ET

accepted_exts = [".js", ".html", ".css"]

no_obsolete = False
www_folder = "."
ts_folder = os.path.join(www_folder, "translations")


def parseSource(filename, sources):
    print("Parsing %s..." % (os.path.normpath(filename)))
    with open(filename, encoding='utf-8', mode='r') as file:
        regex = re.compile(
            r"QBT_TR\((([^\)]|\)(?!QBT_TR))+)\)QBT_TR\[CONTEXT=([a-zA-Z_][a-zA-Z0-9_]*)\]")
        for match in regex.finditer(file.read()):
            string = match.group(1)
            context = match.group(3)

            if context not in sources:
                sources[context] = set()
            sources[context].add(string)


def processTranslation(filename, sources):
    print('Processing %s...' % (os.path.normpath(filename)))

    try:
        tree = ET.ElementTree(file=filename)
    except Exception:
        print('\tFailed to parse %s!' % (os.path.normpath(filename)))
        return

    root = tree.getroot()
    for context in root.findall('context'):
        context_name = context.find('name').text
        has_context = context_name in sources
        if not has_context and no_obsolete:
            root.remove(context)
            continue

        for message in context.findall('message'):
            for location in message.findall('location'):
                message.remove(location)

            source = message.find('source').text
            translation = message.find('translation')
            if has_context and source in sources[context_name]:
                sources[context_name].remove(source)

                trtype = translation.attrib.get('type')
                if (trtype == 'obsolete') or (trtype == 'vanished'):
                    del translation.attrib['type']  # i.e. finished
            else:
                if no_obsolete or (translation.attrib.get('type', '') == 'unfinished'):
                    context.remove(message)
                else:
                    translation.attrib['type'] = 'vanished'

        if not has_context:
            continue

        # add new messages for current context
        for source in sources[context_name]:
            message = ET.SubElement(context, 'message')
            ET.SubElement(message, 'source').text = source
            ET.SubElement(message, 'translation', {'type': 'unfinished'})
        del sources[context_name]

    # add messages for new contexts
    for context_name in sources:
        context = ET.SubElement(root, 'context')
        ET.SubElement(context, 'name').text = context_name

        for source in sources[context_name]:
            message = ET.SubElement(context, 'message')
            ET.SubElement(message, 'source').text = source
            ET.SubElement(message, 'translation', {'type': 'unfinished'})

    # prettify output xml
    indent = ' ' * 4
    root.text = '\n'
    for context in root.findall('./context'):
        context.text = '\n' + indent
        context.tail = '\n'
        context.find('./name').tail = '\n' + indent
        messages = context.findall('./message')
        if len(messages) == 0:
            continue

        for message in messages:
            message.text = '\n' + (indent * 2)
            message.tail = '\n' + indent
            elems = message.findall('./')
            if len(elems) == 0:
                continue

            for elem in elems:
                elem.tail = '\n' + (indent * 2)
            elems[-1:][0].tail = '\n' + indent
        messages[-1:][0].tail = '\n'

    try:
        with open(filename, mode='wb') as file:
            file.write(b'<?xml version="1.0" encoding="utf-8"?>\n'
                       b'<!DOCTYPE TS>\n')
            tree.write(file, encoding='utf-8')
    except Exception:
        print('\tFailed to write %s!' % (os.path.normpath(filename)))


argp = argparse.ArgumentParser(
    prog='tstool.py', description='Update qBittorrent WebUI translation files.')
argp.add_argument('--no-obsolete', dest='no_obsolete', action='store_true',
                  default=no_obsolete,
                  help='remove obsolete messages (default: mark them as obsolete)')
argp.add_argument('--www-folder', dest='www_folder', action='store',
                  default=www_folder,
                  help='folder with WebUI source files (default: "%s")' % (www_folder))
argp.add_argument('--ts-folder', dest='ts_folder', action='store',
                  default=ts_folder,
                  help='folder with WebUI translation files (default: "%s")' % (ts_folder))

args = argp.parse_args()
no_obsolete = args.no_obsolete
www_folder = args.www_folder
ts_folder = args.ts_folder

print("Processing source files...")
nfiles = 0
source_ts = {}
for root, dirs, files in os.walk(www_folder):
    for file in files:
        if os.path.splitext(file)[-1] in accepted_exts:
            parseSource(os.path.join(root, file), source_ts)
            nfiles += 1

if nfiles == 0:
    print("No source files found!")
    sys.exit()

nstrings = sum(len(sublist) for sublist in source_ts)
print("Found %d strings within %d contexts." % (nstrings, len(source_ts)))
print("")

print("Processing translation files...")
for entry in os.scandir(ts_folder):
    if (entry.is_file() and entry.name.startswith('webui_') and entry.name.endswith(".ts")):
        processTranslation(entry.path, copy.deepcopy(source_ts))

print("Done!")
