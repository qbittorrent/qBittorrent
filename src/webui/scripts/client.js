window.addEvent('domready', function(){
	myTable = new dynamicTable('myTable', {overCls: 'over', selectCls: 'selected', altCls: 'alt'});
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
			var request = new Json.Remote(url, {
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
								row[1] = event.name;
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
								myTable.updateRow(event.hash, row);
								break;
							case 'delete':
								myTable.removeRow(event.hash);
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
