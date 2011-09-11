#VERSION: 1.3
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
from helpers import retrieve_url, download_file
import sgmllib3
import re

class vertor(object):
  url = 'http://www.vertor.com'
  name = 'vertor'
  supported_categories = {'all': '0', 'movies': '5', 'tv': '8', 'music': '6', 'games': '3', 'anime': '1', 'software': '2'}

  def __init__(self):
    self.results = []
    self.parser = self.SimpleSGMLParser(self.results, self.url)

  def download_torrent(self, info):
    print(download_file(info))

  class SimpleSGMLParser(sgmllib3.SGMLParser):
    def __init__(self, results, url, *args):
      sgmllib3.SGMLParser.__init__(self)
      self.url = url
      self.td_counter = None
      self.current_item = None
      self.results = results
      self.in_name = False
      self.outside_td = True;
    
    def start_tr(self, attr):
      self.td_counter = -1
      self.current_item = {}
      
    def end_tr(self):
      if self.td_counter == 5:
        self.td_counter = None
        # Display item
        if self.current_item and 'link' in self.current_item:
          self.current_item['engine_url'] = self.url
          if not self.current_item['seeds'].isdigit():
            self.current_item['seeds'] = 0
          if not self.current_item['leech'].isdigit():
            self.current_item['leech'] = 0
          prettyPrinter(self.current_item)
          self.results.append('a')

    def start_a(self, attr):
      #if self.td_counter is None or self.td_counter < 0: return
      params = dict(attr)
      if 'href' in params and params['href'].startswith("http://www.vertor.com/index.php?mod=download"):
        self.current_item['link']=params['href'].strip()
      elif self.td_counter == 0 and 'href' in params and params['href'].startswith("/torrents/") \
      and 'name' not in self.current_item:
        self.current_item['desc_link']='http://www.vertor.com'+params['href'].strip()
        self.in_name = True
        
    def end_a(self):
      if self.in_name:
        self.in_name = False
    
    def handle_data(self, data):
      if self.in_name:
        if 'name' not in self.current_item:
          self.current_item['name'] = ''
        self.current_item['name']+= data.strip()
      elif self.td_counter == 2 and not self.outside_td:
        if 'size' not in self.current_item:
          self.current_item['size'] = ''
        self.current_item['size']+= data.strip()
      elif self.td_counter == 4 and not self.outside_td:
        if 'seeds' not in self.current_item:
          self.current_item['seeds'] = ''
        self.current_item['seeds']+= data.strip()
      elif self.td_counter == 5 and not self.outside_td:
        if 'leech' not in self.current_item:
          self.current_item['leech'] = ''
        self.current_item['leech']+= data.strip()
        
    def end_td(self):
      self.outside_td = True
      
    def start_td(self,attr):
        if isinstance(self.td_counter,int):
          self.outside_td = False
          self.td_counter += 1

  def search(self, what, cat='all'):
    ret = []
    i = 0
    while True and i<11:
      results = []
      parser = self.SimpleSGMLParser(results, self.url)
      dat = retrieve_url(self.url+'/index.php?mod=search&words=%s&cid=%s&orderby=a.seeds&asc=0&search=&exclude=&p=%d'%(what, self.supported_categories[cat], i))
      results_re = re.compile('(?s)<table class="listing">.*')
      for match in results_re.finditer(dat):
        res_tab = match.group(0)
        parser.feed(res_tab)
        parser.close()
        break
      if len(results) <= 0:
        break
      i += 1
      
