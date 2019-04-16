(function () {
    var modalTemplate =
        '<div data-bind="tabs: [new ko.tab(\'Auto download\', \'rules-template\', rules), new ko.tab(\'Feeds\', \'feeds-template\', feeds)]"></div>\
 <script id="rules-template" type="text/html">\
   <select data-name="rules"></select>\
   <button data-name="addRule">Add</button>\
   <button data-name="deleteSelectedRule">Delete</button>\
   <div data-name="selectedRule">\
      <div><label><input data-name="enabled"/>Enabled</label></div>\
      <div><label>Name</label><input data-name="name" /></div>\
      <div><label>Must contain</label><input data-name="mustContain" /></div>\
      <div><label>Must NOT contain</label><input data-name="mustNotContain" /></div>\
      <div><label>Ignore days</label><select data-bind="options: $parent.days, value: ignoreDays"></select></div>\
      <div><label>Save path</label><input data-name="savePath" /><select data-bind="options: $parent.paths, value: selectedPath"></select></div>\
      <label>Feeds</label>\
      <div data-name="feeds">\
          <div><label><input data-name="enabled"/><span data-name="name"></span></label></div>\
      </div>\
      <button data-name="save">Save</button>\
   </div>\
</script>\
<script id="feeds-template" type="text/html">\
    <div data-name="feeds">\
        <h2><!--ko text: name --><!--/ko--><a data-name="anySelected" data-bind="click: downloadSelected" style="float:right;"><img class="MyMenuIcon" alt="Add Torrent Link..." src="images/qbt-theme/insert-link.svg" width="32" height="32"></a></h2>\
        <ul data-name="torrents"><li><label><input data-name="selected"></input> <!--ko text: name --><!--/ko--> </label></li></ul>\
    </div>\
</script>';
    
    var button = new Element("a", { html: "<img class='mochaToolButton' title='RSS Rules' src='images/qbt-theme/rss-config.png' alt='RSS Rules' width='24' height='24'/>" });
    button.setAttribute("data-bind", "click: showRss");
    button.setAttribute("class", "divider");

    var view =  new Element("div",  { html: "<div><div data-name='modal' data-bind='modal: modal'>" + modalTemplate + "</div></div>"});

    var feeds = null;
    var feedsUrl = new URI('api/v2/rss/items');
    feedsUrl.setData('withData', true);

    var loadFeeds = function (callback) {
        new Request.JSON({
            url: feedsUrl,
            noCache: true,
            method: 'get',
            onFailure: function () {
                //TODO: error handling
            },
            onSuccess: function (response) {
                feeds = Object.keys(response).map(function (key) { return { name: key, data: response[key] }; })
                if (callback) callback(feeds);
            }
        }).send();
    }
    loadFeeds();

    window.addEvent('domready', function() {
        button.inject("preferencesButton", "after");
        view.inject(document.body);
        ko.applyBindings(new Model(), document.body);
    });

    var Model = function () {
        this.modal = ko.observable().extend({ notify: 'always' });;
    };

    Model.prototype = {
        showRss: function () {
            if (this.modal() == null) {
                this.modal(new RssModel());
            }
        }
    };
    
    var RssModel = function () {
        this.rules = new RssRuleModel();
        this.feeds = new FeedsModel();
    };

    var FeedsModel = function() {
        this.feeds = ko.observable(feeds.map(function(f) { return new FeedDownloadsModel(f); }));
    };

    var FeedDownloadsModel = function(feed) {
        this.name = feed.name;
        this.torrents = feed.data.articles.map(a => new Torrent(a))
        this.anySelected = ko.computed(() => this.torrents.find(t => t.selected()) != null)
    };

    FeedDownloadsModel.prototype = {
        downloadSelected: function() {
            var urls = this.torrents.filter(t => t.selected()).map(t => encodeURIComponent(t.downloadLink));
            showDownloadPage(urls);
        }
    };

    var RssRuleModel = function () {
        this.rules = ko.observableArray();
        this.selectedRule = ko.observable();

        this.listRules();
        this.canDeleteSelectedRule = ko.computed(function () { return this.selectedRule() != null }, this);
        this.days = [...Array(14).keys()];
    }

    RssRuleModel.prototype = {
        listRules: function () {
            var url = new URI('api/v2/rss/rules');
            var request = new Request.JSON({
                url: url,
                noCache: true,
                method: 'get',
                onFailure: function () {
                    //TODO: error handling
                },
                onSuccess: function (response) {
                    var rules = Object.keys(response).map(function (key) { return new Rule(key, response[key]); });
                    this.paths = rules.map(function (rule) { return rule.savePath(); }).filter(function (value, index, arr) { return arr.indexOf(value) === index; });
                    this.rules(rules);
                }.bind(this)
            }).send();
        },
        addRule: function () {
            this.rules.push(new Rule("Untitled", { enabled: false, mustContain: "", mustNotContain: "", savePath: "", affectedFeeds: [] }));
        },
        deleteSelectedRule: function () {
            var rule = this.selectedRule();
            var url = new URI('api/v2/rss/removeRule');
            new Request.JSON({
                url: url,
                noCache: true,
                method: 'post',
                data: { ruleName: rule.name() },
                onFailure: function (response) {
                    //Unpexted behaviour, eror with status 200 trigger, why?
                    if (response.status !== 200) {
                        throw "Error deleting rule";
                    }
                    this.rules.remove(rule);

                }.bind(this)
            }).send();

        }
    };



    var Rule = function (name, data) {
        this.name = ko.observable(name);
        this.enabled = data.enabled;
        this.mustContain = data.mustContain;
        this.mustNotContain = data.mustNotContain;
        this.savePath = ko.observable(data.savePath);
        this.ignoreDays = data.ignoreDays;
        this.feeds = feeds.map(function (f) { return new Feed(f, data.affectedFeeds.indexOf(f.data.url) >= 0) });

        this.data = data;
        this.selectedPath = ko.observable(data.savePath);
        this.selectedPath.subscribe(function (path) { this.savePath(path); }, this);

        this.canSave = ko.observable(true);
    };

    Rule.prototype = {
        save: function () {
            var toSave = Object.assign({}, this.data);

            for (var key in toSave) {
                if (this.hasOwnProperty(key)) {
                    toSave[key] = ko.utils.unwrapObservable(this[key]);
                }
            }

            toSave.affectedFeeds = this.feeds.filter(function (f) { return f.enabled; }).map(function (f) { return f.url; });
            var json = ko.toJSON(toSave);
            var dirty = ko.toJSON(this.data) !== json;

            if (!dirty) return;

            var url = new URI('api/v2/rss/setRule');

            this.canSave(false);
            var request = new Request.JSON({
                url: url,
                noCache: true,
                method: 'post',
                data: { ruleName: this.name(), ruleDef: json },
                onFailure: function (response) {
                    //Unpexted behaviour, eror with status 200 trigger, why?
                    if (response.status !== 200) {
                        throw "Error saving rule";
                    }

                    this.data = toSave;
                    this.canSave(true);
                }.bind(this)
            }).send();
        }
    };

    var Torrent = function(data) {
        this.selected = ko.observable(false);
        this.name = data.title;
        this.downloadLink = data.torrentURL;
    }

    var Feed = function (feed, enabled) {
        this.name = feed.name;
        this.url = feed.data.url;
        this.id = feed.data.uid;
        this.enabled = enabled;
    };


    var orgOptionsApply = ko.bindingConventions.conventionBinders.options.apply;
    ko.bindingConventions.conventionBinders.options.apply = function (name, element, bindings, options, type, data, viewModel) {
        orgOptionsApply(name, element, bindings, options, type, data, viewModel);

        if (options.length === 0 || options[0]["name"]) {
            bindings.optionsText = function () { return "name"; };
        }
    };

    ko.bindingHandlers.modal = {
        init: function(element, valueAccessor) {
            valueAccessor().subscribe(function(value) {
                if (value) {
                    setTimeout(function() {
                            new MochaUI.Window({
                                title: "RSS",
                                content: element,
                                storeOnClose: true,
                                addClass: 'windowFrame', // fixes iframe scrolling on iOS Safari
                                scrollbars: true,
                                maximizable: false,
                                closable: true,
                                paddingVertical: 0,
                                paddingHorizontal: 0,
                                width: 512,
                                height: 256,
                                onClose: function() {
                                    valueAccessor()(null);
                                }
                            });
                        },
                        0);
                }
            });

        }
    };

    ko.tab = function(caption, template, model) {
        this.caption = caption;
        this.template = template;
        this.model = model;
        this.selected = ko.observable();
    };

    var tabsModel = function(tabs) {
        this.tabs = tabs;
        this.select(tabs[0]);

    };

    tabsModel.prototype = {
        select: function (tab) {
            this.tabs.forEach(t => t.selected(false));
            tab.selected(true);
        }
    };

    ko.bindingHandlers.tabs = {
        init: function (element, valueAccessor, allBindingsAccessor, viewModel, bindingContext) {
            var model = valueAccessor();
            ko.renderTemplate(tabsTemplate, bindingContext.createChildContext(new tabsModel(model)), { templateEngine: stringTemplateEngine }, element, "replaceChildren");
            MochaUI.initializeTabs(element.querySelector("ul"));
            return { controlsDescendantBindings: true };
        }
    };

    var stringTemplateSource = function (template) {
        this.template = template;
    };

    stringTemplateSource.prototype.text = function () {
        return this.template;
    };

    var stringTemplateEngine = new ko.nativeTemplateEngine();
    stringTemplateEngine.makeTemplateSource = function (template) {
        return new stringTemplateSource(template);
    };


    var tabsTemplate = '<div class="toolbarTabs">\
        <ul class="tab-menu" data-name="tabs">\
            <li data-bind="css: { selected: selected }"><a data-name="caption" data-bind="click: $parent.select.bind($parent)"></a></li>\
        </ul>\
        <div class="clear"></div>\
        </div>\
        <div data-name="tabs">\
            <div data-name="selected" data-bind="template: { name: template, data: model }"></div>\
        </div>';
})();
