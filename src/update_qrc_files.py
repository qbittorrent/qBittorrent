#!/usr/bin/env python

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#    * Redistributions of source code must retain the above copyright notice,
#      this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#    * Neither the name of the author nor the names of its contributors may be
#      used to endorse or promote products derived from this software without
#      specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# Small script to update qrc files (lang.qrc, icons.qrc)
# by Christophe Dumez <chris@qbittorrent.org>

import os
from os.path import splitext, join

# update languages files directory
languages_list = [x for x in os.listdir('lang') if x.endswith('.qm')]
output = '''<!DOCTYPE RCC><RCC version="1.0">
<qresource>
'''
for language in languages_list:
  output += '  <file>%s</file>'%('lang'+os.sep+language)
  output += os.linesep
output += '''</qresource>
</RCC>'''
lang_file = open('lang.qrc', 'w')
lang_file.write(output)
lang_file.close()

# update search_engine directory
os.chdir('gui/searchengine')
search_list = []
for nova_folder in ['nova/', 'nova3']:
  for root, dirs, files in os.walk(nova_folder):
    for file in files:
      if file.startswith("__"):
        continue
      if splitext(file)[-1] in ('.py', '.png'):
        search_list.append(join(root, file))

output = '''<!DOCTYPE RCC><RCC version="1.0">
<qresource>
'''
for file in search_list:
  output += '  <file>%s</file>'%(file)
  output += os.linesep
output += '''</qresource>
</RCC>'''
search_file = open('search.qrc', 'w')
search_file.write(output)
search_file.close()

os.chdir('../..');

# update icons files directory
icons_list = []
for root, dirs, files in os.walk('icons'):
  if 'skin_unused' in dirs:
    dirs.remove('skin_unused')
  for file in files:
    if splitext(file)[-1] in ('.png', '.jpg', '.gif'):
      icons_list.append(join(root, file))

output = '''<!DOCTYPE RCC><RCC version="1.0">
<qresource>
'''
for icon in icons_list:
  output += '  <file>%s</file>'%(icon)
  output += os.linesep
output += '''</qresource>
</RCC>'''
icons_file = open('icons.qrc', 'w')
icons_file.write(output)
icons_file.close()

