var ProgressBar = new Class({
	initialize: function(value, parameters) {
		var vals = {
			'id': 'progressbar_' + (ProgressBars++),
			'value': $pick(value, 0),
			'width': 0,
			'height': 0,
			'darkbg': '#006',
			'darkfg': '#fff',
			'lightbg': '#fff',
			'lightfg': '#000'
		};
		if (parameters && $type(parameters) == 'object') $extend(vals, parameters);
		if (vals.height < 12) vals.height = 12;
		var obj = new Element('div', {
			'id': vals.id,
			'class': 'progressbar_wrapper',
			'styles': {
				'border': '1px solid #000',
				'width': vals.width,
				'height': vals.height,
				'position': 'relative',
				'margin': '0 auto'
			}
		});
		obj.vals = vals;
		obj.vals.value = $pick(value, 0); // Fix by Chris
		obj.vals.dark = new Element('div', {
			'id': vals.id + '_dark',
			'class': 'progressbar_dark',
			'styles': {
				'width': vals.width,
				'height': vals.height,
				'background': vals.darkbg,
				'color': vals.darkfg,
				'position': 'absolute',
				'text-align': 'center',
				'left': 0,
				'top': 0,
				'line-height': vals.height
			}
		});
		obj.vals.light = new Element('div', {
			'id': vals.id + '_light',
			'class': 'progressbar_light',
			'styles': {
				'width': vals.width,
				'height': vals.height,
				'background': vals.lightbg,
				'color': vals.lightfg,
				'position': 'absolute',
				'text-align': 'center',
				'left': 0,
				'top': 0,
				'line-height': vals.height
			}
		});
		obj.appendChild(obj.vals.dark);
		obj.appendChild(obj.vals.light);
		obj.getValue = ProgressBar_getValue;
		obj.setValue = ProgressBar_setValue;
		if (vals.width) obj.setValue(vals.value);
		else setTimeout('ProgressBar_checkForParent("' + obj.id + '")', 1);
		return obj;
	}
});

function ProgressBar_getValue() {
	return this.vals.value;
}

function ProgressBar_setValue(value) {
	value = parseFloat(value);
	if (isNaN(value)) value = 0;
	if (value > 100) value = 100;
	if (value < 0) value = 0;
	this.vals.value = value;
	this.vals.dark.empty();
	this.vals.light.empty();
	this.vals.dark.appendText(value + '%');
	this.vals.light.appendText(value + '%');
	var r = parseInt(this.vals.width * (value / 100));
	this.vals.dark.setStyle('clip', 'rect(0,' + r + 'px,' + this.vals.height + 'px,0)');
	this.vals.light.setStyle('clip', 'rect(0,' + this.vals.width + 'px,' + this.vals.height + 'px,' + r + 'px)');
}

function ProgressBar_checkForParent(id) {
	var obj = $(id);
	if (!obj) return;
	if (!obj.parentNode) return setTimeout('ProgressBar_checkForParent("' + id + '")', 1);
	obj.setStyle('width', '100%');
	var w = obj.offsetWidth;
	obj.vals.dark.setStyle('width', w);
	obj.vals.light.setStyle('width', w);
	obj.vals.width = w;
	obj.setValue(obj.vals.value);
}

var ProgressBars = 0;