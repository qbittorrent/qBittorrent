import { Rule, TextNode, defineConfig, definePlugin, sliceLocation } from "html-validate";

class TranslationMacro extends Rule {
    documentation() {
        return {
            description: "Translations in HTML must use `QBT_TR_HTML()` and must not be used in event handler attributes."
        };
    }

    setup() {
        this.on("attr", (event) => {
            if (typeof event.value !== "string")
                return;

            if (event.key.toLowerCase().startsWith("on")) {
                for (const match of event.value.matchAll(/QBT_TR(_[A-Z]+)?\(/g))
                    this.reportMatch(event.target, "Do not use translations in event handler attributes.", event.value, event.valueLocation, match);
                return;
            }

            this.checkText(event.target, event.value, event.valueLocation);
        });

        this.on("dom:ready", (event) => {
            const visit = (element) => {
                // scripts are checked by ESLint
                if (element.is("script") || element.is("style"))
                    return;

                for (const child of element.childNodes) {
                    if (child instanceof TextNode)
                        this.checkText(element, child.textContent, child.location);
                }
                for (const child of element.childElements)
                    visit(child);
            };
            visit(event.document.root);
        });
    }

    checkText(node, text, location) {
        for (const match of text.matchAll(/QBT_TR(_[A-Z]+)?\(/g)) {
            if (match[1] !== "_HTML")
                this.reportMatch(node, "Use `QBT_TR_HTML()` for translations in HTML.", text, location, match);
        }
    }

    reportMatch(node, message, text, location, match) {
        this.report(node, message, sliceLocation(location, match.index, (match.index + match[0].length), text));
    }
}

export default defineConfig({
    extends: [
        "html-validate:recommended"
    ],
    plugins: [
        definePlugin({
            name: "qbt-webui",
            rules: {
                "qbt-webui/translation-macro": TranslationMacro
            }
        })
    ],
    rules: {
        "input-missing-label": "error",
        "long-title": "off",
        "no-inline-style": "off",
        "no-missing-references": "error",
        "prefer-button": "off",
        "qbt-webui/translation-macro": "error",
        "require-sri": [
            "error",
            {
                target: "crossorigin"
            }
        ]
    }
});
