#VERSION: 1.28
#AUTHORS: Christophe Dumez (chris@qbittorrent.org)
#CONTRIBUTORS: Diego de las Heras (ngosang@hotmail.es)

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

import logging
from novaprinter import prettyPrinter
from helpers import retrieve_url, download_file
from json import loads as json_loads

class kickasstorrents(object):
    url = 'https://kat.cr'
    name = 'Kickass Torrents'
    supported_categories = {'all': '', 'movies': 'Movies', 'tv': 'TV', 'music': 'Music', 'games': 'Games', 'software': 'Applications'}

    def __init__(self):
        pass

    def download_torrent(self, info):
        print(download_file(info, info))

    def search(self, what, cat='all'):
        base_query = self.url + '/json.php?q=%s&order=seeders&sort=desc' % what
        for i in range(1, 12):
            query = base_query + "&page=%d" % i

            json_data = retrieve_url(query)
            try:
                json_dict = json_loads(json_data)
            except JSONDecodeError:
                logging.warning("Unable to parse %s result for request=%s", self.url, query)
                continue

            if int(json_dict['total_results']) <= 0:
                return

            for r in json_dict['list']:
                try:
                    if cat != 'all' and self.supported_categories[cat] != r['category']:
                        continue
                    res_dict = dict()
                    res_dict['name'] = r['title']
                    res_dict['size'] = str(r['size'])
                    res_dict['seeds'] = r['seeds']
                    res_dict['leech'] = r['leechs']
                    res_dict['link'] = r['torrentLink']
                    res_dict['desc_link'] = r['link'].replace('http://', 'https://')
                    res_dict['engine_url'] = self.url
                    prettyPrinter(res_dict)
                except:
                    logging.debug("Bad content from %s is found. Skip it. Content=%s", self.name, r)
                    pass
