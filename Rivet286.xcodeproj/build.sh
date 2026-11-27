#!/bin/sh
set -eu

# Finder-launched Xcode may start with only the system directories on PATH.
export PATH="$PATH:/opt/homebrew/bin:/usr/local/bin"

exec /usr/bin/make all demo
