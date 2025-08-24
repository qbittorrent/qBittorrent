import Globals from "globals";
import Html from "eslint-plugin-html";
import Js from "@eslint/js";
import PluginQbtWebUI from "eslint-plugin-qbt-webui";
import PreferArrowFunctions from "eslint-plugin-prefer-arrow-functions";
import * as RegexpPlugin from "eslint-plugin-regexp";
import Stylistic from "@stylistic/eslint-plugin";
import Unicorn from "eslint-plugin-unicorn";

export default [
    Js.configs.recommended,
    RegexpPlugin.configs["flat/recommended"],
    Stylistic.configs["disable-legacy"],
    {
        files: [
            "**/*.html",
            "**/*.js",
            "**/*.mjs"
        ],
        languageOptions: {
            ecmaVersion: 2022,
            globals: {
                ...Globals.browser
            }
        },
        plugins: {
            Html,
            PluginQbtWebUI,
            PreferArrowFunctions,
            RegexpPlugin,
            Stylistic,
            Unicorn
        },
        rules: {
            "curly": ["error", "multi-or-nest", "consistent"],
            "eqeqeq": "error",
            "guard-for-in": "error",
            "no-undef": "off",
            "no-unused-vars": "off",
            "no-var": "error",
            "object-shorthand": ["error", "consistent"],
            "operator-assignment": "error",
            "prefer-arrow-callback": "error",
            "prefer-const": "error",
            "prefer-template": "error",
            "radix": "error",
            "require-await": "error",
            "PluginQbtWebUI/prefix-inc-dec-operators": "error",
            "PreferArrowFunctions/prefer-arrow-functions": "error",
            "Stylistic/no-extra-semi": "error",
            "Stylistic/no-mixed-operators": [
                "error",
                {
                    groups: [
                        ["&", "|", "^", "~", "<<", ">>", ">>>", "==", "!=", "===", "!==", ">", ">=", "<", "<=", "&&", "||", "in", "instanceof"]
                    ]
                }
            ],
            "Stylistic/nonblock-statement-body-position": ["error", "below"],
            "Stylistic/quotes": [
                "error",
                "double",
                {
                    avoidEscape: true,
                    allowTemplateLiterals: "avoidEscape"
                }
            ],
            "Stylistic/quote-props": ["error", "consistent-as-needed"],
            "Stylistic/semi": "error",
            "Stylistic/spaced-comment": ["error", "always", { exceptions: ["*"] }],
            "Unicorn/no-zero-fractions": "error",
            "Unicorn/prefer-number-properties": "error",
            "Unicorn/switch-case-braces": ["error", "avoid"]
        }
    }
];
