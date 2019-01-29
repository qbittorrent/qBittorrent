(function () {
    var modalTemplate = '<div>\
   <select data-name="rules"></select>\
   <button data-name="addRule">Add</button>\
   <button data-name="deleteSelectedRule">Delete</button>\
   <div data-name="selectedRule">\
      <div><label><input data-name="enabled"/>Enabled</label></div>\
      <div><label>Name</label><input data-name="name" /></div>\
      <div><label>Must contain</label><input data-name="mustContain" /></div>\
      <div><label>Must NOT contain</label><input data-name="mustNotContain" /></div>\
      <div><label>Save path</label><input data-name="savePath" /><select data-bind="options: $parent.paths, value: selectedPath"></select></div>\
      <label>Feeds</label>\
      <div data-name="feeds">\
          <div><label><input data-name="enabled"/><span data-name="name"></span></label></div>\
      </div>\
      <button data-name="save">Save</button>\
   </div>\
</div>'

    var button = new Element("a", { html: "<img class='mochaToolButton' title='RSS Rules' src='images/qbt-theme/rss-config.svg' alt='RSS Rules' width='24' height='24'/>" });
    button.setAttribute("data-bind", "click: showRss");
    button.setAttribute("class", "divider")

    var view =  new Element("div",  { html: "<div><div data-name='modal' data-bind='modal: modal'>" + modalTemplate + "</div></div>"})

    var feeds = null;
    new Request.JSON({
        url: new URI('api/v2/rss/items'),
        noCache: true,
        method: 'get',
        onFailure: function () {
            //TODO: error handling
        },
        onSuccess: function (response) {
            feeds = Object.keys(response).map(function (key) { return { name: key, url: response[key] != "" ? response[key] : key }; })
        }
    }).send();

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
        this.rules = ko.observableArray();
        this.selectedRule = ko.observable();

        this.listRules();
        this.canDeleteSelectedRule = ko.computed(function () { return this.selectedRule() != null }, this);
    };

    RssModel.prototype = {
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
        this.feeds = feeds.map(function (f) { return new Feed(f, data.affectedFeeds.indexOf(f.url.url) >= 0) });

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

    var Feed = function (data, enabled) {
        this.name = data.name;
        this.url = data.url.url;
        this.id = data.url.uid;
        this.enabled = enabled;
    };


    var orgOptionsApply = ko.bindingConventions.conventionBinders.options.apply;
    ko.bindingConventions.conventionBinders.options.apply = function (name, element, bindings, options, type, data, viewModel) {
        orgOptionsApply(name, element, bindings, options, type, data, viewModel)

        if (options.length === 0 || options[0]["name"]) {
            bindings.optionsText = function () { return "name"; };
        }
    };

    ko.bindingHandlers.modal = {
        init: function (element, valueAccessor) {
            valueAccessor().subscribe(function (value) {
                if (value) {
                    setTimeout(function () {
                        new MochaUI.Window({
                            title: "RSS auto download",
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
                            onClose: function () {
                                valueAccessor()(null);
                            }
                        });
                    }, 0);
                }
            });

        }
    }
})();
