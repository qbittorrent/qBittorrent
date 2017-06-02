#VERSION: 2.02
#AUTHORS: Christophe Dumez (chris@qbittorrent.org)
#         Douman (custparasite@gmx.se)

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
from helpers import retrieve_url, download_file
from HTMLParser import HTMLParser
from re import compile as re_compile

class legittorrents(object):
    url = 'http://www.legittorrents.info'
    name = 'Legit Torrents'
    supported_categories = {'all': '0', 'movies': '1', 'tv': '13', 'music': '2', 'games': '3', 'anime': '5', 'books': '6'}

    def download_torrent(self, info):
        print(download_file(info))

    class MyHtmlParseWithBlackJack(HTMLParser):
        """ Parser class """
        def __init__(self, url):
            HTMLParser.__init__(self)
            self.url = url
            self.current_item = None
            self.save_item_key = None

        def handle_starttag(self, tag, attrs):
            """ Parser's start tag handler """
            if self.current_item:
                params = dict(attrs)
                if tag == "a":
                    link = params["href"]
                    if link.startswith("index") and "title" in params:
                        #description link
                        self.current_item["name"] = params["title"][14:]
                        self.current_item["desc_link"] = "/".join((self.url, link))
                    elif link.startswith("download"):
                        self.current_item["link"] = "/".join((self.url, link))
                elif tag == "td":
                    if "width" in params and params["width"] == "30" and not "leech" in self.current_item:
                        self.save_item_key = "leech" if "seeds" in self.current_item else "seeds"

            elif tag == "tr":
                self.current_item = {}
                self.current_item["size"] = ""
                self.current_item["engine_url"] = self.url

        def handle_endtag(self, tag):
            """ Parser's end tag handler """
            if self.current_item and tag == "tr":
                if len(self.current_item) > 4:
                    prettyPrinter(self.current_item)
                self.current_item = None

        def handle_data(self, data):
            """ Parser's data handler """
            if self.save_item_key:
                self.current_item[self.save_item_key] = data.strip()
                self.save_item_key = None

    def search(self, what, cat='all'):
        """ Performs search """
        query = "".join((self.url, "/index.php?page=torrents&search=", what, "&category=", self.supported_categories.get(cat, '0'), "&active=1"))

        get_table = re_compile('(?s)<table\sclass="lista".*>(.*)</table>')
        data = get_table.search(retrieve_url(query)).group(0)
        #extract first ten pages of next results
        next_pages = re_compile('(?m)<option value="(.*)">[0-9]+</option>')
        next_pages = ["".join((self.url, page)) for page in next_pages.findall(data)[:10]]

        parser = self.MyHtmlParseWithBlackJack(self.url)
        parser.feed(data)
        parser.close()

        for page in next_pages:
            parser.feed(get_table.search(retrieve_url(page)).group(0))
            parser.close()
