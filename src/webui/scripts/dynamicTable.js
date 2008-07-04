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
