#VERSION: 2.31
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

class btjunkie(object):
  url = 'http://btjunkie.org'
  name = 'btjunkie'
  supported_categories = {'all': '0', 'movies': '6', 'tv': '4', 'music': '1', 'games': '2', 'anime': '7', 'software': '3'}

  def __init__(self):
    self.results = []
    self.parser = self.SimpleSGMLParser(self.results, self.url)

  def download_torrent(self, info):
    print(download_file(info))

  class SimpleSGMLParser(sgmllib3.SGMLParser):
    def __init__(self, results, url, *args):
      sgmllib3.SGMLParser.__init__(self)
      self.url = url
      self.th_counter = None
      self.current_item = None
      self.results = results
      
    def start_a(self, attr):
      params = dict(attr)
      #print params
      if 'href' in params:
        if params['href'].startswith("http://dl.btjunkie.org/torrent"):
          self.current_item = {}
          self.th_counter = 0
          self.current_item['link']=params['href'].strip()
        elif self.th_counter == 0 and params['href'].startswith("/torrent/") and params['href'].find('/files/') == -1:
          self.current_item['desc_link'] = 'http://btjunkie.org'+params['href'].strip()
    
    def handle_data(self, data):
      if self.th_counter == 0:
        if 'name' not in self.current_item:
          self.current_item['name'] = ''
        self.current_item['name']+= data.strip()
      elif self.th_counter == 3:
        if 'size' not in self.current_item:
          self.current_item['size'] = ''
        self.current_item['size']+= data.strip()
      elif self.th_counter == 5:
        if 'seeds' not in self.current_item:
          self.current_item['seeds'] = ''
        self.current_item['seeds']+= data.strip()
      elif self.th_counter == 6:
        if 'leech' not in self.current_item:
          self.current_item['leech'] = ''
        self.current_item['leech']+= data.strip()
      
    def start_th(self,attr):
        if isinstance(self.th_counter,int):
          self.th_counter += 1
          if self.th_counter > 6:
            self.th_counter = None
            # Display item
            if self.current_item:
              self.current_item['engine_url'] = self.url
              if not self.current_item['seeds'].isdigit():
                self.current_item['seeds'] = 0
              if not self.current_item['leech'].isdigit():
                self.current_item['leech'] = 0
              prettyPrinter(self.current_item)
              self.results.append('a')

  def search(self, what, cat='all'):
    # Remove {} since btjunkie does not seem
    # to handle those very well
    what = what.replace('{', '').replace('}', '')
    ret = []
    i = 1
    while True and i<11:
      results = []
      parser = self.SimpleSGMLParser(results, self.url)
      dat = retrieve_url(self.url+'/search?q=%s&c=%s&o=52&p=%d'%(what, self.supported_categories[cat], i))
      # Remove <font> tags from page
      p = re.compile( '<[/]?font.*?>')
      dat = p.sub('', dat)
      #print dat
      #return
      results_re = re.compile('(?s)class="tab_results">.*')
      for match in results_re.finditer(dat):
        res_tab = match.group(0)
        parser.feed(res_tab)
        parser.close()
        break
      if len(results) <= 0:
        break
      i += 1
      
