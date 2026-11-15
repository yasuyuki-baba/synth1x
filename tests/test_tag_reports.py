"""Authored tag inputs, source identity and read-only diagnostic contracts."""
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from tag_patches import compare, scan


def suggestion(tag, rule="name.word", detail="An authored reason."):
    return {"tag": tag, "reasons": [{"rule": rule, "detail": detail}]}


def patch(key, tags, source=b"authored"):
    return {"id": key, "name": "Authored", "source_sha256": hashlib.sha256(source).hexdigest(),
            "error": "", "tags": tags}


def report(patches):
    return {"schema_version": 1, "inference_revision": 3, "patches": patches}


class TagComparison(unittest.TestCase):
    def test_rule_differences_keep_added_removed_and_reason_changes_separate(self):
        before = report([patch("same", [suggestion("Pad"), suggestion("Poly")]),
                         patch("removed", [])])
        after = report([patch("same", [suggestion("Wind"), suggestion("Poly", detail="New reason.")]),
                        patch("added", [])])
        delta = compare(before, after)
        self.assertEqual(delta["summary"]["same_source_tag_changes"], 1)
        self.assertEqual(delta["summary"]["same_source_reason_changes"], 1)
        self.assertEqual(delta["summary"]["added_tags"], {"Wind": 1})
        self.assertEqual(delta["summary"]["removed_tags"], {"Pad": 1})
        self.assertEqual(delta["added_sources"][0]["id"], "added")
        self.assertEqual(delta["removed_sources"][0]["id"], "removed")
        change = delta["changes"][0]
        self.assertEqual(change["added"][0]["reasons"][0]["rule"], "name.word")
        self.assertEqual(change["reasons_changed"][0]["tag"], "Poly")

    def test_source_edits_are_visible_without_being_counted_as_rule_changes(self):
        before = report([patch("same", [suggestion("Pad")])])
        after = report([patch("same", [suggestion("Wind")], b"edited")])
        after["patches"][0]["error"] = "Invalid source"
        delta = compare(before, after)
        self.assertEqual(delta["summary"]["changed_sources"], 1)
        self.assertEqual(delta["summary"]["changed_errors"], 1)
        self.assertEqual(delta["summary"]["same_source_tag_changes"], 0)
        self.assertEqual(delta["summary"]["added_tags"], {})
        self.assertTrue(delta["changes"][0]["source_changed"])
        self.assertEqual(delta["changes"][0]["error_after"], "Invalid source")

    def test_reordering_is_ignored_and_ambiguous_or_incompatible_reports_are_rejected(self):
        before = report([patch("same", [suggestion("Pad"), suggestion("Poly")])])
        after = copy.deepcopy(before)
        after["patches"][0]["tags"].reverse()
        self.assertEqual(compare(before, after)["changes"], [])
        after["patches"].append(after["patches"][0])
        with self.assertRaises(ValueError):
            compare(before, after)
        after = copy.deepcopy(before)
        after["schema_version"] = 99
        with self.assertRaises(ValueError):
            compare(before, after)


@unittest.skipUnless(os.environ.get("SYNTH2_TAG_PROBE_BIN"), "Tag probe target not selected")
class TagProbe(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.probe = Path(os.environ["SYNTH2_TAG_PROBE_BIN"])

    def tearDown(self):
        self.temporary.cleanup()

    def test_archive_and_directory_share_resolved_tags_and_preserve_sources(self):
        source = 'WND "Amber" 管\nver=113\n38,0\n25,100\n'.encode("utf-8")
        folder = self.root / "音色"
        folder.mkdir()
        file = folder / "one.sy1"
        file.write_bytes(source)
        archive = self.root / "bank.zip"
        with zipfile.ZipFile(archive, "w") as bank:
            bank.writestr("Nested/one.sy1", source)
            bank.writestr("Other/one.sy1", source)
        original = archive.read_bytes()
        directory_report = scan(folder, self.probe)
        archive_report = scan(archive, self.probe)
        self.assertEqual(directory_report["summary"]["valid"], 1)
        self.assertEqual(archive_report["summary"]["valid"], 2)
        entries = directory_report["patches"] + archive_report["patches"]
        for entry in entries:
            self.assertEqual(entry["name"], 'WND "Amber" 管')
            self.assertEqual(entry["source_sha256"], hashlib.sha256(source).hexdigest())
            self.assertEqual(entry["tags"], entries[0]["tags"])
        tags = {tag["tag"]: tag["reasons"] for tag in entries[0]["tags"]}
        self.assertEqual(tags["Wind"][0]["rule"], "name.prefix")
        self.assertEqual(tags["Poly"][0]["rule"], "settings.voice-mode")
        self.assertNotEqual(archive_report["patches"][0]["id"], archive_report["patches"][1]["id"])
        self.assertEqual(list(folder.iterdir()), [file])
        self.assertEqual(file.read_bytes(), source)
        self.assertEqual(archive.read_bytes(), original)

    def test_cli_records_load_errors_returns_failure_and_preserves_existing_output(self):
        source = self.root / "bad.sy1"
        source.write_bytes(b"Authored\nver=999\n0,0\n")
        output = self.root / "report.json"
        command = [sys.executable, str(ROOT / "tools/tag_patches.py"), "scan", str(source),
                   "--probe", str(self.probe), "--output", str(output)]
        result = subprocess.run(command, capture_output=True)
        self.assertEqual(result.returncode, 2, result.stderr)
        data = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(data["summary"]["invalid"], 1)
        self.assertTrue(data["patches"][0]["error"])
        self.assertEqual(data["patches"][0]["tags"], [])
        saved = output.read_bytes()
        self.assertEqual(subprocess.run(command, capture_output=True).returncode, 1)
        self.assertEqual(output.read_bytes(), saved)

    def test_invalid_legacy_title_bytes_keep_exact_source_hash_and_valid_json(self):
        source = self.root / "legacy.sy1"
        data = b'KEY \xff"Room" \\Space\t\nver=113\n'
        source.write_bytes(data)
        result = scan(source, self.probe)["patches"][0]
        self.assertIn("\ufffd", result["name"])
        self.assertIn('"Room"', result["name"])
        self.assertEqual(result["source_sha256"], hashlib.sha256(data).hexdigest())


if __name__ == "__main__":
    unittest.main()
