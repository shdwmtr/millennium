#!/bin/bash

# Adapted from install.beta.sh 
# Build and install the current cloned build of Millennium
# `sudo bash docker-build-linux.sh install` will run the build and install process
# `bash docker-build-linux.sh` will build the install files and export them in build-millennium.tar.gz
# Author: github.com/LazyHatGuy
# Licence: MIT

#set -ex

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
    echo -e "$1"
}

is_root() {
    [ "$(id -u)" -eq 0 ]
}

if [ "$1" == 'install' ] && ! is_root;  then
    log "${BOLD_RED}!!${RESET} Insufficient permissions. Please run the script as sudo or root"
    exit 1
fi

case $(uname -sm) in
    "Linux x86_64") ;;
    *) log "${BOLD_RED}!!${RESET} Unsupported platform $(uname -sm). x86_64 is the only available platform."; exit ;;
esac

log "resolving dependencies..."

# check for dependencies
command -v tar >/dev/null || { log "tar isn't installed! Install it from your package manager." >&2; exit 1; }
command -v docker >/dev/null || { log "docker isn't installed! https://docs.docker.com/engine/install/" >&2; exit 1; }

if [ ! -f build-millennium.tar.gz ]; then
    log "${BOLD_YELLOW}File not found! Building...${RESET}"
    
    if docker build . -t millennium-builder:latest ; then
      log "\n\t${BOLD_GREEN}Docker Build Success${RESET}\n"
    else
      log "\n\t${BOLD_RED}Docker Build Failed${RESET}\n"
      exit 1
    fi
    
    if docker run -d --name millennium-builder millennium-builder:latest ;then
      log "\n\t${BOLD_GREEN}Docker Container Started${RESET}\n"
    else
      log "\n\t${BOLD_RED}Failed to start Docker container${RESET}\n"
      exit 1
    fi 
    
    docker cp millennium-builder:/home/runner/build-millennium.tar.gz .
    docker stop millennium-builder
    docker rm millennium-builder
    docker image rm millennium-builder
else
	log "${BOLD_YELLOW}build-millennium.tar.gz already exists. Skipping build${RESET}"
fi

if [ "$1" != 'install' ] ;  then
    log "${BOLD_GREEN}build-millennium.tar.gz Built${RESET}"
    exit 0
fi

# ask to proceed
echo -e "\n${BOLD_PINK}::${RESET} Installing for \"$SUDO_USER\". Proceed with installation? [Y/n] \c"

read -r proceed

if [ "$proceed" = "n" ]; then
    exit 1
fi

# locations
millennium_install="/tmp/millennium"
extract_path="$millennium_install/files"
tar="$PWD/build-millennium.tar.gz"

rm -rfv "$millennium_install"
if ! mkdir -p "$extract_path" ; then
	log "${BOLD_RED}Failed to create tmp dir...${RESET}"
	exit 1
fi

log "deflating assets..."

if file "$tar" | grep -q "gzip compressed data"; then
    if ! tar -xvf "$tar" -C "$extract_path" ; then
    	log "${BOLD_RED}Extraction failed...${RESET}"
    	exit 1
	fi
elif file "$tar" | grep -q "Zip archive data"; then
    if ! unzip -qo "$tar" -d "$extract_path" ; then
    	log "${BOLD_RED}Extraction failed...${RESET}"
		exit 1
	fi
else
    echo "${BOLD_YELLOW}The file is neither a valid tar.gz nor ZIP archive.${RESET}"
    exit 1
fi

log "\n\n\t${BOLD_YELLOW}Moving files to /home/$SUDO_USER/ ${RESET}\n\n"

if ! cp -rfv "$extract_path/build/home/user/.local" "/home/$SUDO_USER/" ; then
	log "${BOLD_RED}Failed to move files to /home/$SUDO_USER/ ...${RESET}"
	exit 1
fi

log "\n\n\t${BOLD_YELLOW}Moving files to /usr ${RESET}\n\n"

if ! sudo cp -rfv "$extract_path/build/usr/" "/" ; then
	log "${BOLD_RED}Failed to move files to /usr/ ...${RESET}"
	exit 1
fi

folder_size=$(du -sb "$extract_path" | awk '{print $1}' | numfmt --to=iec-i --suffix=B --padding=7 | sed 's/\([0-9]\)\([A-Za-z]\)/\1 \2/')

log "\nTotal Install Size: $folder_size"

log "cleaning up packages..."
rm -rf "$millennium_install"
log "done."

if ! sudo chown -R "$SUDO_USER":"$SUDO_USER" "/home/$SUDO_USER/.local/share/millennium" ; then
	log "${BOLD_RED}Failed to set permissions for /home/$SUDO_USER/.local/share/millennium ...${RESET}"
fi

log "\n${BOLD_PINK}::${RESET} To get started, run \"millennium patch\" to load Millennium along side Steam. Your base installation of Steam has not been modified, this is simply an extension."
