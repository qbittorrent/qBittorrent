#VERSION: 1.01
#AUTHORS: Gekko Dam Beer (gekko04@users.sourceforge.net)

# BSD License
# Copyright (c) 2006, Gekko Dam Beer <gekko04@users.sourceforge.net>
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
#     * Neither the name of the <ORGANIZATION> nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH 
# DAMAGE.

from novaprinter import prettyPrinter
import sgmllib
import urllib

class isohunt(object):
	url = 'http://isohunt.com'
	name = 'isoHunt'

	class SimpleSGMLParser(sgmllib.SGMLParser):
		def __init__(self, results, url, *args):
			sgmllib.SGMLParser.__init__(self)
			self.td_counter = None
			self.current_item = None
			self.results = results
			self.url = url

		def start_tr(self, attr):
			params = dict(attr)
			if 'onclick' in params:
				Durl='http://isohunt.com/download'
				self.current_item = {}
				self.td_counter = 0
				try:
					self.current_item['link'] = '%s/%s'%(Durl, params['onclick'].split('/')[2])
				except IndexError:
					self.current_item['link'] = None

		def handle_data(self, data):
			if self.td_counter == 3:
				if not self.current_item.has_key('name'):
					self.current_item['name'] = ''
				self.current_item['name']+= data.strip()
			if self.td_counter == 4:
				if not self.current_item.has_key('size'):
					self.current_item['size'] = ''
				self.current_item['size']+= data.strip()
			if self.td_counter == 5:
				if not self.current_item.has_key('seeds'):
					self.current_item['seeds'] = ''
				self.current_item['seeds']+= data.strip()
			if self.td_counter == 6:
				if not self.current_item.has_key('leech'):
					self.current_item['leech'] = ''
				self.current_item['leech']+= data.strip()

		def start_td(self,attr):
			if isinstance(self.td_counter,int):
				self.td_counter += 1
				if self.td_counter > 7:
					self.td_counter = None
					# add item to results
					if self.current_item:
						self.current_item['engine_url'] = self.url
						if not self.current_item.has_key('seeds') or not self.current_item['seeds'].isdigit():
							self.current_item['seeds'] = 0
						if not self.current_item.has_key('leech') or not self.current_item['leech'].isdigit():
							self.current_item['leech'] = 0
						if self.current_item['link'] is not None:
							prettyPrinter(self.current_item)
							self.results.append('a')

	def __init__(self):
		self.results = []
		self.parser = self.SimpleSGMLParser(self.results, self.url)

	def search(self, what):
		i = 1
		while True and i<11:
			results = []
			parser = self.SimpleSGMLParser(results, self.url)
			dat = urllib.urlopen(self.url+'/torrents.php?ihq=%s&ihp=%s'%(what,i)).read().decode('utf-8', 'replace')
			parser.feed(dat)
			parser.close()
			if len(results) <= 0:
				break
			i += 1
