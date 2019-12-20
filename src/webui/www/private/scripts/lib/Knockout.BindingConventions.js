// Knockout.BindingConventions
// (c) Anders Malmgren - https://github.com/AndersMalmgren/Knockout.BindingConventions
// License: MIT (http://www.opensource.org/licenses/mit-license.php)
(function (window, ko) {
    if (window.ko === undefined || ko.version < "3.0") {
        throw "This library is dependant on Knockout 3.0+";
    }

    String.prototype.endsWith = String.prototype.endsWith ? String.prototype.endsWith : function (suffix) {
        return this.indexOf(suffix, this.length - suffix.length) !== -1;
    };

    String.prototype.trim = String.prototype.trim || function () {
        return this.replace(/^\s+|\s+$/g, '');
    };

    var defaults = {
        roots: [window],
        excludeConstructorNames: ["Class"],
        useTextInputBinding: false
    };

    var textValueBinding = null;
    var prechecked = false;
    ko.bindingConventions = {
        init: function (options) {
            prechecked = false;
            ko.utils.extend(defaults, options);
        },
        conventionBinders: {}
    };

    ko.bindingConventions.ConventionBindingProvider = function () {

        this.orgBindingProvider = ko.bindingProvider.instance || new ko.bindingProvider();
        this.orgNodeHasBindings = this.orgBindingProvider.nodeHasBindings;
        this.attribute = "data-name";
        this.virtualAttribute = "ko name:";
    };

    ko.bindingConventions.ConventionBindingProvider.prototype = {
        getMemberName: function (node) {
            var name = null;

            if (node.nodeType === 1) {
                name = node.getAttribute(this.attribute);
            }
            else if (node.nodeType === 8) {
                var value = "" + node.nodeValue || node.text;
                var index = value.indexOf(this.virtualAttribute);

                if (index > -1) {
                    name = value.substring(index + this.virtualAttribute.length).trim();
                }
            }

            return name;
        },
        nodeHasBindings: function (node) {
            return this.orgNodeHasBindings(node) || this.getMemberName(node) !== null;
        },
        getBindingAccessors: function (node, bindingContext) {
            var name = this.getMemberName(node);

            var result = (name != null && node.nodeType === 8) ? null : this.orgBindingProvider.getBindingAccessors(node, bindingContext);
            if (name != null) {
                result = result || {};
                setBindingsByConvention(name, node, bindingContext, result);
            }

            return result;
        }
    };
    ko.bindingProvider.instance = new ko.bindingConventions.ConventionBindingProvider();

    var getDataFromComplexObjectQuery = function (name, context) {
        var parts = name.split(".");
        for (var i = 0; i < parts.length; i++) {
            var part = parts[i];
            var value = context[part];
            if (i != parts.length - 1)
                value = ko.utils.unwrapObservable(value);
            else {
                return {
                    data: value,
                    context: context,
                    name: part
                };
            }

            context = value;
        }

        return context;
    };

    var setBindingsByConvention = function (name, element, bindingContext, bindings) {
        var data = bindingContext[name] ? bindingContext[name] : bindingContext.$data[name];
        var context = bindingContext.$data;

        if (data === undefined) {
            var result = getDataFromComplexObjectQuery(name, context);
            data = result.data;
            bindingContext = { $data: result.context };
            name = result.name;
        }

        if (data === undefined) {
            throw "Can't resolve member: " + name;
        }

        var dataFn = function() {
            return data;
        };
        var unwrapped = ko.utils.peekObservable(data);
        var type = typeof unwrapped;
        var foundConvention = null;
        for (var index in ko.bindingConventions.conventionBinders) {
            if (ko.bindingConventions.conventionBinders[index].rules !== undefined) {
                var convention = ko.bindingConventions.conventionBinders[index];
                var should = true;
                if (unwrapped == null && convention.deferredApplyIfDataNotSet === true) {
                    continue;
                }

                if (convention.rules.length === 1) {
                    should = convention.rules[0](name, element, bindings, unwrapped, type, data, bindingContext);
                } else {
                    arrayForEach(convention.rules, function(rule) {
                        should = should && rule(name, element, bindings, unwrapped, type, data, bindingContext);
                        return should;
                    });
                }

                if (should) {
                    foundConvention = convention;
                    break;
                }
            }
        }

        if (foundConvention === null && unwrapped != null) throw "No convention was found for " + name;
        if (foundConvention !== null) {
            foundConvention.apply(name, element, bindings, unwrapped, type, dataFn, bindingContext);
        } else if (unwrapped == null && ko.isObservable(data)) {
            // To support deferred bindings, we need to set up a one-time subscription to apply the binding later
            var deferSubscription = data.subscribe(function(newValue) {
                if (newValue != null) {
                    deferSubscription.dispose();
                    var bindings = {};
                    setBindingsByConvention(name, element, bindingContext, bindings);
                    ko.applyBindingAccessorsToNode(element, bindings, bindingContext);
                }
            });

        }
    };

    ko.bindingConventions.conventionBinders.button = {
        rules: [function (name, element, bindings, unwrapped, type) { return element.tagName === "BUTTON" && type === "function"; } ],
        apply: function (name, element, bindings, unwrapped, type, dataFn, bindingContext) {
            bindings.click = dataFn;

            setBinding(bindings, 'enable', "can" + getPascalCased(name), bindingContext);
        }
    };

    ko.bindingConventions.conventionBinders.options = {
        rules: [function (name, element, bindings, unwrapped) { return element.tagName === "SELECT" && unwrapped.push; } ],
        apply: function (name, element, bindings, unwrapped, type, dataFn, bindingContext) {
            bindings.options = dataFn;
            var selectedMemberFound = false;
            var bindingName = null;
            var selected = null;
            var selectedName = null;

            singularize(name, function (singularized) {
                var pascalCasedItemName = getPascalCased(singularized);
                bindingName = "value";
                selectedName = "selected" + pascalCasedItemName;
                selected = setBinding(bindings, bindingName, selectedName, bindingContext);
                if (selected) {
                    setBinding(bindings, 'enable', "canChangeSelected" + pascalCasedItemName, bindingContext);
                    selectedMemberFound = true;
                    return true;
                }
            });

            if (!selectedMemberFound) {
                var pascalCased = getPascalCased(name);
                bindingName = "selectedOptions";
                selectedName = "selected" + pascalCased;
                selected = setBinding(bindings, bindingName, selectedName, bindingContext);
                setBinding(bindings, 'enable', "canChangeSelected" + pascalCased, bindingContext);
            }

            applyMemberWriter(bindings, bindingName, selected, selectedName, bindingContext);
        }
    };

    ko.bindingConventions.conventionBinders.input = {
        rules: [function (name, element) { return element.tagName === "INPUT" || element.tagName === "TEXTAREA"; } ],
        apply: function (name, element, bindings, unwrapped, type, dataFn, bindingContext) {
            var bindingName = null;
            if (type === "boolean") {
                if (ko.utils.ieVersion === undefined) {
                    element.setAttribute("type", "checkbox");
                }
                bindingName = "checked";
            } else {
                bindingName = textValueBinding;
            }
            bindings[bindingName] = dataFn;
            applyMemberWriter(bindings, bindingName, dataFn, name, bindingContext);
            setBinding(bindings, 'enable', "canChange" + getPascalCased(name), bindingContext);
        }
    };

    ko.bindingConventions.conventionBinders.visible = {
        rules: [function (name, element, bindings, unwrapped, type) { return type === "boolean" && element.tagName !== "INPUT"; } ],
        apply: function(name, element, bindings, unwrapped, type, dataFn) {
            if (element.nodeType === element.COMMENT_NODE) {
                bindings["if"] = dataFn;
            } else {
                bindings.visible = dataFn;
            }
        }
    };

    ko.bindingConventions.conventionBinders.text = {
        rules: [function (name, element, bindings, unwrapped, type) { return type !== "object" && type !== "boolean" && element.tagName !== "IMG" && element.tagName !== "INPUT" && element.tagName !== "TEXTAREA" && !nodeHasContent(element); } ],
        apply: function (name, element, bindings, unwrapped, type, dataFn) {
            bindings.text = dataFn;
        },
        deferredApplyIfDataNotSet: true
    };

    ko.bindingConventions.conventionBinders["with"] = {
        rules: [function (name, element, bindings, unwrapped, type) {
            return (type === "object" || unwrapped === undefined) &&
            (unwrapped == null || unwrapped.push === undefined) &&
            nodeHasContent(element);
        } ],
        apply: function (name, element, bindings, unwrapped, type, dataFn) {
            bindings["with"] = dataFn;
        }
    };

    ko.bindingConventions.conventionBinders.foreach = {
        rules: [function (name, element, bindings, unwrapped) { return unwrapped && unwrapped.push && nodeHasContent(element); } ],
        apply: function (name, element, bindings, unwrapped, type, dataFn) {
            bindings.foreach = dataFn;
        }
    };

    ko.bindingConventions.conventionBinders.template = {
        rules: [function (name, element, bindings, unwrapped, type) { return type === "object" && !nodeHasContent(element); } ],
        apply: function (name, element, bindings, unwrapped, type, dataFn) {
            bindings.template = function() {
                var actualModel = ko.unwrap(dataFn());
                var isArray = actualModel != null && actualModel.push !== undefined;
                var isDeferred = actualModel == null || (isArray && actualModel.length == 0);

                var template = null;
                if (!isDeferred) {
                    template = viewLocator.instance.getView(isArray ? actualModel[0] : actualModel);

                    if (template == null) {
                        throw "View name could not be found";
                    }
                }

                var binding = { name: template, 'if': actualModel };
                if (isArray) {
                    binding.foreach = actualModel;
                } else {
                    binding.data = actualModel;
                }
                return binding;
            };
        },
        deferredApplyIfDataNotSet: true
    };

    ko.bindingConventions.conventionBinders.image = {
        rules: [function (name, element, bindings, unwrapped, type) { return type === "string" && element.tagName === "IMG"; } ],
        apply: function (name, element, bindings, unwrapped, type, dataFn) {
            bindings.attr = function() {
                return { src: dataFn() };
            };
        },
        deferredApplyIfDataNotSet: true
    };

    var setBinding = function(bindings, bindingName, dataName, bindingContext) {
        if (bindingContext.$data[dataName] !== undefined) {
            return (bindings[bindingName] = function() {
                return bindingContext.$data[dataName];
            });
        }
    };

    var getPascalCased = function (text) {
        return text.substring(0, 1).toUpperCase() + text.substring(1);
    };

    var pluralEndings = [{ end: "ies", use: "y" }, "es", "s"];
    var singularize = function (name, callback) {
        var singularized = null;
        arrayForEach(pluralEndings, function (ending) {
            var append = ending.use;
            ending = ending.end || ending;
            if (name.endsWith(ending)) {
                singularized = name.substring(0, name.length - ending.length);
                singularized = singularized + (append || "");
                if (callback) {
                    return !callback(singularized);
                }

                return true;
            }
            return true;
        });

        return singularized;
    };

    var arrayForEach = function (array, action) {
        for (var i = 0; i < array.length; i++) {
            var result = action(array[i]);
            if (result === false) break;
        }
    };

    var nodeHasContent = function (node) {
        if (node.__nodeHasContent !== undefined) return node.__nodeHasContent;

        return node.__nodeHasContent = (node.nodeType === node.COMMENT_NODE && (node.nextSibling.nodeType === node.ELEMENT_NODE || hasContentBeforeEndTag(node))) ||
            (node.nodeType === node.ELEMENT_NODE && node.innerHTML.trim() !== "");
    };

    var hasContentBeforeEndTag = function (node) {
        if (node.nextSibling.nodeType === node.COMMENT_NODE && node.nextSibling.textContent.indexOf("/ko") > -1)
            return false;

        if (node.nextSibling.nodeType === node.nextSibling.ELEMENT_NODE || node.nextSibling.textContent.trim() !== "")
            return true;

        return hasContentBeforeEndTag(node.nextSibling);
    };
    
    var applyMemberWriter = function (bindings, bindingName, accessor, memberName, bindingContext) {
        if (!ko.isObservable(accessor())) {
            getMemberWriters(bindings)[bindingName] = function (value) {
                bindingContext.$data[memberName] = value;
            };
        }
    };

    var getMemberWriters = function(bindings) {
        var propertyWriters = null;
        if (bindings._ko_property_writers === undefined) {
            propertyWriters = {};
            bindings._ko_property_writers = function () {
                return propertyWriters;
            };
        }

        return propertyWriters;
    };

    var preCheckConstructorNames = function () {
        var flagged = [];
        var nestedPreCheck = function (root) {
            if (root == null || root.__fcnChecked || root === window) return;

            root.__fcnChecked = true;
            if (root.__fcnChecked === undefined) return;
            flagged.push(root);
            for (var index in root) {
                var item = root[index];
                if (item !== undefined && index.endsWith("Model") && typeof item === "function") {
                    item.__fcnName = index;
                }
                nestedPreCheck(item);
            }
        };

        arrayForEach(defaults.roots, function (root) {
            nestedPreCheck(root);
        });

        arrayForEach(flagged, function (flag) {
            delete flag.__fcnChecked;
        });
    };

    var findConstructorName = function (obj, isConstructor) {
        var constructor = isConstructor ? obj : obj.constructor;

        if (constructor.__fcnName !== undefined) {
            return constructor.__fcnName;
        }

        var funcNameRegex = /function (.{1,})\(/;
        var results = (funcNameRegex).exec(constructor.toString());
        var name = (results && results.length > 1) ? results[1] : undefined;
        var index;

        var excluded = false;
        arrayForEach(defaults.excludeConstructorNames, function (exclude) {
            if (exclude === name) {
                excluded = true;
                return false;
            }
            return true;
        });

        if (name === undefined || excluded) {
            var flagged = [];
            var nestedFind = function (root) {
                if (root == null ||
                    root === window.document ||
                    root === window.html ||
                    root === window.history || // fixes security exception
                    root === window.frameElement || // fixes security exception when placed in an iFrame
                    typeof root === "function" ||
                    root.__fcnChecked === true || // fixes circular references
                    (root.location && root.location != window.location) // fixes (i)frames
                ) {
                    return;
                }
                try {
                    root.__fcnChecked = true;
                } catch (err) {
                    return; // IE error
                }
                if (root.__fcnChecked === undefined) {
                    return;
                }
                flagged.push(root);

                for (index in root) {
                    var item = root[index];
                    if (item === constructor) {
                        return index;
                    }


                    var found = nestedFind(item);
                    if (found !== undefined) {
                        return found;
                    }
                }
            };

            arrayForEach(defaults.roots, function (root) {
                name = nestedFind(root);
                if (name !== undefined) {
                    return false;
                }
                return true;
            });

            for (index in flagged) {
                delete flagged[index].__fcnChecked;
            }
        }
        constructor.__fcnName = name;
        return name;
    };

    var viewLocator = function () {
    };
    viewLocator.prototype = {
        init: function() {
            preCheckConstructorNames();
        },
        getView: function (viewModel) {
            var className = findConstructorName(viewModel);
            var modelEndsWith = "Model";
            var template = null;
            if (className != null && className.endsWith(modelEndsWith)) {
                template = className.substring(0, className.length - modelEndsWith.length);
                if (!template.endsWith("View")) {
                    template = template + "View";
                }
            }
            return template;
        }
    }
    viewLocator.instance = new viewLocator();
    ko.bindingConventions.viewLocator = viewLocator;

    var orgApplyBindings = ko.applyBindings;
    ko.applyBindings = function (viewModel, element) {
        if (prechecked === false && viewLocator.instance.init) {
            viewLocator.instance.init();
            prechecked = true;
        }

        textValueBinding = defaults.useTextInputBinding ? "textInput" : "value";

        orgApplyBindings(viewModel, element);
    };

    ko.bindingConventions.utils = {
        findConstructorName: findConstructorName,
        singularize: singularize,
        getPascalCased: getPascalCased,
        nodeHasContent: nodeHasContent,
        setBinding: setBinding
    };
})(window, ko);