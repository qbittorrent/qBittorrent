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
    addUpLimitSlider: function(hashes) {
        if ($('uplimitSliderarea')) {
            var windowOptions = MochaUI.Windows.windowOptions;
            var sliderFirst = true;
            // Get global upload limit
            var maximum = 500;
            var req = new Request({
                url: 'command/getGlobalUpLimit',
                method: 'post',
                data: {},
                onSuccess: function(data) {
                    if (data) {
                        var tmp = data.toInt();
                        if (tmp > 0) {
                            maximum = tmp / 1024.
                        }
                        else {
                            if (hashes[0] == "global")
                                maximum = 10000;
                            else
                                maximum = 1000;
                        }
                    }
                    // Get torrents upload limit
                    // And create slider
                    if (hashes[0] == 'global') {
                        var up_limit = maximum;
                        if (up_limit < 0) up_limit = 0;
                        maximum = 10000;
                        var mochaSlide = new Slider($('uplimitSliderarea'), $('uplimitSliderknob'), {
                            steps: maximum,
                            offset: 0,
                            initialStep: up_limit.round(),
                            onChange: function(pos) {
                                if (pos > 0) {
                                    $('uplimitUpdatevalue').value = pos;
                                    $('upLimitUnit').style.visibility = "visible";
                                }
                                else {
                                    $('uplimitUpdatevalue').value = '∞';
                                    $('upLimitUnit').style.visibility = "hidden";
                                }
                            }.bind(this)
                        });
                        // Set default value
                        if (up_limit == 0) {
                            $('uplimitUpdatevalue').value = '∞';
                            $('upLimitUnit').style.visibility = "hidden";
                        }
                        else {
                            $('uplimitUpdatevalue').value = up_limit.round();
                            $('upLimitUnit').style.visibility = "visible";
                        }
                    }
                    else {
                        var req = new Request.JSON({
                            url: 'command/getTorrentsUpLimit',
                            noCache : true,
                            method: 'post',
                            data: {
                                hashes: hashes.join('|')
                            },
                            onSuccess: function(data) {
                                if (data) {
                                    var up_limit = data[hashes[0]];
                                    for(var key in data)
                                        if (up_limit != data[key]) {
                                            up_limit = 0;
                                            break;
                                        }
                                    if (up_limit < 0) up_limit = 0;
                                    var mochaSlide = new Slider($('uplimitSliderarea'), $('uplimitSliderknob'), {
                                        steps: maximum,
                                        offset: 0,
                                        initialStep: (up_limit / 1024.).round(),
                                        onChange: function(pos) {
                                            if (pos > 0) {
                                                $('uplimitUpdatevalue').value = pos;
                                                $('upLimitUnit').style.visibility = "visible";
                                            }
                                            else {
                                                $('uplimitUpdatevalue').value = '∞';
                                                $('upLimitUnit').style.visibility = "hidden";
                                            }
                                        }.bind(this)
                                    });
                                    // Set default value
                                    if (up_limit == 0) {
                                        $('uplimitUpdatevalue').value = '∞';
                                        $('upLimitUnit').style.visibility = "hidden";
                                    }
                                    else {
                                        $('uplimitUpdatevalue').value = (up_limit / 1024.).round();
                                        $('upLimitUnit').style.visibility = "visible";
                                    }
                                }
                            }
                        }).send();
                    }
                }
            }).send();
        }
    },

    addDlLimitSlider: function(hashes) {
        if ($('dllimitSliderarea')) {
            var windowOptions = MochaUI.Windows.windowOptions;
            var sliderFirst = true;
            // Get global upload limit
            var maximum = 500;
            var req = new Request({
                url: 'command/getGlobalDlLimit',
                method: 'post',
                data: {},
                onSuccess: function(data) {
                    if (data) {
                        var tmp = data.toInt();
                        if (tmp > 0) {
                            maximum = tmp / 1024.
                        }
                        else {
                            if (hashes[0] == "global")
                                maximum = 10000;
                            else
                                maximum = 1000;
                        }
                    }
                    // Get torrents download limit
                    // And create slider
                    if (hashes[0] == 'global') {
                        var dl_limit = maximum;
                        if (dl_limit < 0) dl_limit = 0;
                        maximum = 10000;
                        var mochaSlide = new Slider($('dllimitSliderarea'), $('dllimitSliderknob'), {
                            steps: maximum,
                            offset: 0,
                            initialStep: dl_limit.round(),
                            onChange: function(pos) {
                                if (pos > 0) {
                                    $('dllimitUpdatevalue').value = pos;
                                    $('dlLimitUnit').style.visibility = "visible";
                                }
                                else {
                                    $('dllimitUpdatevalue').value = '∞';
                                    $('dlLimitUnit').style.visibility = "hidden";
                                }
                            }.bind(this)
                        });
                        // Set default value
                        if (dl_limit == 0) {
                            $('dllimitUpdatevalue').value = '∞';
                            $('dlLimitUnit').style.visibility = "hidden";
                        }
                        else {
                            $('dllimitUpdatevalue').value = dl_limit.round();
                            $('dlLimitUnit').style.visibility = "visible";
                        }
                    }
                    else {
                        var req = new Request.JSON({
                            url: 'command/getTorrentsDlLimit',
                            noCache : true,
                            method: 'post',
                            data: {
                                hashes: hashes.join('|')
                            },
                            onSuccess: function(data) {
                                if (data) {
                                    var dl_limit = data[hashes[0]];
                                    for(var key in data)
                                        if (dl_limit != data[key]) {
                                            dl_limit = 0;
                                            break;
                                        }
                                    if (dl_limit < 0) dl_limit = 0;
                                    var mochaSlide = new Slider($('dllimitSliderarea'), $('dllimitSliderknob'), {
                                        steps: maximum,
                                        offset: 0,
                                        initialStep: (dl_limit / 1024.).round(),
                                        onChange: function(pos) {
                                            if (pos > 0) {
                                                $('dllimitUpdatevalue').value = pos;
                                                $('dlLimitUnit').style.visibility = "visible";
                                            }
                                            else {
                                                $('dllimitUpdatevalue').value = '∞';
                                                $('dlLimitUnit').style.visibility = "hidden";
                                            }
                                        }.bind(this)
                                    });
                                    // Set default value
                                    if (dl_limit == 0) {
                                        $('dllimitUpdatevalue').value = '∞';
                                        $('dlLimitUnit').style.visibility = "hidden";
                                    }
                                    else {
                                        $('dllimitUpdatevalue').value = (dl_limit / 1024.).round();
                                        $('dlLimitUnit').style.visibility = "visible";
                                    }
                                }
                            }
                        }).send();
                    }
                }
            }).send();
        }
    }
});