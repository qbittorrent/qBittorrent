/* -----------------------------------------------------------------

	ATTACH MOCHA LINK EVENTS
	Notes: Here is where you define your windows and the events that open them.
	If you are not using links to run Mocha methods you can remove this function.
	
	If you need to add link events to links within windows you are creating, do
	it in the onContentLoaded function of the new window.

   ----------------------------------------------------------------- */

function attachMochaLinkEvents(){
	
	function addClickEvent(el, fn){
		['Link','Button'].each(function(item) {
			if ($(el+item)){
				$(el+item).addEvent('click', fn);
			}
		});
	}
	
	addClickEvent('download', function(e){
		new Event(e).stop();
		document.mochaUI.newWindow({
			id: 'downloadPage',
			title: 'Download from URLs',
			loadMethod: 'iframe',
			contentURL:'download.html',
			scrollbars: false,
			resizable: false,
			maximizable: false,
			paddingVertical: 0,
			paddingHorizontal: 0,
			width: 500,
			height: 270
		});
	});
	
	addClickEvent('upload', function(e){
		new Event(e).stop();
		document.mochaUI.newWindow({
			id: 'uploadPage',
			title: 'Upload torrent file',
			loadMethod: 'iframe',
			contentURL:'upload.html',
			scrollbars: false,
			resizable: false,
			maximizable: false,
			paddingVertical: 0,
			paddingHorizontal: 0,
			width: 500,
			height: 120
		});
	});

	addClickEvent('delete', function(e){
		new Event(e).stop();
		var h = myTable.selectedIds();
		if(h.length && confirm('Are you sure you want to delete the selected item in download list?')) {
			h.each(function(item, index){
				new Request({url: '/command/delete', method: 'post', data: {hash: item}}).send();
			});
		}
	});

	['pause','resume'].each(function(item) {
		addClickEvent(item, function(e){
			new Event(e).stop();
			var h = myTable.selectedIds();
			if(h.length){
				h.each(function(hash, index){
				  new Request({url: '/command/'+item, method: 'post', data: {hash: hash}}).send();
				});
			}
		});
		
		addClickEvent(item+'All', function(e){
			new Event(e).stop();
			new Request({url: '/command/'+item+'all'}).send();
		});
	});
	
	addClickEvent('bug', function(e){
		new Event(e).stop();
		document.mochaUI.newWindow({
			id: 'bugPage',
			title: 'Report a Bug',
			loadMethod: 'iframe',
			contentURL: 'http://bugs.qbittorrent.org/',
			width: 650,
			height: 400,
		});
	});
	
	addClickEvent('site', function(e){
		new Event(e).stop();
		document.mochaUI.newWindow({
			id: 'sitePage',
			title: 'qBittorrent Website',
			loadMethod: 'iframe',
			contentURL: 'http://www.qbittorrent.org/',
			width: 650,
			height: 400,
		});
	});
	
	addClickEvent('docs', function(e){
		new Event(e).stop();
		document.mochaUI.newWindow({
			id: 'docsPage',
			title: 'qBittorrent official wiki',
			loadMethod: 'iframe',
			contentURL: 'http://wiki.qbittorrent.org/',
			width: 650,
			height: 400,
		});
	});
	
	addClickEvent('about', function(e){
		new Event(e).stop();
		document.mochaUI.newWindow({
			id: 'aboutpage',
			title: 'About',
			loadMethod: 'iframe',
			contentURL: 'about.html',
			width: 650,
			height: 400,
		});
	});
	
	// Deactivate menu header links
	$$('a.returnFalse').each(function(el){
		el.addEvent('click', function(e){
			new Event(e).stop();
		});
	});
}
