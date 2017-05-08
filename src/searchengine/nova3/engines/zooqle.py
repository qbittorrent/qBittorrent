#VERSION: 1.11
#AUTHORS: Kanishk Singh (https://github.com/ArionMiles/)
#CONTRIBUTORS: affaff (https://github.com/affaff)

# Copyright (c) 2017 Kanishk Singh

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


from xml.dom import minidom
from novaprinter import prettyPrinter
user_agent = 'Mozilla/5.0 (X11; Linux i686; rv:38.0) Gecko/20100101 Firefox/38.0'
headers    = {'User-Agent': user_agent}

from io import StringIO
import gzip
try:
    from urllib2 import urlopen, Request, URLError
except ImportError:
    from urllib.request import urlopen, Request, URLError
    

def retrieve_url_nodecode(url):
    """ Return the content of the url page as a string """
    req = Request(url, headers = headers)
    try:
        response = urlopen(req)
    except URLError as errno:
        print(" ".join(("Connection error:", str(errno.reason))))
        print(" ".join(("URL:", url)))
        return ""
    dat = response.read()
    # Check if it is gzipped
    if dat[:2] == '\037\213':
        # Data is gzip encoded, decode it
        compressedstream = StringIO(dat)
        gzipper = gzip.GzipFile(fileobj=compressedstream)
        extracted_data = gzipper.read()
        dat = extracted_data
        return dat
    return dat

class zooqle(object):
    """ Search engine class """
    url = 'https://zooqle.com'
    name = 'Zooqle'
    supported_categories = {'all'       : 'all',
                            'movies'    : 'Movies',
                            'tv'        : 'TV',
                            'music'     : 'Music',
                            'games'     : 'Games',
                            'anime'     : 'Anime',
                            'software'  : 'Apps',
                            'books'     : 'Books',
                            'others'  : 'Other'}
    def search(self, what, cat="all"):
        """ Performs search """
        page = 1
        while page < 11:
            query = "".join((self.url, "/search?q=", what, "+category%3A", self.supported_categories[cat], "&fmt=rss"))
            if( page>1 ):
                query = query + "&pg=" + str (page)
            response = retrieve_url_nodecode(query)
            xmldoc = minidom.parseString(response)
            itemlist = xmldoc.getElementsByTagName('item')
            if( len(itemlist ) ==0):
                return
            for item in itemlist:
                zooqle_dict = zooqle_dict = {"engine_url" : self.url}
                zooqle_dict['name'] = item.getElementsByTagName('title')[0].childNodes[0].data
                zooqle_dict["size"] = item.getElementsByTagName('enclosure')[0].attributes['length'].childNodes[0].data
                if( zooqle_dict["size"]=='0'):
                    zooqle_dict["link"] = item.getElementsByTagName('torrent:magnetURI')[0].childNodes[0].data
                else:
                    zooqle_dict["link"] = item.getElementsByTagName('enclosure')[0].attributes['url'].value                    
                zooqle_dict["desc_link"] = item.getElementsByTagName('link')[0].childNodes[0].data
                zooqle_dict["leech"] = item.getElementsByTagName('torrent:peers')[0].childNodes[0].data
                if not zooqle_dict["leech"].isdigit():
                    zooqle_dict["leech"] = ''
                zooqle_dict["seeds"] = item.getElementsByTagName('torrent:seeds')[0].childNodes[0].data
                if not zooqle_dict["seeds"].isdigit():
                    zooqle_dict["seeds"] = ''
                prettyPrinter(zooqle_dict)
            totalResultVal  = xmldoc.getElementsByTagName('opensearch:totalResults')[0].childNodes[0].data
            startIndex  = xmldoc.getElementsByTagName('opensearch:startIndex')[0].childNodes[0].data
            itemsPerPage = xmldoc.getElementsByTagName('opensearch:itemsPerPage')[0].childNodes[0].data
            if( ( int(startIndex)  + int(itemsPerPage) > int( totalResultVal ))):
                return
            page += 1
        return
