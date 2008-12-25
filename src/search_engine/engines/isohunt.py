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
import re
import urllib

class isohunt(object):
	url = 'http://isohunt.com'
	name = 'isoHunt'

	def search(self, what):
		i = 1
		while True and i<11:
			res = 0
			dat = urllib.urlopen(self.url+'/torrents.php?ihq=%s&ihp=%s&ihs1=2&iho1=d'%(what,i)).read().decode('utf8', 'replace')
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
					torrent_infos['link'] = 'http://isohunt.com/download/'+torrent_infos['link']
					prettyPrinter(torrent_infos)
					res = res + 1
			if res == 0:
				break
			i = i + 1
