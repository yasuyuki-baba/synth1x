#!/usr/bin/env python3
"""Inspect automatic tags and compare reports using the actual product loader."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SCHEMA_VERSION = 1


def indexed_patches(report):
    if not isinstance(report, dict) or report.get("schema_version") != SCHEMA_VERSION:
        raise ValueError("Unsupported tag report schema")
    if not isinstance(report.get("patches"), list):
        raise ValueError("Tag report must contain a patch list")
    indexed = {}
    for patch in report["patches"]:
        if not isinstance(patch, dict):
            raise ValueError("Invalid patch report")
        key = patch["id"]
        if not isinstance(key, str) or key in indexed:
            raise ValueError("Tag report contains an invalid or duplicate patch ID")
        digest = patch["source_sha256"]
        if not isinstance(digest, str) or len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
            raise ValueError("Invalid source hash in tag report")
        if not isinstance(patch["name"], str) or not isinstance(patch["error"], str):
            raise ValueError("Invalid patch name or error in tag report")
        if not isinstance(patch["tags"], list):
            raise ValueError("Invalid tag suggestions in report")
        for tag in patch["tags"]:
            if not isinstance(tag, dict) or not isinstance(tag.get("tag"), str) or not isinstance(tag.get("reasons"), list):
                raise ValueError("Invalid tag suggestion in report")
            for reason in tag["reasons"]:
                if not isinstance(reason, dict) or any(not isinstance(reason.get(key), str) for key in ("rule", "detail")):
                    raise ValueError("Invalid tag reason in report")
        labels = [tag["tag"] for tag in patch["tags"]]
        if len(labels) != len(set(labels)):
            raise ValueError("Tag report contains duplicate suggestions")
        indexed[key] = patch
    return indexed


def scan(source, probe):
    patches = []
    with subprocess.Popen([str(probe.resolve()), str(source.resolve())],
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE) as process:
        try:
            header_line = process.stdout.readline()
            if not header_line:
                raise ValueError(process.stderr.read().decode("utf-8", errors="replace").strip()
                                 or "Tag probe produced no report")
            header = json.loads(header_line)
            for line in process.stdout:
                patch = json.loads(line)
                patch["source_sha256"] = hashlib.sha256(bytes.fromhex(patch.pop("source_hex"))).hexdigest()
                patches.append(patch)
            error = process.stderr.read().decode("utf-8", errors="replace").strip()
            status = process.wait()
            if status not in (0, 2):
                raise ValueError(error or f"Tag probe failed with exit code {status}")
        except BaseException:
            process.kill()
            process.wait()
            raise
    invalid = sum(bool(patch["error"]) for patch in patches)
    if (status == 2) != bool(invalid):
        raise ValueError("Tag probe status does not match its source errors")
    counts = Counter(tag["tag"] for patch in patches for tag in patch["tags"])
    report = {"schema_version": SCHEMA_VERSION, "inference_revision": header["inference_revision"],
              "summary": {"patches": len(patches), "valid": len(patches) - invalid,
                          "invalid": invalid, "tag_counts": dict(sorted(counts.items()))},
              "patches": patches}
    indexed_patches(report)
    return report


def compare(before, after):
    old, new = indexed_patches(before), indexed_patches(after)
    changes = []
    added_tags, removed_tags = Counter(), Counter()
    changed_sources = changed_tags = changed_reasons = changed_errors = 0
    for key in sorted(old.keys() & new.keys()):
        previous, current = old[key], new[key]
        previous_tags = {tag["tag"]: tag for tag in previous["tags"]}
        current_tags = {tag["tag"]: tag for tag in current["tags"]}
        added = sorted(current_tags.keys() - previous_tags.keys())
        removed = sorted(previous_tags.keys() - current_tags.keys())
        reason_key = lambda tag: {(reason["rule"], reason["detail"]) for reason in tag["reasons"]}
        reasons = sorted(tag for tag in previous_tags.keys() & current_tags.keys()
                         if reason_key(previous_tags[tag]) != reason_key(current_tags[tag]))
        source_changed = previous["source_sha256"] != current["source_sha256"]
        error_changed = previous["error"] != current["error"]
        changed_sources += source_changed
        changed_errors += error_changed
        # Inference/loading-change totals exclude edited files. Source edits remain visible below.
        if not source_changed:
            added_tags.update(added)
            removed_tags.update(removed)
            changed_tags += bool(added or removed)
            changed_reasons += bool(reasons)
        if added or removed or reasons or source_changed or error_changed or previous["name"] != current["name"]:
            changes.append({"id": key, "name": current["name"], "source_changed": source_changed,
                            "added": [current_tags[tag] for tag in added],
                            "removed": [previous_tags[tag] for tag in removed],
                            "reasons_changed": [{"tag": tag, "before": previous_tags[tag]["reasons"],
                                                 "after": current_tags[tag]["reasons"]} for tag in reasons],
                            "error_before": previous["error"], "error_after": current["error"]})
    return {"schema_version": SCHEMA_VERSION,
            "before_revision": before["inference_revision"], "after_revision": after["inference_revision"],
            "summary": {"common_sources": len(old.keys() & new.keys()),
                        "changed_sources": changed_sources, "changed_errors": changed_errors,
                        "same_source_tag_changes": changed_tags, "same_source_reason_changes": changed_reasons,
                        "added_tags": dict(sorted(added_tags.items())),
                        "removed_tags": dict(sorted(removed_tags.items()))},
            "added_sources": [new[key] for key in sorted(new.keys() - old.keys())],
            "removed_sources": [old[key] for key in sorted(old.keys() - new.keys())],
            "changes": changes}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    inspect = commands.add_parser("scan", help="Inspect a .sy1, ZIP/RAR/7z bank or recursive directory")
    inspect.add_argument("source", type=Path)
    inspect.add_argument("--probe", type=Path, default=ROOT / "build-oos-core" /
                         ("Synth1XTagProbe.exe" if sys.platform == "win32" else "Synth1XTagProbe"))
    difference = commands.add_parser("compare", help="Compare two reports, including reasons and source edits")
    difference.add_argument("before", type=Path)
    difference.add_argument("after", type=Path)
    for command in (inspect, difference):
        command.add_argument("--output", type=Path, help="Write a new JSON file (existing files are preserved)")
    args = parser.parse_args()
    try:
        if args.command == "scan":
            report = scan(args.source, args.probe)
        else:
            report = compare(json.loads(args.before.read_text(encoding="utf-8")),
                             json.loads(args.after.read_text(encoding="utf-8")))
        text = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
        if args.output:
            with args.output.open("x", encoding="utf-8") as output:
                output.write(text)
        else:
            sys.stdout.buffer.write(text.encode("utf-8"))
        return 2 if args.command == "scan" and report["summary"]["invalid"] else 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"Tag report error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
