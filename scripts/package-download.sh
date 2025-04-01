#!/bin/bash
set -e # Error handling mechanism inside this script

ROOT=$(dirname "$(dirname "${BASH_SOURCE[@]}")")
PKG="$ROOT/package"

REPO="barcoopensource/di-qif-helper"
VERSION="1.0.0"
QT_STAT_ASSET="static-qt-660.zip"
BZIP2_ASSET="bzip2.zip"
XZ_ASSET="xz.zip"
ZLIB_ASSET="zlib.zip"

QT_STAT_PKG="$PKG/qt-static-6.6.0"
QT_JOM_PKG="$PKG/jom"
BZIP2_PKG="$PKG/bzip2"
XZ_PKG="$PKG/xz"
ZLIB_PKG="$PKG/zlib"

echo "Cleanup previous build"
rm -rf "$QT_STAT_PKG"
rm -rf "$QT_JOM_PKG"
rm -rf "$BZIP2_PKG"
rm -rf "$XZ_PKG"
rm -rf "$ZLIB_PKG"
mkdir -p "$PKG"


echo "Fetching jom package"
curl -L -o "$QT_JOM_PKG.zip" "https://download.qt.io/official_releases/jom/jom.zip"

echo "Fetching Qt_Static-6.6.0 package"
gh release download --repo "$REPO" "$VERSION" \
    --pattern "$QT_STAT_ASSET" --output "$QT_STAT_PKG.zip"
	
echo "Fetching bzip2 package"
gh release download --repo "$REPO" "$VERSION" \
    --pattern "$BZIP2_ASSET" --output "$BZIP2_PKG.zip"
	
echo "Fetching xz package"
gh release download --repo "$REPO" "$VERSION" \
    --pattern "$XZ_ASSET" --output "$XZ_PKG.zip"

echo "Fetching zlib package"
gh release download --repo "$REPO" "$VERSION" \
    --pattern "$ZLIB_ASSET" --output "$ZLIB_PKG.zip"

echo "Unzipping packages"	
unzip "$QT_STAT_PKG.zip" -d "$QT_STAT_PKG"
unzip "$QT_JOM_PKG.zip" -d "$QT_JOM_PKG"
unzip "$BZIP2_PKG.zip" -d "$BZIP2_PKG"
unzip "$XZ_PKG.zip" -d "$XZ_PKG"
unzip "$ZLIB_PKG.zip" -d "$ZLIB_PKG"