"""Build this independent ESP-IDF workspace and record its exact local inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOTS = ["CMakeLists.txt", "sdkconfig.defaults", "partitions_ota.csv", "firmware", "components", "main"]
OFFSETS = {0: "bootloader.bin", 0x8000: "partition-table.bin", 0xf000: "ota_data_initial.bin", 0x20000: "app.bin"}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def snapshot():
    records = []
    for name in SOURCE_ROOTS:
        source = ROOT / name
        if not source.exists():
            raise ValueError("Missing local source: " + name)
        for path in sorted(source.rglob("*") if source.is_dir() else [source]):
            if not path.is_file() or any(part in {".git", "__pycache__"} for part in path.relative_to(ROOT).parts):
                continue
            if not path.resolve().is_relative_to(ROOT):
                raise ValueError("Source is outside this workspace: " + str(path))
            data = path.read_bytes()
            records.append({"path": path.relative_to(ROOT).as_posix(), "size": len(data), "sha256": digest(data)})
    return sorted(records, key=lambda item: item["path"])


def check_project_paths(project, idf):
    if Path(project["project_path"]).resolve() != ROOT or Path(project["build_dir"]).resolve() != ROOT / "build":
        raise ValueError("Build uses another workspace or cache")
    for value in project["build_component_paths"]:
        path = Path(value).resolve()
        if not path.is_relative_to(ROOT) and not path.is_relative_to(idf.resolve()):
            raise ValueError("Component source is outside this workspace and ESP-IDF: " + value)


def build(idf, dev_mode=False, secure_chat=False, oled_force_off=False):
    baseline = json.loads((ROOT / "releases/original/manifest.json").read_text(encoding="utf-8"))
    protocol = baseline["ota_protocol"]
    if not protocol and any((dev_mode, secure_chat, oled_force_off)):
        raise ValueError("These engineering switches are only supported by V1.5")
    records = snapshot()
    source_hash = digest(json.dumps(records, ensure_ascii=True, separators=(",", ":")).encode())
    output = ROOT / "build/release"
    firmware, validation = output / "firmware", output / "validation"
    firmware.mkdir(parents=True, exist_ok=True)
    validation.mkdir(exist_ok=True)
    manifest = {"schema": 1, "id": baseline["id"], "source": "../..", "source_roots": SOURCE_ROOTS,
                "source_kind": "independent-workspace", "source_sha256": source_hash,
                "source_files": len(records), "ota_protocol": protocol, "hardware_tested": False,
                "original_source_sha256": baseline["source_sha256"], "build": {"status": "building"}}
    write_json(output / "manifest.json", manifest)
    write_json(validation / "source-files.json", records)
    command = [sys.executable, str(idf / "tools/idf.py"), "-B", str(ROOT / "build"),
               "-D", "SDKCONFIG=" + str(ROOT / "build/sdkconfig"), "-D", "IDF_TARGET=esp32c6",
               "-D", "BLUE_LONG_ENABLE_MIRROR=ON"]
    if protocol:
        for option, enabled in (("BLM_ENABLE_DEV_MODE", dev_mode), ("BLM_ENABLE_UNREVIEWED_SECURE_CHAT", secure_chat),
                                ("BLM_OLED_FORCE_OFF", oled_force_off)):
            command += ["-D", option + ("=ON" if enabled else "=OFF")]
    command += ["build"]
    started = time.monotonic()
    report = {"passed": False, "command": command, "cwd": str(ROOT), "source_sha256": source_hash,
              "hardware_tested": False}
    try:
        with (validation / "build.log").open("w", encoding="utf-8") as log:
            process = subprocess.Popen(command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       text=True, encoding="utf-8", errors="replace")
            for line in process.stdout:
                log.write(line)
                print(line, end="", flush=True)
            result = process.wait()
        report["exit_code"] = result
        if result:
            raise RuntimeError("ESP-IDF build failed; see build/release/validation/build.log")
        if snapshot() != records:
            raise RuntimeError("Source changed during compilation; build again from a stable workspace")
        settings = json.loads((ROOT / "build/flasher_args.json").read_text(encoding="utf-8"))
        if {int(offset, 0) for offset in settings["flash_files"]} != set(OFFSETS):
            raise ValueError("Unexpected flash layout")
        images = []
        for offset, relative in sorted(settings["flash_files"].items(), key=lambda pair: int(pair[0], 0)):
            source = (ROOT / "build" / relative).resolve()
            if not source.is_relative_to(ROOT / "build"):
                raise ValueError("Build image escaped this workspace")
            data = source.read_bytes()
            target = firmware / OFFSETS[int(offset, 0)]
            target.write_bytes(data)
            images.append({"offset": hex(int(offset, 0)), "path": "firmware/" + target.name,
                           "size": len(data), "sha256": digest(data)})
        app = next(image for image in images if image["offset"] == "0x20000")
        if app["size"] > 0x130000:
            raise ValueError("Application exceeds the existing OTA slot")
        settings["flash_files"] = {image["offset"]: Path(image["path"]).name for image in images}
        for item in settings.values():
            if isinstance(item, dict) and "offset" in item and "file" in item:
                item["file"] = OFFSETS[int(item["offset"], 0)]
        write_json(firmware / "flasher_args.json", settings)
        (firmware / "flash_args").write_text(" ".join(settings["write_flash_args"]) + "\n" +
            "\n".join(image["offset"] + " " + Path(image["path"]).name for image in images) + "\n", encoding="utf-8")
        project = json.loads((ROOT / "build/project_description.json").read_text(encoding="utf-8"))
        check_project_paths(project, idf)
        if project["target"] != "esp32c6" or project["git_revision"] != "v5.5.4":
            raise ValueError("Expected ESP-IDF 5.5.4 / ESP32-C6")
        for source, name in (("sdkconfig", "sdkconfig.txt"), ("project_description.json", "project_description.json")):
            shutil.copy2(ROOT / "build" / source, validation / name)
        manifest.update(version=project["project_version"], application=app, images=images,
                        security_epoch=baseline["security_epoch"], bootstrap=None,
                        build={"status": "passed", "sdk": "ESP-IDF 5.5.4", "sdk_revision": project["git_revision"],
                               "target": project["target"], "mirror": True, "dev_mode": dev_mode,
                               "secure_chat": secure_chat, "oled_force_off": oled_force_off,
                               "sdkconfig_sha256": digest((ROOT / "build/sdkconfig").read_bytes()),
                               "report": "validation/build-report.json", "slot_bytes": 0x130000,
                               "slot_free_bytes": 0x130000 - app["size"]})
        if protocol:
            bootstrap = json.loads((ROOT / "releases/original/firmware/bootstrap.json").read_text(encoding="utf-8"))
            bootstrap.update(version=manifest["version"], images=[{"offset": image["offset"],
                             "file": Path(image["path"]).name, "sha256": image["sha256"]} for image in images])
            write_json(firmware / "bootstrap.json", bootstrap)
            manifest.update(board=bootstrap["board"], layout=bootstrap["layout"], storage=bootstrap["storage"],
                            bootstrap={"path": "firmware/bootstrap.json", "sha256": digest((firmware / "bootstrap.json").read_bytes()),
                                       "validated": False})
        report.update(passed=True, application_bytes=app["size"], application_sha256=app["sha256"],
                      slot_free_bytes=0x130000 - app["size"])
    except Exception as error:
        manifest["build"]["status"] = "failed"
        report["error"] = str(error)
        raise
    finally:
        report.update(seconds=round(time.monotonic() - started, 2), completed_utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()))
        write_json(validation / "build-report.json", report)
        write_json(output / "manifest.json", manifest)
    print("PASS " + manifest["id"] + " / " + manifest["version"] + ": " + str(app["size"]) + " bytes")
    print("Application SHA-256: " + app["sha256"])
    print("Release: " + str(output))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--idf", type=Path, required=True)
    parser.add_argument("--dev-mode", action="store_true")
    parser.add_argument("--secure-chat", action="store_true")
    parser.add_argument("--oled-force-off", action="store_true")
    arguments = parser.parse_args()
    build(arguments.idf.resolve(), arguments.dev_mode, arguments.secure_chat, arguments.oled_force_off)
