/*
 * MIT License
 * Copyright (c) 2008 Ishan Arora <ishan@qbittorrent.org>,
 * Christophe Dumez <chris@qbittorrent.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

window.addEvent('domready', function(){
  MochaUI.Desktop = new MochaUI.Desktop();
  MochaUI.Desktop.desktop.setStyles({
	'background': '#fff',
	'visibility': 'visible'
  });
  initializeWindows();
  // Tabs
  myTabs1 = new mootabs('myTabs', {
      width: '100%',
      height: '100%'
  });
  // Download list
	myTable = new dynamicTable('myTable', {overCls: 'over', selectCls: 'selected', altCls: 'alt'});
  myTableUP = new dynamicTable('myTableUP', {overCls: 'over', selectCls: 'selected', altCls: 'alt'});
	var r=0;
	var waiting=false;
  var stateToImg = function(state){
    switch (state)
    {
      case 'paused':
          return '<img src="images/skin/paused.png"/>';
      case 'seeding':
          return '<img src="images/skin/seeding.png"/>';
      case 'checking':
          return '<img src="images/time.png"/>';
      case 'downloading':
          return '<img src="images/skin/downloading.png"/>';
      case 'stalled':
          return '<img src="images/skin/stalled.png"/>';
      case 'queued':
          return '<img src="images/skin/queued.png"/>';
    }
    return '';
  };
	var round1 = function(val){return Math.round(val*10)/10};
	var fspeed = function(val){return round1(val/1024) + ' KiB/s';};
	var fsize = function(val){
		var units = ['B', 'KiB', 'MiB', 'GiB'];
		for(var i=0; i<5; i++){
			if (val < 1024) {
				return round1(val) + ' ' + units[i];
			}
			val /= 1024;
		}
		return round1(val) + ' TiB';
	};
	var ajaxfn = function(){
		var url = 'json/events';
		if (!waiting){
			waiting=true;
			var request = new Request.JSON({
                                url: url,
				method: 'get',
				onComplete: function(events) {
					if(events){
            // Add new torrents or update them
            unfinished_hashes = myTable.getRowIds();
            finished_hashes = myTableUP.getRowIds();
            events_hashes = new Array();
            events.each(function(event){
              events_hashes[events_hashes.length] = event.hash;
              if(event.seed) {
                var row = new Array();
                row.length = 4;
                row[0] = '<img src="images/skin/seeding.png"/>';
                row[1] = event.name;
                row[2] = fsize(event.size);
                row[3] = fspeed(event.upspeed);
                if(!finished_hashes.contains(event.hash)) {
                  // New finished torrent
                  finished_hashes[finished_hashes.length] = event.hash;
                  myTableUP.insertRow(event.hash, row);
                  if(unfinished_hashes.contains(event.hash)) {
                    // Torrent used to be in unfinished list
                    // Remove it
                    myTable.removeRow(event.hash);
                    unfinished_hashes.erase(event.hash);
                  }
                } else {
                  // Update torrent data
                  myTableUP.updateRow(event.hash, row);
                }
              } else {
                var row = new Array();
                row.length = 6;
                row[0] = stateToImg(event.state);
                row[1] = event.name;
                row[2] = fsize(event.size);
                row[3] = round1(event.progress*100) + ' %';
                row[4] = fspeed(event.dlspeed);
                row[5] = fspeed(event.upspeed);
               if(!unfinished_hashes.contains(event.hash)) {
                  // New unfinished torrent
                  unfinished_hashes[unfinished_hashes.length] = event.hash;
                  myTable.insertRow(event.hash, row);
                  if(finished_hashes.contains(event.hash)) {
                    // Torrent used to be in unfinished list
                    // Remove it
                    myTableUP.removeRow(event.hash);
                    finished_hashes.erase(event.hash);
                  }
                } else {
                  // Update torrent data
                  myTable.updateRow(event.hash, row);
                }
              }
            });
						// Remove deleted torrents
            unfinished_hashes.each(function(hash){
              if(!events_hashes.contains(hash)) {
                myTable.removeRow(hash);
              }
            });
            finished_hashes.each(function(hash){
              if(!events_hashes.contains(hash)) {
                myTableUP.removeRow(hash);
              }
            });
					}
					waiting=false;
					ajaxfn.delay(1000);
				}
			}).send();
		}
	};
	ajaxfn();
// 	ajaxfn.periodical(5000);
});

// This runs when a person leaves your page.

window.addEvent('unload', function(){
	if (MochaUI) MochaUI.garbageCleanUp();
});

window.addEvent('keydown', function(event){
  if (event.key == 'a' && event.control) {
    event.stop();
    if($("Tab1").hasClass('active')) {
      myTable.selectAll();
    } else {
      myTableUP.selectAll();
    }
  }
});
