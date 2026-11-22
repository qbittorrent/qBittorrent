import { defineConfig, definePlugin } from "html-validate";

import TranslationMacro from "./htmlValidateTranslationMacro.mjs";

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
