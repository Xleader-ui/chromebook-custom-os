#!/bin/bash
# build_iso.sh -- packages kernel.bin + grub.cfg into a bootable ISO.
set -e

cp kernel.bin iso/boot/kernel.bin
grub-mkrescue -o os.iso iso

echo ""
echo "==> Done. Bootable image: os.iso"
echo "    Write it to a USB stick with, e.g.:"
echo "    sudo dd if=os.iso of=/dev/sdX bs=4M status=progress conv=fsync"
