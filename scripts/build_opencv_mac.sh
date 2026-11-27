#!/usr/bin/env bash
# Builds a minimal, static OpenCV 4.10.0 for the macOS simulator build.
#
# Windows uses the prebuilt opencv_world DLL vendored in third_party/opencv.
# On macOS we build the same version from source, with only what watch_sim
# needs (core, imgproc, imgcodecs with PNG, highgui with Cocoa), as static
# libraries. watch_sim then links everything in and dist-mac/watch_sim is a
# single executable that runs on any Apple Silicon Mac, like dist/watch_sim.exe.
#
#   scripts/build_opencv_mac.sh          # -> third_party/opencv-mac/
#
# Both the work dir and the install dir are git-ignored; rerun the script to
# recreate them. Needs: Xcode command line tools, cmake, curl.
set -euo pipefail

VERSION=4.10.0
SHA256=b2171af5be6b26f7a06b1229948bbb2bdaa74fcf5cd097e0af6378fce50a6eb9
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$ROOT/third_party/opencv-mac-build"
PREFIX="$ROOT/third_party/opencv-mac"
TARBALL="$WORK/opencv-$VERSION.tar.gz"

mkdir -p "$WORK"
if [[ ! -f "$TARBALL" ]]; then
  curl -fsSL -o "$TARBALL" "https://github.com/opencv/opencv/archive/refs/tags/$VERSION.tar.gz"
fi
echo "$SHA256  $TARBALL" | shasum -a 256 -c -

if [[ ! -d "$WORK/opencv-$VERSION" ]]; then
  tar -xzf "$TARBALL" -C "$WORK"
fi

cmake -S "$WORK/opencv-$VERSION" -B "$WORK/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_LIST=core,imgproc,imgcodecs,highgui \
  -DBUILD_ZLIB=ON -DBUILD_PNG=ON \
  -DWITH_PNG=ON \
  -DWITH_JPEG=OFF -DWITH_TIFF=OFF -DWITH_WEBP=OFF -DWITH_OPENJPEG=OFF \
  -DWITH_JASPER=OFF -DWITH_OPENEXR=OFF -DWITH_IMGCODEC_HDR=OFF \
  -DWITH_IMGCODEC_SUNRASTER=OFF -DWITH_IMGCODEC_PXM=OFF -DWITH_IMGCODEC_PFM=OFF \
  -DWITH_FFMPEG=OFF -DWITH_AVFOUNDATION=OFF -DWITH_GSTREAMER=OFF \
  -DWITH_OPENCL=OFF -DWITH_OPENCLAMDFFT=OFF -DWITH_OPENCLAMDBLAS=OFF \
  -DWITH_LAPACK=OFF -DWITH_EIGEN=OFF -DWITH_PROTOBUF=OFF -DWITH_ADE=OFF \
  -DWITH_ITT=OFF -DWITH_IPP=OFF -DWITH_OPENMP=OFF -DWITH_TBB=OFF \
  -DWITH_QT=OFF -DWITH_GTK=OFF -DWITH_VTK=OFF -DWITH_OBSENSOR=OFF \
  -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF \
  -DBUILD_DOCS=OFF -DBUILD_opencv_apps=OFF -DBUILD_JAVA=OFF \
  -DBUILD_opencv_python3=OFF -DBUILD_opencv_python_bindings_generator=OFF \
  -DOPENCV_GENERATE_PKGCONFIG=OFF

cmake --build "$WORK/build" -j"$(sysctl -n hw.ncpu)"
cmake --install "$WORK/build"
echo "OpenCV $VERSION (static, minimal) installed to $PREFIX"
