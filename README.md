# Custom Chromebook OS Image Builder

Builds a bootable 32-bit (i386) Debian-based image for a Chromebook running
unlocked/custom firmware (MrChromebox Legacy Boot or UEFI).

Edit `config.py` first -- at minimum change `ROOT_PASSWORD`, and set
`BOOT_MODE` to match how you flashed your Chromebook's firmware.

You have three ways to actually run the build, pick whichever is easiest:

## Option A: GitHub Actions (recommended if you're on a Mac)
No local Linux needed at all -- the build runs on GitHub's servers.

1. Push this folder to a new GitHub repo.
2. Go to the **Actions** tab -> **Build Custom OS Image** -> **Run workflow**.
3. Wait for it to finish (~10-15 min), then download the `custom-os-image`
   artifact from the completed run. It contains `custom-os.img`.
4. `dd` it to a USB stick (see below).

This also happens automatically on every push to `main`.

## Option B: Docker on your Mac
Requires Docker Desktop installed and running.

```bash
cd chromebook-custom-os
chmod +x build_on_mac.sh
./build_on_mac.sh
```

The finished image lands in `build/custom-os.img`.

## Option C: Native Linux machine or VM
```bash
cd chromebook-custom-os
sudo apt-get install debootstrap parted kpartx dosfstools \
    grub-pc-bin grub-efi-ia32-bin grub-common xorriso rsync
sudo python3 compile_image.py
```

## Writing the image to USB
On Mac:
```bash
diskutil list                 # find your USB stick, e.g. /dev/disk4
diskutil unmountDisk /dev/disk4
sudo dd if=build/custom-os.img of=/dev/rdisk4 bs=4m status=progress
diskutil eject /dev/disk4
```
(Use the raw device `/dev/rdiskN`, not `/dev/diskN` -- much faster on Mac.)

On Linux:
```bash
sudo dd if=build/custom-os.img of=/dev/sdX bs=4M status=progress conv=fsync
```

## Booting it on the Chromebook
1. Make sure you've already unlocked the firmware via MrChromebox's utility
   (see the earlier steps -- Developer Mode + firmware flash + removing the
   write-protect screw if your board needs it).
2. Insert the USB stick, power on, press **Esc** (BIOS mode) or the boot
   menu key for UEFI, and select the USB drive.
3. It should boot straight into the image built above.

## Notes / things you'll likely want to tweak
- `PACKAGES` in `config.py` installs a minimal `openbox` + `lightdm` desktop.
  Swap in `xfce4`, `i3`, etc. if you want something else, or strip it down
  further for a headless box.
- `IMAGE_SIZE_MB` defaults to 4GB -- bump it up if you're installing more.
- If your Chromebook is actually 64-bit (most modern ones are), you can set
  `ARCH = "amd64"` and swap `linux-image-686` for `linux-image-amd64` in
  `config.py` -- everything else in this build stays the same.
- GitHub-hosted runners have ~14GB of free disk, so keep `IMAGE_SIZE_MB`
  comfortably under that if building via Option A.
