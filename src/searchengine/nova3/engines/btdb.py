#VERSION: 1.01
#AUTHORS: Charles Worthing
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

from html.parser import HTMLParser
#qBt
from novaprinter import prettyPrinter
from helpers import download_file, retrieve_url

class btdb(object):
    """ Search engine class """
    url = 'https://btdb.in'
    name = 'BTDB'

    def download_torrent(self, info):
        """ Downloader """
        print(download_file(info))

    class MyHtmlParser(HTMLParser):
        """ Parser class """
        def __init__(self, results, url):
            HTMLParser.__init__(self)
            self.results = results
            self.url = url
            self.current_item = {} # One torrent result
            self.add_query = True
            self.torrent_info_index = 0 # Count of the meta data encountered
            self.torrent_info_array = []
            self.meta_data_grabbing = 0
            self.meta_data_array = []
            self.torrent_no_files = 0
            self.torrent_date_added = 0
            self.torrent_popularity = 0
            self.mangnet_link = ""
            self.desc_link = ""
            self.torrent_name = ""

        def handle_starttag(self, tag, attrs):
            if tag == "span":
                span_dict = dict(attrs)
                if "class" in span_dict:
                    the_class = span_dict["class"]
                    if the_class == "item-meta-info-value":
                        self.meta_data_grabbing += 1
                    else:
                        self.meta_data_grabbing = 0
            if tag == "script":
                return
            if tag == "li":
                for attr in attrs:
                    if attr[1] == "search-ret-item":
                        self.torrent_info_index = 1
            if tag == "a":
                if self.torrent_info_index > 0:
                    params = dict(attrs)
                    if "href" in params:
                        link = params["href"]
                        if link.startswith("/torrent"):
                            self.desc_link = "".join((self.url, link))
                            self.torrent_name = params["title"]
                        if link.startswith("magnet:"):
                            self.mangnet_link = link

        def handle_endtag(self, tag): 
            if tag == "script":
                return
            if tag == "div":
                if self.meta_data_grabbing > 0:
                    
                    self.torrent_no_files = self.meta_data_array[2] # Not used
                    self.torrent_date_added = self.meta_data_array[4] # Not used
                    self.torrent_popularity = self.meta_data_array[6] # Not used

                    self.current_item["size"] = self.meta_data_array[0]
                    self.current_item["name"] = self.torrent_name
                    self.current_item["engine_url"] = self.url
                    self.current_item["link"] = self.mangnet_link
                    self.current_item["desc_link"] = self.desc_link
                    self.current_item["seeds"] = -1
                    self.current_item["leech"] = -1

                    prettyPrinter(self.current_item)
                    self.results.append('a')
                    self.current_item = {}

                    self.meta_data_grabbing = 0
                    self.meta_data_array = []
                    self.mangnet_link = ""
                    self.desc_link = ""
                    self.torrent_name = ""

        def handle_data(self, data):
            if self.torrent_info_index > 0:
                self.torrent_info_array.append(data)
                self.torrent_info_index += 1
            if self.meta_data_grabbing > 0:
                self.meta_data_array.append(data)
                self.meta_data_grabbing += 1

        def handle_entityref(self, name):
            c = unichr(name2codepoint[name])

        def handle_charref(self, name):
            if name.startswith('x'):
                c = unichr(int(name[1:], 16))
            else:
                c = unichr(int(name))


    def search(self, what, cat='all'):
        """ Performs search """
        results_list = []
        parser = self.MyHtmlParser(results_list, self.url)
        i = 1
        while i < 31:
            # "what" is already urlencoded
            html = retrieve_url(self.url + '/q/%s/%d?sort=popular' % (what, i))
            parser.feed(html)
            if len(results_list) < 1:
                break
            del results_list[:]
            i += 1
        parser.close()
