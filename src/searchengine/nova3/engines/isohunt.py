#VERSION: 1.42
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
import re
from helpers import retrieve_url, download_file

class isohunt(object):
	url = 'https://isohunt.to'
	name = 'isoHunt'
	supported_categories = {'all': '', 'movies': '1', 'tv': '3', 'music': '2', 'games': '4', 'anime': '7', 'software': '5', 'pictures': '6', 'books': '9'}

	def download_torrent(self, info):
		print(download_file(info))

	def search(self, what, cat='all'):
		# Remove {} since isohunt does not seem
		# to handle those very well
		what = what.replace('{', '').replace('}', '')
		i = 1
		while True and i<11:
			res = 0
			dat = retrieve_url(self.url+'/torrents.php?ihq=%s&iht=%s&ihp=%s&ihs1=2&iho1=d'%(what, self.supported_categories[cat],i))
			# I know it's not very readable, but the SGML parser feels in pain
			section_re = re.compile('(?s)id=link.*?</tr><tr')
			torrent_re = re.compile('(?s)torrent_details/(?P<link>.*?[^/]+).*?'
			'>(?P<name>.*?)</a>.*?'
			'>(?P<size>[\d,\.]+\s+MB)</td>.*?'
			'>(?P<seeds>\d+)</td>.*?'
			'>(?P<leech>\d+)</td>')
			for match in section_re.finditer(dat):
				txt = match.group(0)
				m = torrent_re.search(txt)
				if m:
					torrent_infos = m.groupdict()
					torrent_infos['name'] = re.sub('<.*?>', '', torrent_infos['name'])
					torrent_infos['engine_url'] = self.url
					torrent_code = torrent_infos['link']
					torrent_infos['link'] = self.url + '/download/' + torrent_code
					torrent_infos['desc_link'] = self.url + '/torrent_details/' + torrent_code + '/dvdrip?tab=summary'
					prettyPrinter(torrent_infos)
					res = res + 1
			if res == 0:
				break
			i = i + 1
