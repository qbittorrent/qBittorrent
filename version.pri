# keep it all lowercase to match program naming convention on *nix systems
PROJECT_NAME = qbittorrent

# Define version numbers here
VER_MAJOR = 4
VER_MINOR = 4
VER_BUGFIX = 0
VER_BUILD = 0
VER_STATUS = alpha1 # Should be empty for stable releases!

# Don't touch the rest part
PROJECT_VERSION = $${VER_MAJOR}.$${VER_MINOR}.$${VER_BUGFIX}

!equals(VER_BUILD, 0) {
    PROJECT_VERSION = $${PROJECT_VERSION}.$${VER_BUILD}
}

PROJECT_VERSION = $${PROJECT_VERSION}$${VER_STATUS}

COMMIT_HASH = ""

define_commit_hash {
    # first check if git is available
    HAS_GIT = FALSE

    win32:system("where git >null 2>&1"): HAS_GIT = TRUE
    unix|macos:system("which git > /dev/null 2>&1"): HAS_GIT = TRUE

    equals(HAS_GIT, TRUE) {
        COMMIT_HASH = $$system(git rev-parse --short=10 HEAD $$system_quote(--git-dir=$$PWD/.git))
        win32:system("git describe --exact-match $$COMMIT_HASH >null 2>&1"): COMMIT_HASH=""
        unix|macos:if(system("git describe --exact-match $$COMMIT_HASH > /dev/null 2>&1")): COMMIT_HASH=""
    } else {
        warning("define_commit_hash is used but git is not available")
    }
}

# Generate version header
versionHeader = $$cat(src/base/version.h.in, blob)
versionHeader = $$replace(versionHeader, "@VER_MAJOR@", $$VER_MAJOR)
versionHeader = $$replace(versionHeader, "@VER_MINOR@", $$VER_MINOR)
versionHeader = $$replace(versionHeader, "@VER_BUGFIX@", $$VER_BUGFIX)
versionHeader = $$replace(versionHeader, "@VER_BUILD@", $$VER_BUILD)
versionHeader = $$replace(versionHeader, "@PROJECT_VERSION@", $$PROJECT_VERSION)
versionHeader = $$replace(versionHeader, "@COMMIT_HASH@", $$COMMIT_HASH)
write_file(src/base/version.h, versionHeader)
