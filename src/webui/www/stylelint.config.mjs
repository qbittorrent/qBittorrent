export default {
    extends: "stylelint-config-standard",
    plugins: [
        "stylelint-order"
    ],
    ignoreFiles: ["private/css/lib/*.css"],
    rules: {
        "color-hex-length": "long",
        "comment-empty-line-before": null,
        "comment-whitespace-inside": null,
        "no-descending-specificity": null,
        "order/properties-alphabetical-order": true,
        "selector-class-pattern": null,
        "selector-id-pattern": null,
    }
};
