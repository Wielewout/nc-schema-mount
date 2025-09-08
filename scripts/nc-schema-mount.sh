#!/bin/bash

set -e

export SYSREPO_REPOSITORY_PATH="/etc/sysrepo"
export SR_YANG_DIR="$SYSREPO_REPOSITORY_PATH/yang"

SCRIPT_DIR="$(cd "$(dirname "$0")"/.. && pwd)"
YANG_DIR="$(cd "$(dirname "$0")"/../yang && pwd)"

echo "Installing acc-host module if needed..."
if ! sysrepoctl -l | awk '{print $1}' | grep -qx acc-host; then
    sysrepoctl -i "$YANG_DIR/acc-host.yang"
fi

echo "Copying acc-foo module if needed..."
if [ ! -f "$SR_YANG_DIR/acc-foo.yang" ]; then
    cp "$YANG_DIR/acc-foo.yang" "$SR_YANG_DIR/acc-foo.yang"
fi

echo "Starting nc-schema-mount"
"$SCRIPT_DIR/build/nc-schema-mount" -s "$YANG_DIR"
