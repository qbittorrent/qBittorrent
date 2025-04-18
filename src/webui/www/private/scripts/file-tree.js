/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Thomas Piccirello <thomas.piccirello@gmail.com>
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

"use strict";

window.qBittorrent ??= {};
window.qBittorrent.FileTree ??= (() => {
    const exports = () => {
        return {
            FilePriority: FilePriority,
            TriState: TriState,
            FileTree: FileTree,
            FileNode: FileNode,
            FolderNode: FolderNode,
        };
    };

    const FilePriority = {
        Ignored: 0,
        Normal: 1,
        High: 6,
        Maximum: 7,
        Mixed: -1
    };
    Object.freeze(FilePriority);

    const TriState = {
        Unchecked: 0,
        Checked: 1,
        Partial: 2
    };
    Object.freeze(TriState);

    const FileTree = new Class({
        root: null,
        nodeMap: {},

        setRoot: function(root) {
            this.root = root;
            this.generateNodeMap(root);

            if (this.root.isFolder)
                this.root.calculateSize();
        },

        getRoot: function() {
            return this.root;
        },

        generateNodeMap: function(root) {
            const stack = [root];
            while (stack.length > 0) {
                const node = stack.pop();

                // don't store root node in map
                if (node.root !== null)
                    this.nodeMap[node.rowId] = node;

                stack.push(...node.children);
            }
        },

        getNode: function(rowId) {
            return (this.nodeMap[rowId] === undefined)
                ? null
                : this.nodeMap[rowId];
        },

        getRowId: (node) => {
            return node.rowId;
        },

        /**
         * Returns the nodes in DFS in-order
         */
        toArray: function() {
            const ret = [];
            const stack = this.root.children.toReversed();
            while (stack.length > 0) {
                const node = stack.pop();
                ret.push(node);
                stack.push(...node.children.toReversed());
            }
            return ret;
        }
    });

    const FileNode = new Class({
        name: "",
        path: "",
        rowId: null,
        size: 0,
        checked: TriState.Unchecked,
        remaining: 0,
        progress: 0,
        priority: FilePriority.Normal,
        availability: 0,
        depth: 0,
        root: null,
        data: null,
        isFolder: false,
        children: [],
    });

    const FolderNode = new Class({
        Extends: FileNode,

        /**
         * Will automatically tick the checkbox for a folder if all subfolders and files are also ticked
         */
        autoCheckFolders: true,

        initialize: function() {
            this.isFolder = true;
        },

        addChild: function(node) {
            this.children.push(node);
        },

        /**
         * Calculate size of node and its children
         */
        calculateSize: function() {
            const stack = [this];
            const visited = [];

            while (stack.length > 0) {
                const root = stack.at(-1);

                if (root.isFolder) {
                    if (visited.at(-1) !== root) {
                        visited.push(root);
                        stack.push(...root.children);
                        continue;
                    }

                    visited.pop();

                    // process children
                    root.size = 0;
                    root.remaining = 0;
                    root.progress = 0;
                    root.availability = 0;
                    root.checked = TriState.Unchecked;
                    root.priority = FilePriority.Normal;
                    let isFirstFile = true;

                    for (const child of root.children) {
                        root.size += child.size;

                        if (isFirstFile) {
                            root.priority = child.priority;
                            root.checked = child.checked;
                            isFirstFile = false;
                        }
                        else {
                            if (root.priority !== child.priority)
                                root.priority = FilePriority.Mixed;
                            if (root.checked !== child.checked)
                                root.checked = TriState.Partial;
                        }

                        const isIgnored = (child.priority === FilePriority.Ignored);
                        if (!isIgnored) {
                            root.remaining += child.remaining;
                            root.progress += (child.progress * child.size);
                            root.availability += (child.availability * child.size);
                        }
                    }

                    root.checked = root.autoCheckFolders ? root.checked : TriState.Checked;
                    root.progress /= root.size;
                    root.availability /= root.size;
                }

                stack.pop();
            }
        }
    });

    return exports();
})();
Object.freeze(window.qBittorrent.FileTree);
