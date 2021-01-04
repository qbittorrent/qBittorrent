# keep it all lowercase to match program naming convention on *nix systems
PROJECT_NAME = qbittorrent

# Define version numbers here
VER_MAJOR = 4
VER_MINOR = 3
VER_BUGFIX = 2
VER_BUILD = 0
VER_STATUS = # Should be empty for stable releases!

# Don't touch the rest part
PROJECT_VERSION = $${VER_MAJOR}.$${VER_MINOR}.$${VER_BUGFIX}

!equals(VER_BUILD, 0) {
    PROJECT_VERSION = $${PROJECT_VERSION}.$${VER_BUILD}
}

PROJECT_VERSION = $${PROJECT_VERSION}$${VER_STATUS}

# Generate version header
versionHeader = $$cat(src/base/version.h.in, blob)
versionHeader = $$replace(versionHeader, "@VER_MAJOR@", $$VER_MAJOR)
versionHeader = $$replace(versionHeader, "@VER_MINOR@", $$VER_MINOR)
versionHeader = $$replace(versionHeader, "@VER_BUGFIX@", $$VER_BUGFIX)
versionHeader = $$replace(versionHeader, "@VER_BUILD@", $$VER_BUILD)
versionHeader = $$replace(versionHeader, "@PROJECT_VERSION@", $$PROJECT_VERSION)
write_file(src/base/version.h, versionHeader)
