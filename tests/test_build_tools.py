"""Candidate archive contracts with self-authored bundles and notice files."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

SPEC = importlib.util.spec_from_file_location(
    'package_native', Path(__file__).resolve().parents[1] / 'tools/package_native.py')
packager = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(packager)
SDK_SPEC = importlib.util.spec_from_file_location(
    'prepare_native_sdks', Path(__file__).resolve().parents[1] / 'tools/prepare_native_sdks.py')
sdk_setup = importlib.util.module_from_spec(SDK_SPEC)
SDK_SPEC.loader.exec_module(sdk_setup)


class CandidatePackages(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.build = self.root / 'build'
        self.output = self.root / 'dist'
        self.manifest = self.root / 'dependencies.json'
        self.manifest.write_text(json.dumps({'product_commit': 'a' * 40,
                                             'iplug2': 'b' * 40}))
        (self.root / 'MIGRATION.md').write_text('# Upgrading from Synth2\n')
        for name in [
            'LICENSE', 'INSTALL.md', 'third_party/miniz/LICENSE', 'third_party/tinyxml2/LICENSE',
            'third_party/libarchive/COPYING', 'third_party/libarchive/NOTICES',
            'third_party/xz/COPYING', 'third_party/xz/COPYING.0BSD',
            'projects/Synth2/resources/IPLUG2-LICENSE.txt',
            'projects/Synth2/resources/fonts/LICENSE.txt',
            'iPlug2/Dependencies/IPlug/VST3_SDK/LICENSE.txt',
            'iPlug2/Dependencies/IPlug/CLAP_SDK/LICENSE',
            'iPlug2/Dependencies/IPlug/CLAP_HELPERS/LICENSE',
            'iPlug2/WDL/license.txt',
        ]:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('Self-authored placeholder notice\n')

    def binary(self, relative):
        path = self.build / 'out' / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b'Self-authored binary placeholder')
        path.chmod(0o755)
        return path

    def test_windows_requires_all_products_and_excludes_unrelated_files(self):
        self.binary('Synth1X.vst3/Contents/x86_64-win/Synth1X.vst3')
        self.binary('Synth1X.clap')
        self.binary('Synth1X.exe')
        self.binary('unrelated.pdb')
        result = packager.package(self.root, self.build, 'windows',
                                  self.manifest, self.output)
        with zipfile.ZipFile(result) as archive:
            names = archive.namelist()
            for name in ['Synth1X/Synth1X.exe', 'Synth1X/Synth1X.clap',
                         'Synth1X/Synth1X.vst3/Contents/x86_64-win/Synth1X.vst3',
                         'Synth1X/notices/iPlug2/WDL/license.txt', 'Synth1X/LICENSE',
                         'Synth1X/INSTALL.md', 'Synth1X/notices/miniz-LICENSE',
                         'Synth1X/notices/libarchive-COPYING', 'Synth1X/notices/libarchive-NOTICES',
                         'Synth1X/notices/xz-COPYING', 'Synth1X/notices/xz-COPYING.0BSD',
                         'Synth1X/notices/tinyxml2-LICENSE', 'Synth1X/notices/fonts-LICENSE.txt',
                         'Synth1X/notices/iPlug2-LICENSE.txt', 'Synth1X/notices/VST3-LICENSE.txt',
                         'Synth1X/notices/CLAP-LICENSE', 'Synth1X/notices/CLAP-helpers-LICENSE']:
                self.assertIn(name, names)
            self.assertNotIn('Synth1X/unrelated.pdb', names)
            self.assertEqual(archive.read('Synth1X/INSTALL.md'),
                             (self.root / 'INSTALL.md').read_bytes())
            self.assertEqual(archive.read('Synth1X/MIGRATION.md'),
                             (self.root / 'MIGRATION.md').read_bytes())
            self.assertIn(b'INSTALL.md', archive.read('Synth1X/README.txt'))
            self.assertIn(b'MIGRATION.md', archive.read('Synth1X/README.txt'))
            self.assertEqual(archive.read('Synth1X/dependencies.json'),
                             self.manifest.read_bytes())
        with self.assertRaisesRegex(ValueError, 'replace'):
            packager.package(self.root, self.build, 'windows', self.manifest, self.output)

    @unittest.skipIf(os.name == 'nt', 'Symlink creation requires Windows developer privileges')
    def test_macos_preserves_executable_modes_and_bundle_symlinks(self):
        for suffix in ['vst3', 'clap', 'component', 'app']:
            self.binary(f'Synth1X.{suffix}/Contents/MacOS/Synth1X')
        (self.build / 'out/Synth1X.app/Contents/current').symlink_to('MacOS/Synth1X')
        result = packager.package(self.root, self.build, 'macos',
                                  self.manifest, self.output)
        with tarfile.open(result) as archive:
            link = archive.getmember('Synth1X/Synth1X.app/Contents/current')
            self.assertTrue(link.issym())
            self.assertEqual(link.linkname, 'MacOS/Synth1X')
            binary = archive.getmember('Synth1X/Synth1X.app/Contents/MacOS/Synth1X')
            self.assertEqual(binary.mode & 0o111, 0o111)
            self.assertIsNotNone(archive.getmember('Synth1X/dependencies.json'))
            self.assertEqual(archive.extractfile('Synth1X/INSTALL.md').read(),
                             (self.root / 'INSTALL.md').read_bytes())
            self.assertEqual(archive.extractfile('Synth1X/MIGRATION.md').read(),
                             (self.root / 'MIGRATION.md').read_bytes())

    @unittest.skipIf(os.name == 'nt', 'Unix archive modes and symlinks require Unix')
    def test_linux_preserves_products_resources_modes_and_symlinks(self):
        self.binary('Synth1X.vst3/Contents/x86_64-linux/Synth1X.so')
        self.binary('Synth1X.clap')
        self.binary('Synth1X')
        resource = self.binary('Synth1X.clap.resources/fonts/editor.ttf')
        self.binary('Synth1X.resources/img/panel.png')
        (resource.parent / 'current.ttf').symlink_to('editor.ttf')
        self.binary('unrelated.so')
        result = packager.package(self.root, self.build, 'linux',
                                  self.manifest, self.output)
        self.assertEqual(result.name, 'Synth1X-linux.tar.gz')
        with tarfile.open(result) as archive:
            for name in ['Synth1X/Synth1X', 'Synth1X/Synth1X.clap',
                         'Synth1X/Synth1X.vst3/Contents/x86_64-linux/Synth1X.so']:
                self.assertEqual(archive.getmember(name).mode & 0o111, 0o111)
            link = archive.getmember('Synth1X/Synth1X.clap.resources/fonts/current.ttf')
            self.assertTrue(link.issym())
            self.assertEqual(link.linkname, 'editor.ttf')
            self.assertEqual(archive.extractfile('Synth1X/Synth1X.resources/img/panel.png').read(),
                             b'Self-authored binary placeholder')
            self.assertEqual(archive.extractfile('Synth1X/dependencies.json').read(),
                             self.manifest.read_bytes())
            self.assertIn('Synth1X/INSTALL.md', archive.getnames())
            self.assertEqual(archive.extractfile('Synth1X/MIGRATION.md').read(),
                             (self.root / 'MIGRATION.md').read_bytes())
            self.assertIn('Synth1X/notices/iPlug2/WDL/license.txt', archive.getnames())
            self.assertNotIn('Synth1X/unrelated.so', archive.getnames())

    def test_linux_rejects_a_missing_or_wrong_architecture_bundle(self):
        self.binary('Synth1X.clap')
        self.binary('Synth1X')
        self.binary('Synth1X.vst3/Contents/MacOS/Synth1X')
        with self.assertRaisesRegex(ValueError, 'Missing native product'):
            packager.package(self.root, self.build, 'linux', self.manifest, self.output)
        self.assertFalse(self.output.exists())

    def test_partial_bundles_cannot_become_successful_candidates(self):
        self.binary('Synth1X.clap')
        self.binary('Synth1X.exe')
        self.binary('Synth1X.vst3/Contents/Resources/placeholder')
        with self.assertRaisesRegex(ValueError, 'Missing native product'):
            packager.package(self.root, self.build, 'windows', self.manifest, self.output)
        self.assertFalse(self.output.exists())


@unittest.skipUnless(shutil.which('git'), 'Git is required for local SDK fixtures')
class PinnedSDKPreparation(unittest.TestCase):
    def test_fresh_install_preserves_git_clean_crlf_and_rejects_edited_sdk(self):
        # No external checkout, network, user identity or signing key is needed.
        def git(path, *args):
            return subprocess.check_output(['git', '-C', str(path), *args],
                                           text=True, stderr=subprocess.PIPE).strip()

        def commit(path):
            git(path, 'add', '-A')
            git(path, '-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid',
                '-c', 'commit.gpgsign=false', 'commit', '-qm', 'Self-authored fixture')

        with tempfile.TemporaryDirectory() as directory, patch.dict(os.environ, {
                'GIT_CONFIG_NOSYSTEM': '1', 'GIT_CONFIG_GLOBAL': os.devnull}):
            base = Path(directory)
            source = base / 'format-sdk'
            source.mkdir()
            git(source, 'init', '-q')
            (source / 'control.h').write_bytes(b'// Self-authored SDK fixture\n')
            commit(source)
            sdk = base / 'iplug2'
            sdk.mkdir()
            git(sdk, 'init', '-q')
            placeholder = sdk / 'Dependencies/IPlug/Example/readme.txt'
            placeholder.parent.mkdir(parents=True)
            (sdk / '.gitattributes').write_bytes(b'*.txt text=auto\n')
            placeholder.write_bytes(b'Self-authored scaffold\n')
            commit(sdk)
            git(sdk, 'config', 'core.autocrlf', 'true')
            placeholder.write_bytes(b'Self-authored scaffold\r\n')
            product = base / 'product'
            product.mkdir()
            git(product, 'init', '-q')
            revision = git(source, 'rev-parse', 'HEAD')
            lock = {'iplug2': {'revision': git(sdk, 'rev-parse', 'HEAD')},
                    'native_sdks': {'example': {'repository': source.as_uri(),
                        'revision': revision, 'path': 'Dependencies/IPlug/Example'}}}
            (product / 'dependencies.lock.json').write_text(json.dumps(lock))
            commit(product)
            manifest = base / 'manifest.json'
            result = sdk_setup.prepare(product, sdk / '..' / sdk.name, manifest)
            self.assertEqual(result['example']['revision'], revision)
            self.assertFalse(git(sdk, 'diff', '--name-only', 'HEAD'))
            sdk_setup.prepare(product, sdk, manifest)
            self.assertEqual((placeholder.parent / 'control.h').read_bytes(),
                             b'// Self-authored SDK fixture\n')
            (placeholder.parent / 'control.h').write_bytes(b'// Modified SDK\n')
            with self.assertRaisesRegex(ValueError, 'local changes'):
                sdk_setup.prepare(product, sdk, manifest)


if __name__ == '__main__':
    unittest.main()
