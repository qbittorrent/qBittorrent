

/**************************************************************

	Script		: Dynamic Table
	Version		: 0.4
	Authors		: Ishan Arora
	Desc			: Programable sortable table
	Licence		: Open Source MIT Licence

**************************************************************/

var dynamicTable = new Class	({
	
	initialize: function(table, options){
		this.setOptions({
			overCls: false,
			selectCls: false,
			altCls: false
		}, options);
		this.table = $(table);
		this.rows = new Object();
		this.cur = false;
	},

	altRow: function()
	{
		var trs = this.table.getElements('tr');
		if(this.options.altCls)
		{
			trs.each(function(el,i){
				if(i % 2){
					el.addClass(this.options.altCls);
				}else{
					el.removeClass(this.options.altCls);
				}
			}.bind(this));
		}
	},

	insertRow: function(id, row){
		this.removeRow(id);
		var tr = new Element('tr');
		this.rows[id] = tr;
		for(var i=0; i<row.length; i++)
		{
			var td = new Element('td');
			td.setHTML(row[i]);
			td.injectInside(tr);
		};

		if(this.options.overCls){
			tr.addEvent('mouseover', function(){
				tr.addClass(this.options.overCls);
			}.bind(this));
			tr.addEvent('mouseout', function(){
				tr.removeClass(this.options.overCls);
			}.bind(this));
		}
		tr.addEvent('click', function(){
			var temptr = this.rows[this.cur];
			if(temptr){
				temptr.removeClass(this.options.selectCls);
			}
			temptr = this.rows[id];
			if(temptr){
				temptr.addClass(this.options.selectCls);
			}
			this.cur = id;
		}.bind(this));

		tr.injectInside(this.table);
		this.altRow();
	},

	updateRow: function(id, row){
		var tr = this.rows[id];
		if($defined(tr))
		{
			var tds = tr.getElements('td');
			row.each(function(el, i){
				tds[i].setHTML(el);
			});
			return true;
		}
		return false;
	},

	removeRow: function(id){
		if(this.cur === id)
		{
			this.cur = false;
		}
		var tr = this.rows[id];
		if($defined(tr))
		{
			tr.remove();
			this.altRow();
			return true;
		}
		return false;
	},

	selectedId: function(){
		return this.cur;
	},
	
	getRowIds: function(){
		var ids = new Array();
		var i = 0;
		for (var id in this.rows) {
			ids[i] = id;
			i++
		}
		return ids;
	}
});
dynamicTable.implement(new Options);
dynamicTable.implement(new Events);

/*************************************************************/
