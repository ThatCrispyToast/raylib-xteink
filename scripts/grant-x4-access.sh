#!/usr/bin/env bash
# Owner-run, temporary access to one reader. Removed automatically at reboot.
set -euo pipefail
if [[ $EUID != 0 ]]; then
    echo 'Usage: sudo bash scripts/grant-x4-access.sh SERIAL [USER]' >&2
    exit 1
fi
reader_serial=${1:?Supply the reader serial number from /dev/serial/by-id}
reader_owner=${2:-${SUDO_USER:-}}
if [[ ! $reader_serial =~ ^[a-fA-F0-9:]{12,17}$ || ! $reader_owner =~ ^[a-z_][a-z0-9_-]*$ ]]; then
    echo 'Invalid serial number or username' >&2
    exit 1
fi
id "$reader_owner" >/dev/null
install -d -m 755 /run/udev/rules.d
cat > /run/udev/rules.d/70-raylib-x4.rules <<RULE
SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", ATTRS{serial}=="$reader_serial", OWNER="$reader_owner", MODE="0600"
RULE
udevadm control --reload-rules
udevadm trigger --action=change --subsystem-match=tty
udevadm settle
echo 'Temporary X4-only access enabled. Undo: remove /run/udev/rules.d/70-raylib-x4.rules, reload rules, and reconnect the reader.'
