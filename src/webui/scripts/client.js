/*
 * MIT License
 * Copyright (c) 2008 Ishan Arora <ishan@qbittorrent.org>
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
		var url = 'json/events?r='+r;
		if (!waiting){
			waiting=true;
			var request = new Request.JSON({
                                url: url,
				method: 'get',
				onComplete: function(jsonObj) {
					if(jsonObj){
						r=jsonObj.revision;
						var events=jsonObj.events;
						events.each(function(event){
							switch(event.type){
							case 'add':
								var row = new Array();
								row.length = 6;
                switch (event.state)
                {
                  case 'paused':
                      row[0] = '<img src="images/skin/paused.png"/>';
                  break;
                  case 'seeding':
                      row[0] = '<img src="images/skin/seeding.png"/>';
                  break;
                  case 'checking':
                      row[0] = '<img src="images/time.png"/>';
                  break;
                  case 'downloading':
                      row[0] = '<img src="images/skin/downloading.png"/>';
                  break;
                  case 'connecting':
                      row[0] = '<img src="images/skin/connecting.png"/>';
                  break;
                  case 'stalled':
                      row[0] = '<img src="images/skin/stalled.png"/>';
                  break;
                  case 'queued':
                      row[0] = '<img src="images/skin/queued.png"/>';
                  break;
                }
								row[1] = event.name;
                if(event.seed)
                  myTableUP.insertRow(event.hash, row);
                else
								  myTable.insertRow(event.hash, row);
								break;
							case 'modify':
								var row = new Array();
								if($defined(event.state))
								{
									switch (event.state)
									{
										case 'paused':
												row[0] = '<img src="images/skin/paused.png"/>';
										break;
										case 'seeding':
												row[0] = '<img src="images/skin/seeding.png"/>';
										break;
										case 'checking':
												row[0] = '<img src="images/time.png"/>';
										break;
										case 'downloading':
												row[0] = '<img src="images/skin/downloading.png"/>';
										break;
										case 'connecting':
												row[0] = '<img src="images/skin/connecting.png"/>';
										break;
										case 'stalled':
												row[0] = '<img src="images/skin/stalled.png"/>';
										break;
										case 'queued':
												row[0] = '<img src="images/skin/queued.png"/>';
										break;
									}
								}
								if($defined(event.size)){
									row[2] = fsize(event.size);
								}
								if($defined(event.progress))
								{
									row[3] = round1(event.progress*100) + ' %';
								}
								if($defined(event.dlspeed))
								row[4] = fspeed(event.dlspeed);
								if($defined(event.upspeed))
								row[5] = fspeed(event.upspeed);
                if(event.seed)
                  myTableUP.updateRow(event.hash, row);
                else
								  myTable.updateRow(event.hash, row);
								break;
							case 'delete':
                if(event.seed)
                  myTableUP.removeRow(event.hash);
                else
								  myTable.removeRow(event.hash);
								break;
              case 'finish':
                myTable.removeRow(event.hash);
                var row = new Array();
								row.length = 6;
								row[1] = event.name;
                myTableUP.insertRow(event.hash, row);
                break;
              case 'unfinish':
                myTableUP.removeRow(event.hash);
                var row = new Array();
								row.length = 6;
								row[1] = event.name;
                myTable.insertRow(event.hash, row);
                break;
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
