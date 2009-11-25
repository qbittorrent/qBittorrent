/*

Script: Parametrics.js
	Initializes the GUI property sliders.

Copyright:
	Copyright (c) 2007-2008 Greg Houston, <http://greghoustondesign.com/>.	

License:
	MIT-style license.

Requires:
	Core.js, Window.js

*/

MochaUI.extend({
	addUpLimitSlider: function(hash){
		if ($('uplimitSliderarea')) {
			var windowOptions = MochaUI.Windows.windowOptions;
			var sliderFirst = true;
			var req = new Request({
				url: '/command/getTorrentUpLimit',
				method: 'post',
				data: {hash: hash},
				onSuccess: function(data) {
					if(data){
						var up_limit = data.toInt();
						if(up_limit < 0) up_limit = 0;
						var mochaSlide = new Slider($('uplimitSliderarea'), $('uplimitSliderknob'), {
							steps: 500,
							offset: 0,
							initialStep: (up_limit/1024.).round(),
							onChange: function(pos){
								if(pos > 0) {
									$('uplimitUpdatevalue').set('html', pos);
									$('UpLimitUnit').set('html', "_(KiB/s)");
								} else {
									$('uplimitUpdatevalue').set('html', '∞');
									$('UpLimitUnit').set('html', "");
								}
							}.bind(this)
						});
						// Set default value
						if(up_limit == 0) {
							$('uplimitUpdatevalue').set('html', '∞');
							$('UpLimitUnit').set('html', "");
						} else {
							$('uplimitUpdatevalue').set('html', (up_limit/1024.).round());
							$('UpLimitUnit').set('html', "_(KiB/s)");
						}
					}
				}
			}).send();
		}
	}
});
