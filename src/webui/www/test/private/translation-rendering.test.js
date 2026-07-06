/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

import { fileURLToPath } from "node:url";
import fs from "node:fs";
import path from "node:path";
import vm from "node:vm";

import { describe, expect, test } from "vitest";

const WEBUI_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const TRANSLATIONS_DIR = path.join(WEBUI_ROOT, "translations");

const trRegex = /QBT_TR\(((?:[^)]|\)(?!QBT_TR))+)\)QBT_TR\[CONTEXT=([a-zA-Z_]\w*)\]/g;
const scriptRegex = /<script(?:\s[^>]*)?>([\s\S]*?)<\/script>/gi;

const getFiles = (dir, predicate) => {
    const result = [];
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
        const entryPath = path.join(dir, entry.name);
        if (entry.isDirectory())
            result.push(...getFiles(entryPath, predicate));
        else if (predicate(entryPath))
            result.push(entryPath);
    }

    return result;
};

const parseTranslations = (tsPath) => {
    const doc = new DOMParser().parseFromString(fs.readFileSync(tsPath, "utf8"), "application/xml");
    expect(doc.querySelector("parsererror")).toBeNull();

    const translations = new Map();
    for (const context of doc.querySelectorAll("context")) {
        const contextName = context.querySelector("name")?.textContent ?? "";
        for (const message of context.querySelectorAll("message")) {
            const source = message.querySelector("source")?.textContent;
            if (source === undefined)
                continue;

            const translationElement = message.querySelector("translation");
            if ((translationElement === null) || translationElement.hasAttribute("type"))
                continue;

            translations.set(`${contextName}\0${source}`, translationElement.textContent ?? "");
        }
    }

    return translations;
};

const renderTranslations = (source, translations) => {
    return source.replace(trRegex, (match, sourceText, context) => {
        const translation = translations.get(`${context}\0${sourceText}`) || sourceText;
        return translation.replaceAll("\"", "&#34;");
    });
};

const translatedJavaScriptSources = () => {
    const sources = [];
    for (const filePath of getFiles(path.join(WEBUI_ROOT, "private"), (entryPath) => entryPath.endsWith(".js")))
        sources.push({ path: filePath, content: fs.readFileSync(filePath, "utf8") });

    for (const filePath of getFiles(path.join(WEBUI_ROOT, "public"), (entryPath) => entryPath.endsWith(".js")))
        sources.push({ path: filePath, content: fs.readFileSync(filePath, "utf8") });

    const htmlFiles = [
        ...getFiles(path.join(WEBUI_ROOT, "private"), (entryPath) => entryPath.endsWith(".html")),
        ...getFiles(path.join(WEBUI_ROOT, "public"), (entryPath) => entryPath.endsWith(".html")),
    ];
    for (const filePath of htmlFiles) {
        const html = fs.readFileSync(filePath, "utf8");
        for (const [index, match] of [...html.matchAll(scriptRegex)].entries()) {
            if (match[1].trim().length > 0)
                sources.push({ path: `${filePath} <script ${index + 1}>`, content: match[1] });
        }
    }

    return sources;
};

describe("translated JavaScript syntax", () => {
    const sources = translatedJavaScriptSources();

    for (const tsPath of getFiles(TRANSLATIONS_DIR, (entryPath) => entryPath.endsWith(".ts"))) {
        const locale = path.basename(tsPath, ".ts").replace(/^webui_/, "");
        test(`locale ${locale}`, () => {
            const translations = parseTranslations(tsPath);
            for (const source of sources) {
                const rendered = renderTranslations(source.content, translations);
                try {
                    new vm.Script(rendered, { filename: source.path });
                }
                catch (error) {
                    throw new Error(`${source.path}: ${error.message}`, { cause: error });
                }
            }
        });
    }
});
