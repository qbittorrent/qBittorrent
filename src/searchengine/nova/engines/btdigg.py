#VERSION: 1.30
#AUTHORS: BTDigg team (research@btdigg.org)

#                    GNU GENERAL PUBLIC LICENSE
#                       Version 3, 29 June 2007
#
#                   <http://www.gnu.org/licenses/>
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.

import urllib
import urllib2
import sys

from novaprinter import prettyPrinter

class btdigg(object):
    url = 'https://btdigg.org'
    name = 'BTDigg'

    supported_categories = {'all': ''}

    def __init__(self):
        pass

    def search(self, what, cat='all'):
        req = urllib.unquote(what)
        what_list = req.decode('utf8').split()
        i = 0
        results = 0
        while i < 3:
            u = urllib2.urlopen('https://api.btdigg.org/api/public-8e9a50f8335b964f/s01?%s' % urllib.urlencode(dict(q = req, p = i)))
            for line in u:
                try:
                    line = line.decode('utf8')
                    if line.startswith('#'):
                        continue

                    info_hash, name, files, size, dl, seen = line.strip().split('\t')[:6]
                    name = name.replace('|', '')
                    # BTDigg returns unrelated results, we need to filter
                    if not all(word in name.lower() for word in what_list):
                        continue

                    res = dict(link = 'magnet:?xt=urn:btih:%s&dn=%s' % (info_hash, urllib.quote(name.encode('utf8'))),
                               name = name,
                               size = size,
                               seeds = int(dl),
                               leech = int(dl),
                               engine_url = self.url,
                               desc_link = '%s/search?%s' % (self.url, urllib.urlencode(dict(info_hash = info_hash, q = req))))

                    prettyPrinter(res)
                    results += 1
                except:
                    pass

            if results == 0:
                break
            i += 1

if __name__ == "__main__":
    s = btdigg()
    s.search(sys.argv[1])
