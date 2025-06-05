import { defineConfig } from "html-validate";

export default defineConfig({
    extends: [
        "html-validate:recommended"
    ],
    rules: {
        "input-missing-label": "error",
        "long-title": "off",
        "no-inline-style": "off",
        "no-missing-references": "error",
        "prefer-button": "off",
        "require-sri": [
            "error",
            {
                target: "crossorigin"
            }
        ]
    }
});
