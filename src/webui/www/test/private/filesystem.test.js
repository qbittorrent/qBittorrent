import { afterEach, beforeEach, expect, test } from "vitest";

import "../../private/scripts/filesystem.js";

const setPlatform = (platform) => {
    window.qBittorrent.Cache = {
        buildInfo: {
            get: () => ({ platform })
        }
    };
};

const originalParent = window.parent;

beforeEach(() => {
    setPlatform("linux");
});

afterEach(() => {
    window.parent = originalParent;
});

test("Test isValidFileName() matches Unix filename limits", () => {
    const isValidFileName = window.qBittorrent.Filesystem.isValidFileName;

    expect(isValidFileName("")).toBe(false);
    expect(isValidFileName(".")).toBe(false);
    expect(isValidFileName("..")).toBe(false);
    expect(isValidFileName("a".repeat(255))).toBe(true);
    expect(isValidFileName("a".repeat(256))).toBe(false);
    expect(isValidFileName("é".repeat(127))).toBe(true);
    expect(isValidFileName("é".repeat(128))).toBe(false);
    expect(isValidFileName("folder/file")).toBe(false);
    expect(isValidFileName("folder\\file")).toBe(true);
});

test("Test isValidFileName() applies Windows filename rules", () => {
    setPlatform("windows");

    const isValidFileName = window.qBittorrent.Filesystem.isValidFileName;

    expect(isValidFileName("CON.txt")).toBe(false);
    expect(isValidFileName("file.")).toBe(false);
    expect(isValidFileName("file ")).toBe(false);
    expect(isValidFileName("folder\\file")).toBe(false);
});

test("Test isValidFileName() applies macOS filename rules", () => {
    setPlatform("macos");

    const isValidFileName = window.qBittorrent.Filesystem.isValidFileName;

    expect(isValidFileName("folder:file")).toBe(false);
    expect(isValidFileName("folder\\file")).toBe(true);
});

test("Test platform detection falls back to the parent WebUI", () => {
    window.qBittorrent.Cache = undefined;
    window.parent = {
        qBittorrent: {
            Cache: {
                buildInfo: {
                    get: () => ({ platform: "windows" })
                }
            }
        }
    };

    const filesystem = window.qBittorrent.Filesystem;

    expect(filesystem.isCaseSensitive()).toBe(false);
    expect(filesystem.isValidFileName("CON.txt")).toBe(false);
});
