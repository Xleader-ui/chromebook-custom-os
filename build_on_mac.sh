#!/bin/bash
# Run this on your Mac (with Docker Desktop installed & running) instead of
# running compile_image.py directly. It builds the Docker image, then runs
# the compiler inside a privileged container so loop devices work, and
# copies the finished .img file back out to build/ on your Mac.
set -e

IMAGE_TAG="chromebook-os-builder"

echo "==> Building Docker build environment..."
docker build -t "$IMAGE_TAG" .

echo "==> Running image compiler inside container (needs --privileged for loop devices)..."
docker run --rm --privileged \
    -v "$(pwd)/build:/work/build" \
    "$IMAGE_TAG"

echo "==> Done. Your image should be in ./build/"
ls -lh build/*.img 2>/dev/null || echo "(no .img found -- check the log above for errors)"
