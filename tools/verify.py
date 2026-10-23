"""Check frozen originals and the latest build against this workspace's files."""
import json
from pathlib import Path
from build import ROOT, digest, snapshot
from flash_layout import capacity, profile


def check_files(folder, records):
    for record in records:
        path = (folder / record["path"]).resolve()
        if not path.is_relative_to(folder.resolve()):
            raise ValueError("Manifest path escaped: " + record["path"])
        data = path.read_bytes()
        if len(data) != record["size"] or digest(data) != record["sha256"]:
            raise ValueError("File changed: " + str(path))


def verify(folder, original=False):
    manifest = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
    records = json.loads((folder / "validation/source-files.json").read_text(encoding="utf-8"))
    if digest(json.dumps(records, ensure_ascii=True, separators=(",", ":")).encode()) != manifest["source_sha256"]:
        raise ValueError("Source record changed")
    if original:
        check_files(folder / "source", records)
    elif records != snapshot():
        raise ValueError("Current source differs from the last successful build; compile again")
    if manifest["build"]["status"] != "passed":
        raise ValueError("No successful build")
    images = manifest["images"]
    if len(images) != 4 or {int(image["offset"], 0) for image in images} != {0, 0x8000, 0xf000, 0x20000}:
        raise ValueError("Unexpected four-segment flash layout")
    if manifest["application"] != next(image for image in images if image["offset"] == "0x20000"):
        raise ValueError("Application record differs")
    check_files(folder, images)
    table = (folder / 'firmware/partition-table.bin').read_bytes()
    layout_name, _ = profile(table)
    if manifest['application']['size'] > capacity(table):
        raise ValueError('Application exceeds the compiled partition table')
    if not original and (manifest['build'].get('layout') != layout_name or
                         manifest['build']['slot_bytes'] != capacity(table) or
                         manifest['build']['slot_free_bytes'] != capacity(table)-manifest['application']['size']):
        raise ValueError('Manifest capacity differs from compiled partition table')
    bootstrap = manifest.get("bootstrap")
    if bootstrap and digest((folder / bootstrap["path"]).read_bytes()) != bootstrap["sha256"]:
        raise ValueError("Bootstrap manifest changed")
    print("PASS", folder, manifest["version"], manifest["application"]["sha256"])


if __name__ == "__main__":
    import argparse
    parser=argparse.ArgumentParser(description='Verify frozen original and a release matching current source.')
    parser.add_argument('--release',type=Path,default=ROOT/'build/release')
    args=parser.parse_args()
    verify(ROOT / "releases/original", original=True)
    verify(args.release.resolve())
