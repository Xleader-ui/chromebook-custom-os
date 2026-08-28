#!/usr/bin/env python3
"""
compile_image.py

Orchestrates debootstrap + grub to produce a bootable 32-bit (i386) disk
image for a Chromebook running unlocked/custom firmware (e.g. MrChromebox
Legacy Boot or UEFI).

MUST be run as root, on Linux (bare metal, VM, Docker container, or a
GitHub Actions ubuntu-latest runner). It will NOT run on macOS directly --
see README.md for the Docker and GitHub Actions build paths.
"""

import os
import shutil
import subprocess
import sys

import config

REQUIRED_TOOLS = ["debootstrap", "parted", "kpartx", "mkfs.ext4", "grub-install", "rsync"]


def run(cmd, **kwargs):
    print(f"+ {' '.join(cmd)}")
    subprocess.run(cmd, check=True, **kwargs)


def check_root():
    if os.geteuid() != 0:
        sys.exit("This script must be run as root (sudo python3 compile_image.py).")


def check_tools():
    missing = [t for t in REQUIRED_TOOLS if shutil.which(t) is None]
    if missing:
        sys.exit(
            "Missing required tools: "
            + ", ".join(missing)
            + "\nOn Debian/Ubuntu: sudo apt-get install debootstrap parted kpartx "
            "dosfstools grub-pc-bin grub-efi-ia32-bin xorriso rsync"
        )


def debootstrap_rootfs():
    if os.path.exists(config.ROOTFS_DIR) and os.listdir(config.ROOTFS_DIR):
        print(f"==> Rootfs already exists at {config.ROOTFS_DIR}, skipping debootstrap")
        return
    os.makedirs(config.ROOTFS_DIR, exist_ok=True)

    # Two-stage debootstrap so we can drop in a policy-rc.d stub before the
    # second stage runs. Without it, packages like dconf-service/polkitd try
    # to actually start their systemd service during postinst, which fails
    # in CI runners / containers that don't have a real init system, and
    # takes the whole debootstrap down with it.
    run([
        "debootstrap",
        "--foreign",
        f"--arch={config.ARCH}",
        "--include=" + ",".join(config.PACKAGES),
        config.DISTRO,
        config.ROOTFS_DIR,
        config.MIRROR,
    ])

    policy_path = os.path.join(config.ROOTFS_DIR, "usr", "sbin", "policy-rc.d")
    os.makedirs(os.path.dirname(policy_path), exist_ok=True)
    with open(policy_path, "w") as f:
        f.write("#!/bin/sh\nexit 101\n")
    os.chmod(policy_path, 0o755)

    run(["chroot", config.ROOTFS_DIR, "/debootstrap/debootstrap", "--second-stage"])

    # Leave policy-rc.d in place for now -- configure_chroot() still needs it
    # while it runs `systemctl enable` calls (enabling doesn't start anything,
    # but some postinst scripts double-check). It gets removed at the end of
    # configure_chroot() so the real system can start services normally.


def configure_chroot():
    print("==> Configuring system inside chroot")
    setup_src = os.path.join("scripts", "chroot_setup.sh")
    setup_dst = os.path.join(config.ROOTFS_DIR, "chroot_setup.sh")

    with open(setup_src) as f:
        script = f.read()
    script = script.replace("__HOSTNAME__", config.HOSTNAME)
    script = script.replace("__ROOT_PASSWORD__", config.ROOT_PASSWORD)

    with open(setup_dst, "w") as f:
        f.write(script)
    os.chmod(setup_dst, 0o755)

    # bind mount pseudo-filesystems needed for chroot to work properly
    for fs in ["/dev", "/proc", "/sys"]:
        target = config.ROOTFS_DIR + fs
        os.makedirs(target, exist_ok=True)
        run(["mount", "--bind", fs, target])

    try:
        run(["chroot", config.ROOTFS_DIR, "/bin/bash", "/chroot_setup.sh"])
    finally:
        for fs in ["/dev", "/proc", "/sys"]:
            target = config.ROOTFS_DIR + fs
            subprocess.run(["umount", "-lf", target])

    os.remove(setup_dst)

    # Remove the policy-rc.d stub now that all package configuration is done,
    # so the real system boots and starts services normally.
    policy_path = os.path.join(config.ROOTFS_DIR, "usr", "sbin", "policy-rc.d")
    if os.path.exists(policy_path):
        os.remove(policy_path)


def build_disk_image():
    print("==> Building raw disk image")
    os.makedirs(config.WORK_DIR, exist_ok=True)
    image_path = os.path.join(config.WORK_DIR, config.IMAGE_NAME)

    # 1. Create empty image file
    run(["fallocate", "-l", f"{config.IMAGE_SIZE_MB}M", image_path])

    # 2. Partition: single bootable ext4 partition (BIOS boot flag set for grub-pc)
    run(["parted", "-s", image_path, "mklabel", "msdos"])
    run(["parted", "-s", image_path, "mkpart", "primary", "ext4", "1MiB", "100%"])
    run(["parted", "-s", image_path, "set", "1", "boot", "on"])

    # 3. Map partitions to loop devices
    run(["kpartx", "-av", image_path])
    loop_base = subprocess.check_output(
        ["losetup", "-j", image_path]
    ).decode().split(":")[0].split("/")[-1]
    part_dev = f"/dev/mapper/{loop_base}p1"

    try:
        # 4. Format + mount
        run(["mkfs.ext4", "-L", "rootfs", part_dev])
        mount_point = os.path.join(config.WORK_DIR, "mnt")
        os.makedirs(mount_point, exist_ok=True)
        run(["mount", part_dev, mount_point])

        try:
            # 5. Copy rootfs in
            run(["rsync", "-aHAX", config.ROOTFS_DIR + "/", mount_point + "/"])

            # 6. Install grub onto the loop device representing the whole disk
            loop_disk = f"/dev/{loop_base}"
            for fs in ["/dev", "/proc", "/sys"]:
                os.makedirs(mount_point + fs, exist_ok=True)
                run(["mount", "--bind", fs, mount_point + fs])
            try:
                if config.BOOT_MODE in ("bios", "both"):
                    run(["chroot", mount_point, "grub-install",
                         "--target=i386-pc", "--recheck", loop_disk])
                if config.BOOT_MODE in ("uefi", "both"):
                    run(["chroot", mount_point, "grub-install",
                         "--target=i386-efi", "--efi-directory=/boot/efi",
                         "--removable", "--recheck"])
                run(["chroot", mount_point, "update-grub"])
            finally:
                for fs in ["/dev", "/proc", "/sys"]:
                    subprocess.run(["umount", "-lf", mount_point + fs])
        finally:
            run(["umount", mount_point])
    finally:
        run(["kpartx", "-dv", image_path])

    print(f"\n==> Done. Image written to: {image_path}")
    print("    Write it to a USB stick with, e.g.:")
    print(f"    sudo dd if={image_path} of=/dev/sdX bs=4M status=progress conv=fsync")


def main():
    check_root()
    check_tools()
    debootstrap_rootfs()
    configure_chroot()
    build_disk_image()


if __name__ == "__main__":
    main()
