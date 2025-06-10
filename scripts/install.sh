#!/usr/bin/env sh
# This file is part of the Millennium project.
# This script is used to install Millennium on a Linux machine.
# https://github.com/shdwmtr/millennium/blob/main/scripts/install.sh
# Copyright (c) 2025 Millennium
set -e
BOLD_RED="\033[1;31m"
BOLD_PINK="\033[1;35m"
BOLD_GREEN="\033[1;32m"
BOLD_YELLOW="\033[1;33m"
BOLD_BLUE="\033[1;34m"
BOLD_MAGENTA="\033[1;35m"
BOLD_CYAN="\033[1;36m"
BOLD_WHITE="\033[1;37m"
RESET="\033[0m"
log() {
    printf "%b\n" "$1"
}
is_root() {
    [ "$(id -u)" -eq 0 ]
}
if ! is_root;  then
    log "${BOLD_RED}!!${RESET} Insufficient permissions. Please run the script as sudo or root"
    exit 1
fi
case $(uname -sm) in
    "Linux x86_64") target="linux-x86_64" ;;
    *) log "${BOLD_RED}!!${RESET} Unsupported platform $(uname -sm). x86_64 is the only available platform."; exit ;;
esac
log "resolving dependencies..."
# check for dependencies
command -v curl >/dev/null || { log "curl isn't installed! Install it from your package manager." >&2; exit 1; }
command -v tar >/dev/null || { log "tar isn't installed! Install it from your package manager." >&2; exit 1; }
command -v grep >/dev/null || { log "grep isn't installed! Install it from your package manager." >&2; exit 1; }
command -v jq >/dev/null || { log "jq isn't installed! Install it from your package manager." >&2; exit 1; }
# download uri
releases_uri="https://api.github.com/repos/shdwmtr/millennium/releases"
if [ -z "$tag" ]; then
    tag=$(curl -LsH 'Accept: application/vnd.github.v3+json' "$releases_uri" | jq -r '[.[] | select(.prerelease == false)] | .[0].tag_name')
    # get the bytes size of the release assets
    size=$(curl -LsH 'Accept: application/vnd.github.v3+json' "$releases_uri" | jq -r '[.[] | select(.prerelease == false)] | .[0].assets | .[0].size')
fi
tag=${tag#v}
size=${size:-0}
download_uri=https://github.com/shdwmtr/millennium/releases/download/v$tag/millennium-v$tag-$target.tar.gz
# convert bytes to human readable format
size=$(echo $size | awk '{ split("B KB MB GB TB PB", v); s=1; while ($1 > 1024) { $1 /= 1024; s++ } printf "%.2f %s\n", $1, v[s] }')
log "Packages (1) Millennium@$tag-x86_64"
log "\nTotal Download Size: $size"
log "Retreiving packages..."
# ask to proceed
echo -e "\n${BOLD_PINK}::${RESET} Installing for \"$SUDO_USER\". Proceed with installation? [Y/n] \c"
read -r proceed </dev/tty
if [ "$proceed" = "n" ]; then
    exit 1
fi
# locations
millennium_install="/tmp/millennium"
extract_path="$millennium_install/files"
tar="$millennium_install/millennium.tar.gz"
rm -rf "$millennium_install"
mkdir -p "$millennium_install"
log "receiving packages..."
curl --fail --location --output "$tar" "$download_uri" --silent
log "deflating assets..."
mkdir -p "$extract_path"
tar xzf "$tar" -C "$extract_path"
mv "$extract_path/home/user" "$extract_path/home/$SUDO_USER"
folder_size=$(du -sb "$extract_path" | awk '{print $1}' | numfmt --to=iec-i --suffix=B --padding=7 | sed 's/\([0-9]\)\([A-Za-z]\)/\1 \2/')
log "\nTotal Install Size: $folder_size"
cp -r "$extract_path"/* / || true
chmod +x /usr/bin/millennium
# Fix: Use proper user:group ownership
if [ -d "/home/$SUDO_USER/.local/share/millennium" ]; then
    chown -R "$SUDO_USER:$(id -gn "$SUDO_USER")" /home/"$SUDO_USER"/.local/share/millennium
else
    log "${BOLD_YELLOW}Warning:${RESET} Directory /home/$SUDO_USER/.local/share/millennium not found"
fi
log "cleaning up packages..."
rm -rf "$millennium_install"
log "done."
log "\n${BOLD_PINK}::${RESET} To get started, see https://docs.steambrew.app/users/installing#post-installation.\n    Your base installation of Steam has not been modified, this is simply an extension."
