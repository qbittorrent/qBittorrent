"use strict";

window.qBittorrent ??= {};
window.qBittorrent.MultiRename ??= (() => {
    const exports = () => {
        return {
            AppliesTo: AppliesTo,
            RenameFiles: RenameFiles
        };
    };

    const AppliesTo = {
        FilenameExtension: "FilenameExtension",
        Filename: "Filename",
        Extension: "Extension"
    };

    const RenameFiles = new Class({
        hash: "",
        selectedFiles: [],
        matchedFiles: [],

        // Search Options
        _inner_search: "",
        setSearch(val) {
            this._inner_search = val;
            this._inner_update();
            this.onChanged(this.matchedFiles);
        },
        useRegex: false,
        matchAllOccurrences: false,
        caseSensitive: false,

        // Replacement Options
        _inner_replacement: "",
        setReplacement(val) {
            this._inner_replacement = val;
            this._inner_update();
            this.onChanged(this.matchedFiles);
        },
        appliesTo: AppliesTo.FilenameExtension,
        includeFiles: true,
        includeFolders: false,
        replaceAll: false,
        fileEnumerationStart: 0,

        onChanged: (rows) => {},
        onInvalidRegex: (err) => {},
        onRenamed: (rows) => {},
        onRenameError: (response) => {},

        _inner_update: function() {
            const findMatches = (regex, str) => {
                let result;
                let count = 0;
                let lastIndex = 0;
                regex.lastIndex = 0;
                const matches = [];
                do {
                    result = regex.exec(str);
                    if (result === null)
                        break;

                    matches.push(result);

                    // regex assertions don't modify lastIndex,
                    // so we need to explicitly break out to prevent infinite loop
                    if (lastIndex === regex.lastIndex)
                        break;
                    else
                        lastIndex = regex.lastIndex;

                    // Maximum of 250 matches per file
                    ++count;
                } while (regex.global && (count < 250));

                return matches;
            };

            const replaceBetween = (input, start, end, replacement) => {
                return input.substring(0, start) + replacement + input.substring(end);
            };
            const replaceGroup = (input, search, replacement, escape, stripEscape = true) => {
                let result = "";
                let i = 0;
                while (i < input.length) {
                    // Check if the current index contains the escape string
                    if (input.substring(i, i + escape.length) === escape) {
                        // Don't replace escape chars when they don't precede the current search being performed
                        if (input.substring(i + escape.length, i + escape.length + search.length) !== search) {
                            result += input[i];
                            i++;
                            continue;
                        }
                        // Replace escape chars when they precede the current search being performed, unless explicitly told not to
                        if (stripEscape) {
                            result += input.substring(i + escape.length, i + escape.length + search.length);
                            i += escape.length + search.length;
                        }
                        else {
                            result += input.substring(i, i + escape.length + search.length);
                            i += escape.length + search.length;
                        }
                        // Check if the current index contains the search string
                    }
                    else if (input.substring(i, i + search.length) === search) {
                        result += replacement;
                        i += search.length;
                        // Append characters that didn't meet the previous criteria
                    }
                    else {
                        result += input[i];
                        i++;
                    }
                }
                return result;
            };

            this.matchedFiles = [];

            // Ignore empty searches
            if (!this._inner_search)
                return;

            // Setup regex flags
            let regexFlags = "";
            if (this.matchAllOccurrences)
                regexFlags += "g";
            if (!this.caseSensitive)
                regexFlags += "i";

            // Setup regex search
            const regexEscapeExp = /[/\-\\^$*+?.()|[\]{}]/g;
            const standardSearch = new RegExp(this._inner_search.replace(regexEscapeExp, "\\$&"), regexFlags);
            let regexSearch;
            try {
                regexSearch = new RegExp(this._inner_search, regexFlags);
            }
            catch (err) {
                if (this.useRegex) {
                    this.onInvalidRegex(err);
                    return;
                }
            }
            const search = this.useRegex ? regexSearch : standardSearch;

            let fileEnumeration = this.fileEnumerationStart;
            for (let i = 0; i < this.selectedFiles.length; ++i) {
                const row = this.selectedFiles[i];

                // Ignore files
                if (!row.isFolder && !this.includeFiles)
                    continue;

                // Ignore folders
                else if (row.isFolder && !this.includeFolders)
                    continue;

                // Get file extension and reappend the "." (only when the file has an extension)
                let fileExtension = window.qBittorrent.Filesystem.fileExtension(row.original);
                if (fileExtension)
                    fileExtension = `.${fileExtension}`;

                const fileNameWithoutExt = row.original.slice(0, row.original.lastIndexOf(fileExtension));

                let matches = [];
                let offset = 0;
                switch (this.appliesTo) {
                    case "FilenameExtension":
                        matches = findMatches(search, `${fileNameWithoutExt}${fileExtension}`);
                        break;
                    case "Filename":
                        matches = findMatches(search, `${fileNameWithoutExt}`);
                        break;
                    case "Extension":
                        // Adjust the offset to ensure we perform the replacement at the extension location
                        offset = fileNameWithoutExt.length;
                        matches = findMatches(search, `${fileExtension}`);
                        break;
                }
                // Ignore rows without a match
                if (!matches || (matches.length === 0))
                    continue;

                let renamed = row.original;
                for (let i = matches.length - 1; i >= 0; --i) {
                    const match = matches[i];
                    let replacement = this._inner_replacement;
                    // Replace numerical groups
                    for (let g = 0; g < match.length; ++g) {
                        const group = match[g];
                        if (!group)
                            continue;
                        replacement = replaceGroup(replacement, `$${g}`, group, "\\", false);
                    }
                    // Replace named groups
                    for (const namedGroup in match.groups) {
                        if (!Object.hasOwn(match.groups, namedGroup))
                            continue;
                        replacement = replaceGroup(replacement, `$${namedGroup}`, match.groups[namedGroup], "\\", false);
                    }
                    // Replace auxiliary variables
                    for (let v = "dddddddd"; v !== ""; v = v.substring(1)) {
                        const fileCount = fileEnumeration.toString().padStart(v.length, "0");
                        replacement = replaceGroup(replacement, `$${v}`, fileCount, "\\", false);
                    }
                    // Remove empty $ variable
                    replacement = replaceGroup(replacement, "$", "", "\\");
                    const wholeMatch = match[0];
                    const index = match["index"];
                    renamed = replaceBetween(renamed, index + offset, index + offset + wholeMatch.length, replacement);
                }

                row.renamed = renamed;
                ++fileEnumeration;
                this.matchedFiles.push(row);
            }
        },

        rename: async function() {
            if (!this.matchedFiles || (this.matchedFiles.length === 0) || !this.hash) {
                this.onRenamed([]);
                return;
            }

            const replaced = [];
            const _inner_rename = async function(i) {
                const match = this.matchedFiles[i];
                const newName = match.renamed;
                if (newName === match.original) {
                    // Original file name is identical to Renamed
                    return;
                }

                const isFolder = match.isFolder;
                const parentPath = window.qBittorrent.Filesystem.folderName(match.path);
                const oldPath = parentPath
                    ? parentPath + window.qBittorrent.Filesystem.PathSeparator + match.original
                    : match.original;
                const newPath = parentPath
                    ? parentPath + window.qBittorrent.Filesystem.PathSeparator + newName
                    : newName;
                try {
                    await fetch((isFolder ? "api/v2/torrents/renameFolder" : "api/v2/torrents/renameFile"), {
                        method: "POST",
                        body: new URLSearchParams({
                            hash: this.hash,
                            oldPath: oldPath,
                            newPath: newPath
                        })
                    });
                    replaced.push(match);
                }
                catch (response) {
                    this.onRenameError(response, match);
                }
            }.bind(this);

            const replacements = this.matchedFiles.length;
            if (this.replaceAll) {
                // matchedFiles are in DFS order so we rename in reverse
                // in order to prevent unwanted folder creation
                for (let i = replacements - 1; i >= 0; --i)
                    await _inner_rename(i);
            }
            else {
                // single replacements go linearly top-down because the
                // file tree gets recreated after every rename
                await _inner_rename(0);
            }
            this.onRenamed(replaced);
        },
        update: function() {
            this._inner_update();
            this.onChanged(this.matchedFiles);
        }
    });

    return exports();
})();
Object.freeze(window.qBittorrent.MultiRename);
