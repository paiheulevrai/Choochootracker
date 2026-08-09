#!/bin/bash

# Sample bash script for convenient upload of just PortMaster binary over SFTP for muOS

# Configuration variables
SOURCE_FILE="build/portmaster/choochootracker.aarch64"
REMOTE_SERVER="sftp://muos@10.0.0.219:2022"
REMOTE_PATH="/SD2 (sdcard)/ports/choochootracker"
: "${MUOS_PASSWORD:?Set MUOS_PASSWORD before running this script}"

# Upload file via SFTP
sshpass -p "$MUOS_PASSWORD" sftp -o StrictHostKeyChecking=no "$REMOTE_SERVER" << EOF
cd "$REMOTE_PATH"
put "$SOURCE_FILE"
quit
EOF

echo "Upload completed: $SOURCE_FILE -> $REMOTE_PATH"
