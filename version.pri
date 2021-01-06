# Generate version header
versionHeader = $$cat(src/base/version.h.in, blob)
write_file(src/base/version.h, versionHeader)
