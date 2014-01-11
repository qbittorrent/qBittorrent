#VERSION: 1.53
#AUTHORS: Fabien Devaux (fab@gnux.info)
#CONTRIBUTORS: Christophe Dumez (chris@qbittorrent.org)

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
import sgmllib3
from helpers import retrieve_url, download_file

PREVIOUS_IDS = set()

class piratebay(object):
	url = 'https://thepiratebay.se'
	name = 'The Pirate Bay'
	supported_categories = {'all': '0', 'movies': '200', 'music': '100', 'games': '400', 'software': '300'}

	def __init__(self):
		self.results = []
		self.parser = self.SimpleSGMLParser(self.results, self.url)

	def download_torrent(self, info):
		print(download_file(info))

	class SimpleSGMLParser(sgmllib3.SGMLParser):
		def __init__(self, results, url, *args):
			sgmllib3.SGMLParser.__init__(self)
			self.td_counter = None
			self.current_item = None
			self.results = results
			self.url = url
			self.code = 0
			self.in_name = None

		def start_a(self, attr):
			params = dict(attr)
			if params['href'].startswith('/torrent/'):
				self.current_item = {}
				self.td_counter = 0
				self.current_item['desc_link'] = self.url + params['href'].strip()
				self.in_name = True
				self.current_item['id'] = params['href'].split('/')[2]
			elif params['href'].startswith('magnet:'):
				self.current_item['link']=params['href'].strip()
				self.in_name = False

		def handle_data(self, data):
			if self.td_counter == 0:
				if self.in_name:
					if 'name' not in self.current_item:
						self.current_item['name'] = ''
					self.current_item['name']+= data.strip()
				else:
					#Parse size
					if 'Size' in data:
						self.current_item['size'] = data[data.index("Size")+5:]
						self.current_item['size'] = self.current_item['size'][:self.current_item['size'].index(',')]
			elif self.td_counter == 1:
				if 'seeds' not in self.current_item:
					self.current_item['seeds'] = ''
				self.current_item['seeds']+= data.strip()
			elif self.td_counter == 2:
				if 'leech' not in self.current_item:
					self.current_item['leech'] = ''
				self.current_item['leech']+= data.strip()

		def start_td(self,attr):
			if isinstance(self.td_counter,int):
				self.td_counter += 1
				if self.td_counter > 3:
					self.td_counter = None
					# Display item
					if self.current_item:
						if self.current_item['id'] in PREVIOUS_IDS:
							self.results = []
							self.reset()
							return
						self.current_item['engine_url'] = self.url
						if not self.current_item['seeds'].isdigit():
							self.current_item['seeds'] = 0
						if not self.current_item['leech'].isdigit():
							self.current_item['leech'] = 0
						prettyPrinter(self.current_item)
						PREVIOUS_IDS.add(self.current_item['id'])
						self.results.append('a')
	def search(self, what, cat='all'):
		ret = []
		i = 0
		order = 'se'
		while True and i<11:
			results = []
			parser = self.SimpleSGMLParser(results, self.url)
			dat = retrieve_url(self.url+'/search/%s/%d/7/%s' % (what, i, self.supported_categories[cat]))
			parser.feed(dat)
			parser.close()
			if len(results) <= 0:
				break
			i += 1
