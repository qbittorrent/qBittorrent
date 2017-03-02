#VERSION: 3.10
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

from xml.dom import minidom
#qBt
from novaprinter import prettyPrinter
from helpers import download_file, retrieve_url

class extratorrent(object):
    """ Search engine class """
    url = 'https://extra.to'
    name = 'ExtraTorrent'
    supported_categories = {'all'       : '0',
                            'movies'    : '4',
                            'tv'        : '8',
                            'music'     : '5',
                            'games'     : '3',
                            'anime'     : '1',
                            'software'  : '7',
                            'books'     : '2',
                            'pictures'  : '6'}

    def download_torrent(self, info):
        """ Downloader """
        print(download_file(info))

    def search(self, what, cat="all"):
        """ Performs search """
        query = "".join((self.url, "/rss.xml?type=search&search=", what, "&cid=", self.supported_categories[cat]))
        response = retrieve_url(query)

        xmldoc = minidom.parseString(response)
        itemlist = xmldoc.getElementsByTagName('item')
        for item in itemlist:
            current_item = current_item = {"engine_url" : self.url}
            current_item['name'] = item.getElementsByTagName('title')[0].childNodes[0].data
            current_item["link"] = item.getElementsByTagName('enclosure')[0].attributes['url'].value.replace('extratorrent.cc', 'extra.to')
            current_item["desc_link"] = item.getElementsByTagName('link')[0].childNodes[0].data.replace('extratorrent.cc', 'extra.to')
            current_item["size"] = item.getElementsByTagName('size')[0].childNodes[0].data
            current_item["leech"] = item.getElementsByTagName('leechers')[0].childNodes[0].data
            if not current_item["leech"].isdigit():
                current_item["leech"] = ''
            current_item["seeds"] = item.getElementsByTagName('seeders')[0].childNodes[0].data
            if not current_item["seeds"].isdigit():
                current_item["seeds"] = ''

            prettyPrinter(current_item)

        return
