#!/bin/bash
# Runs INSIDE the chroot to finish configuring the system.
# Variables are substituted in by compile_image.py before this is copied in.
set -e

HOSTNAME="__HOSTNAME__"
ROOT_PASSWORD="__ROOT_PASSWORD__"

echo "$HOSTNAME" > /etc/hostname
cat > /etc/hosts <<EOF
127.0.0.1   localhost
127.0.1.1   $HOSTNAME
EOF

echo "root:${ROOT_PASSWORD}" | chpasswd

# Basic fstab -- root partition, adjust label/UUID handling as needed
cat > /etc/fstab <<EOF
/dev/sda1   /   ext4    errors=remount-ro   0   1
EOF

# Enable networking + ssh + display manager at boot
systemctl enable NetworkManager || true
systemctl enable ssh || true
systemctl enable lightdm || true

# Clean apt caches to shrink image
apt-get clean
