#VERSION: 1.33
#AUTHORS: Gekko Dam Beer (gekko04@users.sourceforge.net)
#CONTRIBUTORS: Christophe Dumez (chris@qbittorrent.org)
#              Bruno Barbieri (brunorex@gmail.com)

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
from urllib2 import HTTPError 
from HTMLParser import HTMLParser
import urllib
import re

class torrentreactor(object):
	url = 'http://www.torrentreactor.net'
	name = 'TorrentReactor.Net'
	supported_categories = {'all': '', 'movies': '5', 'tv': '8', 'music': '6', 'games': '3', 'anime': '1', 'software': '2'}

	def download_torrent(self, info):
		print download_file(info)

	class SimpleHTMLParser(HTMLParser):
		def __init__(self, results, url, *args):
			HTMLParser.__init__(self)
			self.td_counter = None
			self.current_item = None
			self.results = results
			self.id = None
			self.url = url
			self.dispatcher = { 'a' : self.start_a, 'td' : self.start_td }

		def handle_starttag(self, tag, attrs):
			if tag in self.dispatcher:
				self.dispatcher[tag](attrs)

		def start_a(self, attr):
			params = dict(attr)
			if re.match("/torrents/\d+.*", params['href']):
				self.current_item = {}
				self.current_item['desc_link'] = self.url+params['href'].strip()
			elif 'torrentreactor.net/download.php' in params['href']:
				self.td_counter = 0
				self.current_item['link'] = params['href'].strip()
				self.current_item['name'] = urllib.unquote_plus(params['href'].split('&')[1].split('name=')[1])

		def handle_data(self, data):
			if self.td_counter == 1:
				if not self.current_item.has_key('size'):
					self.current_item['size'] = ''
				self.current_item['size']+= data.strip()
			elif self.td_counter == 2:
				if not self.current_item.has_key('seeds'):
					self.current_item['seeds'] = ''
				self.current_item['seeds']+= data.strip()
			elif self.td_counter == 3:
				if not self.current_item.has_key('leech'):
					self.current_item['leech'] = ''
				self.current_item['leech']+= data.strip()

		def start_td(self,attr):
			if isinstance(self.td_counter,int):
				self.td_counter += 1
				if self.td_counter > 3:
					self.td_counter = None
					# add item to results
					if self.current_item:
						self.current_item['engine_url'] = self.url
						if not self.current_item['seeds'].isdigit():
							self.current_item['seeds'] = 0
						if not self.current_item['leech'].isdigit():
							self.current_item['leech'] = 0
						prettyPrinter(self.current_item)
						self.has_results = True
						self.results.append('a')

	def __init__(self):
		self.results = []
		self.parser = self.SimpleHTMLParser(self.results, self.url)

	def search(self, what, cat='all'):
		i = 0
		dat = ''
		while True and i<11:
			results = []
			parser = self.SimpleHTMLParser(results, self.url)

			try:
				dat = retrieve_url(self.url+'/torrent-search/%s/%s?sort=seeders.desc&type=all&period=none&categories=%s'%(what, (i*35), self.supported_categories[cat]))
			except HTTPError:
				break

			parser.feed(dat)
			parser.close()
			if len(results) <= 0:
				break
			i += 1
