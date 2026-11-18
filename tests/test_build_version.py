"""Validate generated product metadata without native SDKs or repository tags."""
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CMAKE = os.environ.get('CMAKE_COMMAND') or shutil.which('cmake')


@unittest.skipUnless(CMAKE, 'CMake is required')
class BuildVersion(unittest.TestCase):
    def generate(self, tag=None, github=False):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        source = root / 'source'
        source.mkdir()
        # Source snapshots have no Git directory; no outside repository is consulted.
        script = root / 'version.cmake'
        script.write_text(f'include("{ROOT.as_posix()}/cmake/Synth1XVersion.cmake")\n'
                          f'synth1x_configure_version("{source.as_posix()}" "{root.as_posix()}/out")\n')
        env = {key: value for key, value in os.environ.items()
               if key not in ('GITHUB_ACTIONS', 'GITHUB_REF_TYPE', 'GITHUB_REF_NAME')}
        env['SOURCE_DATE_EPOCH'] = '1767225600'  # 2026-01-01 UTC
        command = [CMAKE]
        if github:
            env.update(GITHUB_ACTIONS='true', GITHUB_REF_TYPE='tag', GITHUB_REF_NAME=tag)
        elif tag is not None:
            command.append('-DSYNTH1X_RELEASE_TAG=' + tag)
        result = subprocess.run(command + ['-P', str(script)], env=env,
                                capture_output=True, text=True)
        return root / 'out', result

    def check_metadata(self, output, numeric, display, packed):
        header = (output / 'BuildVersion.h').read_text()
        self.assertIn(f'"{display}"', header)
        word = re.search(r'SYNTH1X_VERSION_HEX (0x[0-9a-f]+)', header).group(1)
        self.assertEqual(int(word, 16), packed)
        bundle_formats = {'AU': 'audiounit', 'CLAP': 'clap', 'VST3': 'vst3', 'macOS': 'app'}
        for format, identifier in bundle_formats.items():
            data = plistlib.loads((output / f'Synth1X-{format}-Info.plist').read_bytes())
            self.assertEqual(data['CFBundleExecutable'], 'Synth1X')
            self.assertEqual(data['CFBundleIdentifier'], f'io.github.yasuyuki-baba.{identifier}.Synth1X')
            self.assertEqual(data['CFBundleVersion'], numeric)
            self.assertEqual(data['CFBundleShortVersionString'], numeric)
            self.assertIn(display, data['CFBundleGetInfoString'])
            if format == 'VST3':
                self.assertEqual(data['CFBundleSignature'], 'Sy1X')
            if format == 'AU':
                component = data['AudioComponents'][0]
                self.assertEqual(component['version'], packed)
                self.assertEqual(component['manufacturer'], 'BABA')
                self.assertEqual(component['subtype'], 'Sy1X')
                self.assertEqual(data['CFBundleSignature'], component['subtype'])
                self.assertEqual(component['factoryFunction'], 'Synth1X_Factory')
                self.assertEqual(data['NSPrincipalClass'], 'Synth1X_View')
            if format == 'macOS':
                self.assertEqual(data['NSMainNibFile'], 'Synth1X-macOS-MainMenu')

    def test_release_and_github_tag_use_the_same_metadata(self):
        for github in (False, True):
            with self.subTest(github=github):
                output, result = self.generate('v2.3.4', github)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.check_metadata(output, '2.3.4', '2.3.4', 0x20304)

    def test_local_source_archive_has_reproducible_date_label(self):
        output, result = self.generate()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.check_metadata(output, '0.0.0', '0.0.0-dev.20260101', 0)

    def test_prerelease_keeps_numeric_bundle_fields(self):
        output, result = self.generate('v0.2.0-rc.1')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.check_metadata(output, '0.2.0', '0.2.0-rc.1', 0x200)

    def test_invalid_or_overflowing_release_tags_fail(self):
        for tag in ('1.2.3', 'v01.2.3', 'v1.256.0', 'v1.0.256', 'v65536.0.0', 'v1.2.3;bad'):
            with self.subTest(tag=tag):
                _, result = self.generate(tag)
                self.assertNotEqual(result.returncode, 0)


if __name__ == '__main__':
    unittest.main()
