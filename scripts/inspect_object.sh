#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -eq 0 ]; then
    echo "usage: inspect_object.sh PLUGIN.o [...]" >&2
    exit 2
fi

arm_nm="${ARM_NM:-arm-none-eabi-nm}"
arm_readelf="${ARM_READELF:-arm-none-eabi-readelf}"
arm_size="${ARM_SIZE:-arm-none-eabi-size}"

for object in "$@"; do
    header="$($arm_readelf -h "$object")"
    grep -Eq 'Class:[[:space:]]+ELF32' <<<"$header"
    grep -Eq 'Data:[[:space:]]+2.s complement, little endian' <<<"$header"
    grep -Eq 'Type:[[:space:]]+REL \(Relocatable file\)' <<<"$header"
    grep -Eq 'Machine:[[:space:]]+ARM' <<<"$header"

    if ! "$arm_nm" -g --defined-only "$object" | awk '{print $NF}' | grep -qx pluginEntry; then
        echo "$object: pluginEntry is not globally defined" >&2
        exit 1
    fi

    unexpected="$($arm_nm -u "$object" \
        | awk '{print $NF}' \
        | grep -Ev '^(NT_globals|_GLOBAL_OFFSET_TABLE_|expf|memset)$' \
        || true)"
    if [ -n "$unexpected" ]; then
        echo "$object: unexpected undefined symbols:" >&2
        echo "$unexpected" >&2
        exit 1
    fi

    echo "$object: ELF32 little-endian ARM relocatable; pluginEntry present"
    "$arm_size" -A "$object"
    "$arm_nm" -u "$object"
done
