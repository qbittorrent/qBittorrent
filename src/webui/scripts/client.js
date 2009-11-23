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

myTable = new dynamicTable();
ajaxfn = function(){};

window.addEvent('domready', function(){
  MochaUI.Desktop = new MochaUI.Desktop();
  MochaUI.Desktop.desktop.setStyles({
	'background': '#fff',
	'visibility': 'visible'
  });
  initializeWindows();
  myTable.setup('myTable', 4);
  var r=0;
  var waiting=false;
  var stateToImg = function(state){
    switch (state)
    {
      case 'pausedUP':
      case 'pausedDL':
          return '<img src="images/skin/paused.png"/>';
      case 'seeding':
      case 'stalledUP':
          return '<img src="images/skin/seeding.png"/>';
      case 'checkingUP':
      case 'checkingDL':
          return '<img src="images/oxygen/run-build.png"/>';
      case 'downloading':
          return '<img src="images/skin/downloading.png"/>';
      case 'stalledDL':
          return '<img src="images/skin/stalled.png"/>';
      case 'queuedUP':
      case 'queuedDL':
          return '<img src="images/oxygen/mail-queue.png"/>';
      default:
	  return '<img src="images/skin/stalled.png"/>';
    }
    return '';
  };
	var round1 = function(val){return Math.round(val*10)/10};
	var ajaxfn = function(){
		var queueing_enabled = false;
		var url = 'json/events';
		if (!waiting){
			waiting=true;
			var request = new Request.JSON({
                                url: url,
				method: 'get',
				onFailure: function() {
					$('error_div').set('html', 'qBittorrent client is not reachable');
					waiting=false;
					ajaxfn.delay(2000);
				},
				onSuccess: function(events) {
					 $('error_div').set('html', '');
					if(events){
            // Add new torrents or update them
            torrent_hashes = myTable.getRowIds();
            events_hashes = new Array();
            events.each(function(event){
              events_hashes[events_hashes.length] = event.hash;
                var row = new Array();
                row.length = 10;
                row[0] = stateToImg(event.state);
                row[1] = event.name;
		row[2] = event.priority
                row[3] = event.size;
                row[4] = round1(event.progress*100);
		row[5] = event.num_seeds;
		row[6] = event.num_leechs;
                row[7] = event.dlspeed;
                row[8] = event.upspeed;
		row[9] = event.eta;
		row[10] = event.ratio;
		if(row[2] != "*")
			queueing_enabled = true;
               if(!torrent_hashes.contains(event.hash)) {
                  // New unfinished torrent
                  torrent_hashes[torrent_hashes.length] = event.hash;
		  //alert("Inserting row");
                  myTable.insertRow(event.hash, row);
                } else {
                  // Update torrent data
                  myTable.updateRow(event.hash, row, event.state);
                }
            });
            // Remove deleted torrents
            torrent_hashes.each(function(hash){
              if(!events_hashes.contains(hash)) {
                myTable.removeRow(hash);
              }
            });
	    if(queueing_enabled) {
		$('queueingButtons').removeClass('invisible');
		myTable.showPriority();
	    } else {
		$('queueingButtons').addClass('invisible');
		myTable.hidePriority();
	    }
					}
					waiting=false;
					ajaxfn.delay(1000);
				}
			}).send();
		}
	};
	ajaxfn();
// 	ajaxfn.periodical(5000);

	setFilter = function(f) {
	  // Visually Select the right filter
	  $("all_filter").removeClass("selectedFilter");
	  $("downloading_filter").removeClass("selectedFilter");
	  $("completed_filter").removeClass("selectedFilter");
	  $("active_filter").removeClass("selectedFilter");
	  $("inactive_filter").removeClass("selectedFilter");
	  $(f+"_filter").addClass("selectedFilter");
	  myTable.setFilter(f);
	  ajaxfn();
	}

});

function closeWindows() {
  MochaUI.closeAll();
}

// This runs when a person leaves your page.

window.addEvent('unload', function(){
	if (MochaUI) MochaUI.garbageCleanUp();
});

window.addEvent('keydown', function(event){
  if (event.key == 'a' && event.control) {
    event.stop();
    myTable.selectAll();
  }
});
