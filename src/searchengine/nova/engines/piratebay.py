#VERSION: 2.00
#AUTHORS: Fabien Devaux (fab@gnux.info)
#CONTRIBUTORS: Christophe Dumez (chris@qbittorrent.org)
#              Arthur (custparasite@gmx.se)

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

from novaprinter import prettyPrinter
from HTMLParser import HTMLParser
from helpers import download_file
import urllib2

PREVIOUS_IDS = set()

class piratebay(object):
    url = 'http://thepiratebay.se'
    name = 'The Pirate Bay'
    supported_categories = {'all': '0', 'music': '100', 'movies': '200', 'games': '400', 'software': '300'}

    def download_torrent(self, info):
        print(download_file(info))

    class MyHtmlParseWithBlackJack(HTMLParser):
        def __init__(self, results, url):
            HTMLParser.__init__(self)
            self.url = url
            self.results = results
            self.current_item = None
            self.size_found = False
            self.unit_found = False
            self.seed_found = False
            self.skip_td = False
            self.leech_found = False
            self.dispatcher = {'a'      : self.handle_tag_a_ref,
                               'font'   : self.handle_tag_font_size,
                               'td'     : self.handle_tag_td_sl      }

        def handle_tag_a_ref(self, attrs):
            params = dict(attrs)
            #1
            if params['href'].startswith('/torrent/'):
                get_id = params['href'].split('/')[2]
                if not get_id in PREVIOUS_IDS:
                    self.current_item = {}
                    self.current_item['desc_link'] = self.url + params['href'].strip()
                    self.current_item['name'] = params['title'][12:].strip()
                    self.current_item['id'] = get_id
            #2
            elif (not self.current_item is None) and (params['href'].startswith('magnet:')):
                self.current_item['link'] = params['href'].strip()

        def handle_tag_font_size(self, attrs):
            if not self.current_item is None:
                params = dict(attrs)
                #3
                if params['class'] == "detDesc":
                    self.size_found = True

        def handle_tag_td_sl(self, attrs):
            if not self.current_item is None:
                params = dict(attrs)
                if not self.current_item is None:
                    if self.seed_found:
                        #5
                        self.current_item['leech'] = ''
                        self.leech_found = True
                        self.seed_found = False
                    else:
                        #4
                        self.current_item['seeds'] = ''
                        self.seed_found = True

        def handle_starttag(self, tag, attrs):
            if tag in self.dispatcher:
                self.dispatcher[tag](attrs)

        def handle_data(self, data):
            if not self.current_item is None:
                if self.size_found:
                    #with utf-8 you're going to have something like that: ['Uploaded', '10-02'], ['15:31,', 'Size', '240.34'], ['MiB,', 'ULed', 'by']
                    temp = data.split()
                    if 'Size' in temp:
                        sizeIn = temp.index('Size')
                        self.current_item['size'] = temp[sizeIn + 1]
                        self.size_found = False
                        self.unit_found = True
                elif self.unit_found:
                    temp = data.split()
                    self.current_item['size'] = ' '.join((self.current_item['size'], temp[0]))
                    self.unit_found = False
                elif self.seed_found:
                    self.current_item['seeds'] += data.rstrip()
                elif self.leech_found:
                    self.current_item['leech'] += data.rstrip()
                    self.current_item['engine_url'] = self.url
                    prettyPrinter(self.current_item)
                    PREVIOUS_IDS.add(self.current_item['id'])
                    self.results.append('a')
                    self.current_item = None
                    self.size_found = False
                    self.unit_found = False
                    self.seed_found = False
                    self.leech_found = False

    def search(self, what, cat='all'):
        ret = []
        i = 0
        while i < 11:
            results = []
            parser = self.MyHtmlParseWithBlackJack(results, self.url)
            query = '%s/search/%s/%d/99/%s' % (self.url, what, i, self.supported_categories[cat])
            dat = urllib2.urlopen(query)
            parser.feed(dat.read().decode('utf-8'))
            parser.close()
            if len(results) <= 0:
                break
            i += 1
