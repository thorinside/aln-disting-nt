#!/usr/bin/env bash

set -euo pipefail

unset CDPATH
repo_root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
build_dir="$repo_root/build"
release_dir="$repo_root/release"
staging_dir="$release_dir/staging"

rm -rf -- "$release_dir"
mkdir -p "$staging_dir"

package_plugin() {
    local object_name="$1"
    local archive_name="$2"
    local display_name="$3"
    local package_root="$staging_dir/$archive_name"

    mkdir -p "$package_root/programs/plug-ins"
    cp "$build_dir/$object_name" "$package_root/programs/plug-ins/$object_name"
    printf '%s\n' \
        "Copy programs/plug-ins/$object_name to the same path on the disting NT MicroSD card." \
        "The plug-in appears as '$display_name' and requires firmware 1.15 or later." \
        >"$package_root/INSTALL.txt"
    (
        cd "$package_root"
        zip -q -r "$release_dir/$archive_name.zip" programs INSTALL.txt
    )
    cp "$build_dir/$object_name" "$release_dir/$object_name"
}

package_plugin "aln_fold_wavefolder.o" "aln-fold-wavefolder" "ALN Fold Wavefolder"
package_plugin "aln_distortion_bank.o" "aln-distortion-bank" "ALN Distortion Bank"

rm -rf -- "$staging_dir"
unzip -l "$release_dir/aln-fold-wavefolder.zip"
unzip -l "$release_dir/aln-distortion-bank.zip"
