#VERSION: 1.1
#AUTHORS: Christophe Dumez (chris@qbittorrent.org)

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
from helpers import retrieve_url
import io, gzip, urllib.request, urllib.error, urllib.parse, tempfile
import sgmllib3
import re
import os

class torrentdownloads(object):
  url = 'http://www.torrentdownloads.net'
  name = 'TorrentDownloads'
  supported_categories = {'all': '0', 'movies': '4', 'tv': '8', 'music': '5', 'games': '3', 'anime': '1', 'software': '7', 'books': '2'}

  def __init__(self):
    self.results = []
    #self.parser = self.SimpleSGMLParser(self.results, self.url)

  def download_torrent(self, url):
    """ Download file at url and write it to a file, return the path to the file and the url """
    file, path = tempfile.mkstemp()
    file = os.fdopen(file, "w")
    # Download url
    req = urllib.request.Request(url)
    response = urllib.request.urlopen(req)
    dat = response.read()
    # Check if it is gzipped
    if dat[:2] == '\037\213':
        # Data is gzip encoded, decode it
        compressedstream = io.StringIO(dat)
        gzipper = gzip.GzipFile(fileobj=compressedstream)
        extracted_data = gzipper.read()
        dat = extracted_data
        
    # Write it to a file
    file.write(dat.strip())
    file.close()
    # return file path
    print(path+" "+url)

  class SimpleSGMLParser(sgmllib3.SGMLParser):
    def __init__(self, results, url, what, *args):
      sgmllib3.SGMLParser.__init__(self)
      self.url = url
      self.li_counter = None
      self.current_item = None
      self.results = results
      self.what = what.upper().split('+')
      if len(self.what) == 0:
        self.what = None
      
    def start_a(self, attr):
      params = dict(attr)
      #print params
      if 'href' in params and params['href'].startswith("http://www.torrentdownloads.net/torrent/"):
        self.current_item = {}
        self.li_counter = 0
        self.current_item['desc_link'] = params['href'].strip()
        self.current_item['link']=params['href'].strip().replace('/torrent', '/download', 1)
    
    def handle_data(self, data):
      if self.li_counter == 0:
        if 'name' not in self.current_item:
          self.current_item['name'] = ''
        self.current_item['name']+= data
      elif self.li_counter == 1:
        if 'size' not in self.current_item:
          self.current_item['size'] = ''
        self.current_item['size']+= data.strip().replace("&nbsp;", " ")
      elif self.li_counter == 2:
        if 'seeds' not in self.current_item:
          self.current_item['seeds'] = ''
        self.current_item['seeds']+= data.strip()
      elif self.li_counter == 3:
        if 'leech' not in self.current_item:
          self.current_item['leech'] = ''
        self.current_item['leech']+= data.strip()
      
    def start_li(self,attr):
        if isinstance(self.li_counter,int):
          self.li_counter += 1
          if self.li_counter > 3:
            self.li_counter = None
            # Display item
            if self.current_item:
              self.current_item['engine_url'] = self.url
              if not self.current_item['seeds'].isdigit():
                self.current_item['seeds'] = 0
              if not self.current_item['leech'].isdigit():
                self.current_item['leech'] = 0
              # Search should use AND operator as a default
              tmp = self.current_item['name'].upper();
              if self.what is not None:
                for w in self.what:
                  if tmp.find(w) < 0: return
              prettyPrinter(self.current_item)
              self.results.append('a')

  def search(self, what, cat='all'):
    ret = []
    i = 1
    while i<11:
      results = []
      parser = self.SimpleSGMLParser(results, self.url, what)
      dat = retrieve_url(self.url+'/search/?page=%d&search=%s&s_cat=%s&srt=seeds&pp=50&order=desc'%(i, what, self.supported_categories[cat]))
      results_re = re.compile('(?s)<div class="torrentlist">.*')
      for match in results_re.finditer(dat):
        res_tab = match.group(0)
        parser.feed(res_tab)
        parser.close()
        break
      if len(results) <= 0:
        break
      i += 1
      
