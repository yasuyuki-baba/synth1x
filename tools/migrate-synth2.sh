#!/usr/bin/env bash
# Copy Synth2 user data on macOS or Linux. Preview is the default.
set -euo pipefail
shopt -s dotglob nullglob
umask 077

apply=false
case "${1-}" in
    --apply) apply=true ;;
    -h|--help) printf 'Usage: bash migrate-synth2.sh [--apply]\n'; exit 0 ;;
    '') ;;
    *) printf 'Usage: bash migrate-synth2.sh [--apply]\n' >&2; exit 2 ;;
esac
if [[ $# -gt 1 ]]; then
    printf 'Usage: bash migrate-synth2.sh [--apply]\n' >&2
    exit 2
fi

case "$(uname -s)" in
    Darwin)
        data_home="${HOME:?HOME is required}/Library/Application Support"
        config_home="$data_home"
        ;;
    Linux)
        data_home="${XDG_DATA_HOME:-${HOME:?HOME is required}/.local/share}"
        config_home="${XDG_CONFIG_HOME:-${HOME:?HOME is required}/.config}"
        if [[ "$config_home" != /* ]]; then
            config_home="${HOME:?HOME is required}/.config"
        fi
        ;;
    *)
        printf 'This script supports macOS and Linux.\n' >&2
        exit 2
        ;;
esac
if [[ "$data_home" != /* || "$config_home" != /* ]]; then
    printf 'The user data and settings paths must be absolute.\n' >&2
    exit 2
fi

stage=''
cleanup() {
    if [[ -n "$stage" ]]; then
        rm -f -- "$stage/$staged_name"
        rmdir -- "$stage"
    fi
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

copy_item() {
    local source="$1" destination="$2" child parent
    if [[ -L "$source" || -L "$destination" ]]; then
        printf 'Skip symbolic link: %s -> %s\n' "$source" "$destination"
    elif [[ -d "$source" ]]; then
        if [[ -e "$destination" && ! -d "$destination" ]]; then
            printf 'Keep existing: %s\n' "$destination"
            return
        fi
        for child in "$source"/*; do
            case "${child##*/}" in
                .synth2-tags.cache|.tags.cache|.archives.cache|.synth2-write-*|.patch-write-*) continue ;;
            esac
            if [[ "$child" == "$data_home/Synth2/preset-directory" ]]; then
                printf 'Custom library: copy the folder and select its copy in Synth1X (see MIGRATION.md).\n'
                continue
            fi
            copy_item "$child" "$destination/${child##*/}"
        done
    elif [[ -f "$source" ]]; then
        if [[ -e "$destination" ]]; then
            printf 'Keep existing: %s\n' "$destination"
            return
        fi
        printf 'Copy: %s -> %s\n' "$source" "$destination"
        if "$apply"; then
            parent="${destination%/*}"
            mkdir -p -- "$parent"
            staged_name="${destination##*/}"
            stage=$(mktemp -d "$parent/.migration.XXXXXXXX")
            cp -p -- "$source" "$stage/$staged_name"
            # Publish a complete file, with no overwrite if the name was just created.
            if ! ln -- "$stage/$staged_name" "$parent/"; then
                if [[ ! -e "$destination" && ! -L "$destination" ]]; then
                    return 1
                fi
                printf 'Keep existing: %s\n' "$destination"
            fi
            cleanup
            stage=''
        fi
    elif [[ -e "$source" ]]; then
        printf 'Skip special file: %s\n' "$source"
    fi
}

printf 'User data: %s/Synth2 -> %s/Synth1X\n' "$data_home" "$data_home"
if [[ -d "$data_home/Synth2" || -L "$data_home/Synth2" ]]; then
    copy_item "$data_home/Synth2" "$data_home/Synth1X"
else
    printf 'No Synth2 user data folder found.\n'
fi
if [[ "$config_home" != "$data_home" ]]; then
    printf 'Standalone settings: %s/Synth2/settings.ini -> %s/Synth1X/settings.ini\n' "$config_home" "$config_home"
    if [[ -L "$config_home/Synth2" || -L "$config_home/Synth1X" ]]; then
        printf 'Skip standalone settings in symbolic-link folder.\n'
    elif [[ -f "$config_home/Synth2/settings.ini" || -L "$config_home/Synth2/settings.ini" ]]; then
        copy_item "$config_home/Synth2/settings.ini" "$config_home/Synth1X/settings.ini"
    fi
fi
if ! "$apply"; then
    printf 'Preview only. Close your DAW and both apps, then rerun with --apply to copy.\n'
fi
