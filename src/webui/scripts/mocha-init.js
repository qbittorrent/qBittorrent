/* -----------------------------------------------------------------

	ATTACH MOCHA LINK EVENTS
	Notes: Here is where you define your windows and the events that open them.
	If you are not using links to run Mocha methods you can remove this function.

	If you need to add link events to links within windows you are creating, do
	it in the onContentLoaded function of the new window.

   ----------------------------------------------------------------- */

initializeWindows = function(){

	function addClickEvent(el, fn){
		['Link','Button'].each(function(item) {
			if ($(el+item)){
				$(el+item).addEvent('click', fn);
			}
		});
	}

	addClickEvent('download', function(e){
		new Event(e).stop();
		new MochaUI.Window({
			id: 'downloadPage',
			title: "_(Download from urls)",
			loadMethod: 'iframe',
			contentURL:'download.html',
			scrollbars: true,
			resizable: false,
			maximizable: false,
			closable: true,
			paddingVertical: 0,
			paddingHorizontal: 0,
			width: 500,
			height: 300
		});
	});

	addClickEvent('preferences', function(e) {
		new Event(e).stop();
		new MochaUI.Window({
			id: 'preferencesPage',
			title: "_(Options)",
			loadMethod: 'xhr',
			toolbar: true,
			contentURL: 'preferences_content.html',
			require: {
				css: ['css/Tabs.css']
			},
			toolbarURL: 'preferences.html',
			resizable: true,
			maximizable: false,
			closable: true,
			paddingVertical: 0,
			paddingHorizontal: 0,
			width: 700,
			height: 300
		});
	});

	addClickEvent('upload', function(e){
		new Event(e).stop();
		new MochaUI.Window({
			id: 'uploadPage',
			title: "_(Download local torrent)",
			loadMethod: 'iframe',
			contentURL:'upload.html',
			scrollbars: true,
			resizable: false,
			maximizable: false,
			paddingVertical: 0,
			paddingHorizontal: 0,
			width: 600,
			height: 130
		});
	});

	globalUploadLimitFN = function() {
		new MochaUI.Window({
				id: 'uploadLimitPage',
				title: "_(Global Upload Speed Limiting)",
				loadMethod: 'iframe',
				contentURL:'uploadlimit.html?hash=global',
				scrollbars: false,
				resizable: false,
				maximizable: false,
				paddingVertical: 0,
				paddingHorizontal: 0,
				width: 424,
				height: 80
			});
	}

	uploadLimitFN = function() {
		var h = myTable.selectedIds();
		if(h.length){
			var hash = h[0];
			new MochaUI.Window({
				id: 'uploadLimitPage',
				title: "_(Torrent Upload Speed Limiting)",
				loadMethod: 'iframe',
				contentURL:'uploadlimit.html?hash='+hash,
				scrollbars: false,
				resizable: false,
				maximizable: false,
				paddingVertical: 0,
				paddingHorizontal: 0,
				width: 424,
				height: 80
			});
		}
	};

	globalDownloadLimitFN = function() {
		new MochaUI.Window({
				id: 'downloadLimitPage',
				title: "_(Global Download Speed Limiting)",
				loadMethod: 'iframe',
				contentURL:'downloadlimit.html?hash=global',
				scrollbars: false,
				resizable: false,
				maximizable: false,
				paddingVertical: 0,
				paddingHorizontal: 0,
				width: 424,
				height: 80
			});
	}

	downloadLimitFN = function() {
		var h = myTable.selectedIds();
		if(h.length){
			var hash = h[0];
			new MochaUI.Window({
				id: 'downloadLimitPage',
				title: "_(Torrent Download Speed Limiting)",
				loadMethod: 'iframe',
				contentURL:'downloadlimit.html?hash='+hash,
				scrollbars: false,
				resizable: false,
				maximizable: false,
				paddingVertical: 0,
				paddingHorizontal: 0,
				width: 424,
				height: 80
			});
		}
	};

	deleteFN = function() {
		var h = myTable.selectedIds();
		/*if(h.length && confirm('_(Are you sure you want to delete the selected torrents from the transfer list?)')) {
			h.each(function(item, index){
				new Request({url: 'command/delete', method: 'post', data: {hash: item}}).send();
			});
		}*/
		if(h.length) {
                	new MochaUI.Window({
				id: 'confirmDeletionPage',
				title: "_(Deletion confirmation - qBittorrent)",
				loadMethod: 'iframe',
				contentURL:'confirmdeletion.html?hashes='+h.join(','),
				scrollbars: false,
				resizable: false,
				maximizable: false,
				padding: 10,
				width: 424,
				height: 140
			});
                }
	};

	addClickEvent('delete', function(e){
		new Event(e).stop();
		deleteFN();
	});

	pauseFN = function() {
		var h = myTable.selectedIds();
		if(h.length){
			h.each(function(hash, index){
			  new Request({url: 'command/pause', method: 'post', data: {hash: hash}}).send();
			});
		}
	};

	startFN = function() {
		var h = myTable.selectedIds();
		if(h.length){
			h.each(function(hash, index){
			  new Request({url: 'command/resume', method: 'post', data: {hash: hash}}).send();
			});
		}
	};

	recheckFN = function() {
		var h = myTable.selectedIds();
		if(h.length){
			h.each(function(hash, index){
			  new Request({url: 'command/recheck', method: 'post', data: {hash: hash}}).send();
			});
		}
	};

	['pause','resume', 'recheck'].each(function(item) {
		addClickEvent(item, function(e){
			new Event(e).stop();
			var h = myTable.selectedIds();
			if(h.length){
				h.each(function(hash, index){
				  new Request({url: 'command/'+item, method: 'post', data: {hash: hash}}).send();
				});
			}
		});

		addClickEvent(item+'All', function(e){
			new Event(e).stop();
			new Request({url: 'command/'+item+'all'}).send();
		});
	});

	['decreasePrio','increasePrio', 'topPrio', 'bottomPrio'].each(function(item) {
		addClickEvent(item, function(e){
			new Event(e).stop();
			setPriorityFN(item);
		});
	});

	setPriorityFN = function(cmd) {
		var h = myTable.selectedIds();
		if(h.length) {
			new Request({url: 'command/'+cmd, method: 'post', data: {hashes: h.join("|")}}).send();
		}
	}

	addClickEvent('bug', function(e){
		new Event(e).stop();
		new MochaUI.Window({
			id: 'bugPage',
			title: '_(Report a bug)',
			loadMethod: 'iframe',
			contentURL: 'http://bugs.qbittorrent.org/',
			width: 650,
			height: 400
		});
	});

	addClickEvent('site', function(e){
		new Event(e).stop();
		new MochaUI.Window({
			id: 'sitePage',
			title: 'qBittorrent Website',
			loadMethod: 'iframe',
			contentURL: 'http://www.qbittorrent.org/',
			width: 650,
			height: 400
		});
	});

	addClickEvent('docs', function(e){
		new Event(e).stop();
		new MochaUI.Window({
			id: 'docsPage',
			title: 'qBittorrent official wiki',
			loadMethod: 'iframe',
			contentURL: 'http://wiki.qbittorrent.org/',
			width: 650,
			height: 400
		});
	});

	addClickEvent('about', function(e){
		new Event(e).stop();
		new MochaUI.Window({
			id: 'aboutpage',
			title: 'About',
			loadMethod: 'xhr',
			contentURL: 'about.html',
			width: 650,
			height: 200,
			padding: 10
		});
	});

	addClickEvent('shutdown', function(e){
		new Event(e).stop();
		new Request({url: 'command/shutdown',
                             onSuccess: function() {
                               document.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?><!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\"><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>_(qBittorrent has been shutdown.)</title><style type=\"text/css\">body { text-align: center; }</style></head><body><h1>_(qBittorrent has been shutdown.)</h1></body></html>");
                               stop();
                             }
                            }
                ).send();
	});

	// Deactivate menu header links
	$$('a.returnFalse').each(function(el){
		el.addEvent('click', function(e){
			new Event(e).stop();
		});
	});
}
