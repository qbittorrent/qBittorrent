#VERSION: 2.00
#AUTHORS: Christophe Dumez (chris@qbittorrent.org)
#CONTRIBUTORS: Diego de las Heras (diegodelasheras@gmail.com)

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
from http.client import HTTPConnection as http
from novaprinter import prettyPrinter
from helpers import download_file

class mininova(object):
    """ Search engine class """
    url = 'http://www.mininova.org'
    name = 'Mininova'
    supported_categories = {'all'       : '0',
                            'movies'    : '4',
                            'tv'        : '8',
                            'music'     : '5',
                            'games'     : '3',
                            'anime'     : '1',
                            'software'  : '7',
                            'pictures'  : '6',
                            'books'     : '2'}

    def download_torrent(self, info):
        print(download_file(info))

    class MyHtmlParseWithBlackJack(HTMLParser):
        """ Parser class """
        def __init__(self, list_searches, url):
            HTMLParser.__init__(self)
            self.list_searches = list_searches
            self.url = url
            self.table_results = False
            self.current_item = None
            self.cur_item_name = None
            self.next_queries = True

        def handle_starttag_tr(self, _):
            """ Handler of tr start tag """
            self.current_item = dict()

        def handle_starttag_a(self, attrs):
            """ Handler of a start tag """
            params = dict(attrs)
            link = params["href"]

            if link.startswith("/get/"):
                #download link
                self.current_item["link"] = "".join((self.url, link))
            elif link.startswith("/tor/"):
                #description
                self.current_item["desc_link"] = "".join((self.url, link))
                self.cur_item_name = "name"
                self.current_item["name"] = ""
            elif self.next_queries and link.startswith("/search"):
                if params["title"].startswith("Page"):
                    self.list_searches.append(link)

        def handle_starttag_td(self, attrs):
            """ Handler of td start tag """
            if ("align", "right") in attrs:
                if not "size" in self.current_item.keys():
                    self.cur_item_name = "size"
                    self.current_item["size"] = ""

        def handle_starttag_span(self, attrs):
            """ Handler of span start tag """
            if ("class", "g") in attrs:
                self.cur_item_name = "seeds"
                self.current_item["seeds"] = ""
            elif ("class", "b") in attrs:
                self.cur_item_name = "leech"
                self.current_item["leech"] = ""

        def handle_starttag(self, tag, attrs):
            """ Parser's start tag handler """
            if self.table_results:
                dispatcher = getattr(self, "_".join(("handle_starttag", tag)), None)
                if dispatcher:
                    dispatcher(attrs)

            elif tag == "table":
                self.table_results = ("class", "maintable") in attrs

        def handle_endtag(self, tag):
            """ Parser's end tag handler """
            if tag == "tr" and self.current_item:
                self.current_item["engine_url"] = self.url
                prettyPrinter(self.current_item)
                self.current_item = None
            elif self.cur_item_name:
                if tag == "a" or tag == "span":
                    self.cur_item_name = None

        def handle_data(self, data):
            """ Parser's data handler """
            if self.cur_item_name:
                temp = self.current_item[self.cur_item_name]
                self.current_item[self.cur_item_name] = " ".join((temp, data))

    def search(self, what, cat="all"):
        """ Performs search """
        connection = http("www.mininova.org")

        query = "/".join(("/search", what, self.supported_categories[cat], "seeds"))

        connection.request("GET", query)
        response = connection.getresponse()
        if response.status != 200:
            return

        list_searches = []
        parser = self.MyHtmlParseWithBlackJack(list_searches, self.url)
        parser.feed(response.read().decode('utf-8'))
        parser.close()

        parser.next_queries = False
        for search_query in list_searches:
            connection.request("GET", search_query)
            response = connection.getresponse()
            parser.feed(response.read().decode('utf-8'))
            parser.close()

        connection.close()
        return
