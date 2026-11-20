#!/usr/bin/env python3
"""Package the exact native products, retaining Unix executable modes, resources and symlinks."""
import argparse
import json
from pathlib import Path
import shutil
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PRODUCTS = {
    'linux': ['Synth1X.vst3', 'Synth1X.clap', 'Synth1X'],
    'windows': ['Synth1X.vst3', 'Synth1X.clap', 'Synth1X.exe'],
    'macos': ['Synth1X.vst3', 'Synth1X.clap', 'Synth1X.component', 'Synth1X.app'],
}


def package(root, build, platform, manifest, output):
    products = [build / 'out' / name for name in PRODUCTS[platform]]
    for path in products:
        binary = path
        if path.is_dir():
            if platform == 'linux':
                binary = path / 'Contents' / 'x86_64-linux' / 'Synth1X.so'
            elif platform == 'windows':
                binary = path / 'Contents' / 'x86_64-win' / 'Synth1X.vst3'
            else:
                binary = path / 'Contents' / 'MacOS' / 'Synth1X'
        if not binary.is_file() or binary.stat().st_size == 0:
            raise ValueError(f'Missing native product: {path}')
    metadata = json.loads(manifest.read_text())
    if not metadata.get('product_commit') or not metadata.get('iplug2'):
        raise ValueError('Dependency manifest lacks product/SDK identities')
    output.mkdir(parents=True, exist_ok=True)
    suffix = '.zip' if platform == 'windows' else '.tar.gz'
    archive = output / ('Synth1X-' + platform + suffix)
    if archive.exists():
        raise ValueError('Refusing to replace an existing native package')
    with tempfile.TemporaryDirectory() as temporary:
        staging = Path(temporary) / 'Synth1X'
        staging.mkdir()
        for path in products:
            destination = staging / path.name
            if path.is_dir():
                shutil.copytree(path, destination, symlinks=True)
            else:
                shutil.copy2(path, destination)
                resources = path.with_name(path.name + '.resources')
                if platform == 'linux' and resources.is_dir():
                    shutil.copytree(resources, staging / resources.name, symlinks=True)
        shutil.copy2(manifest, staging / 'dependencies.json')
        shutil.copy2(root / 'LICENSE', staging / 'LICENSE')
        shutil.copy2(root / 'INSTALL.md', staging / 'INSTALL.md')
        shutil.copy2(root / 'MIGRATION.md', staging / 'MIGRATION.md')
        migration_script = 'migrate-synth2.ps1' if platform == 'windows' else 'migrate-synth2.sh'
        shutil.copy2(root / 'tools' / migration_script, staging / migration_script)
        for source, destination in [
            ('third_party/miniz/LICENSE', 'notices/miniz-LICENSE'),
            ('third_party/libarchive/COPYING', 'notices/libarchive-COPYING'),
            ('third_party/libarchive/NOTICES', 'notices/libarchive-NOTICES'),
            ('third_party/xz/COPYING', 'notices/xz-COPYING'),
            ('third_party/xz/COPYING.0BSD', 'notices/xz-COPYING.0BSD'),
            ('third_party/tinyxml2/LICENSE', 'notices/tinyxml2-LICENSE'),
            ('projects/Synth1X/resources/IPLUG2-LICENSE.txt', 'notices/iPlug2-LICENSE.txt'),
            ('projects/Synth1X/resources/fonts/LICENSE.txt', 'notices/fonts-LICENSE.txt'),
            ('iPlug2/Dependencies/IPlug/VST3_SDK/LICENSE.txt', 'notices/VST3-LICENSE.txt'),
            ('iPlug2/Dependencies/IPlug/CLAP_SDK/LICENSE', 'notices/CLAP-LICENSE'),
            ('iPlug2/Dependencies/IPlug/CLAP_HELPERS/LICENSE', 'notices/CLAP-helpers-LICENSE'),
        ]:
            target = staging / destination
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(root / source, target)
        # Preserve bundled dependency notices, including WDL/NanoVG and SDK
        # subcomponent licenses, without treating them as project-owned code.
        sdk = root / 'iPlug2'
        for source in sdk.rglob('*'):
            if (source.is_file() and '.git' not in source.relative_to(sdk).parts
                    and source.name.lower().startswith(('license', 'copying'))):
                target = staging / 'notices/iPlug2' / source.relative_to(sdk)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
        (staging / 'README.txt').write_text(
            'Synth1X native package\n\n'
            'These unsigned binaries have not been certified for DAW or device use.\n'
            'Installation and unsigned-build instructions are in INSTALL.md.\n'
            'Upgrade instructions are in MIGRATION.md.\n'
            'Build identity and SDK revisions are in dependencies.json.\n'
            'Source: https://github.com/yasuyuki-baba/synth1x/tree/'
            + metadata['product_commit'] + '\n'
            'See LICENSE and notices for the applicable source/component terms.\n')
        if platform != 'windows':
            with tarfile.open(archive, 'w:gz', dereference=False) as result:
                result.add(staging, arcname='Synth1X')
        else:
            with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as result:
                for path in sorted(staging.rglob('*')):
                    if path.is_file():
                        result.write(path, path.relative_to(staging.parent))
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--platform', choices=PRODUCTS, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(package(ROOT, args.build.resolve(), args.platform,
                  args.manifest.resolve(), args.output.resolve()))


if __name__ == '__main__':
    main()
