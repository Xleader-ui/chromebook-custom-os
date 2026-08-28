"""
Build configuration for the custom Chromebook OS image.
Edit these values, then run: sudo python3 compile_image.py
"""

# --- Target system ---
ARCH = "i386"                  # 32-bit x86. Use "amd64" if your Chromebook is actually 64-bit.
DISTRO = "bookworm"            # Debian release codename (bookworm = Debian 12)
MIRROR = "http://deb.debian.org/debian"

# --- Boot mode ---
# "bios"  -> legacy/SeaBIOS boot (matches MrChromebox "Legacy Boot" firmware)
# "uefi"  -> 32-bit UEFI boot (matches MrChromebox "UEFI (Full ROM)" firmware)
# "both"  -> hybrid image that supports either
BOOT_MODE = "bios"

# --- Image output ---
IMAGE_NAME = "custom-os.img"   # raw disk image, dd this to a USB stick
IMAGE_SIZE_MB = 4096           # total image size
HOSTNAME = "custom-os"
ROOT_PASSWORD = "changeme"     # CHANGE THIS before building

# --- Packages installed into the image ---
# Kept minimal on purpose -- add whatever DE/WM or tools you want.
PACKAGES = [
    "linux-image-686",         # 32-bit kernel (use linux-image-amd64 if ARCH=amd64)
    "grub-pc",                 # BIOS bootloader
    "grub-efi-ia32",           # 32-bit UEFI bootloader (only needed if BOOT_MODE uses uefi)
    "systemd-sysv",
    "network-manager",
    "sudo",
    "openssh-server",
    "vim",
    "curl",
    "wget",
    "xserver-xorg",
    "xinit",
    "openbox",                 # lightweight window manager; swap for whatever you like
    "lightdm",
]

# --- Working directories (relative to this folder) ---
ROOTFS_DIR = "build/rootfs"
WORK_DIR = "build"
