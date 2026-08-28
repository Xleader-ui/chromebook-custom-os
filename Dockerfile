# Build environment for compiling the custom Chromebook OS image.
# This runs on the Linux VM inside Docker Desktop, so it works fine on macOS.
FROM debian:bookworm

RUN apt-get update && apt-get install -y \
    debootstrap \
    grub-pc-bin \
    grub-efi-ia32-bin \
    grub-common \
    xorriso \
    parted \
    kpartx \
    dosfstools \
    rsync \
    python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work
COPY . /work

ENTRYPOINT ["python3", "compile_image.py"]
