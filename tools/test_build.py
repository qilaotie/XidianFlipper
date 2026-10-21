"""Small regression checks for editable-workspace integrity and isolation."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import build
import verify


class WorkspaceChecks(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve() / "workspace"
        self.root.mkdir()
        self.patcher = patch.object(build, "ROOT", self.root)
        self.patcher.start()
        self.addCleanup(self.patcher.stop)
        for name in build.SOURCE_ROOTS:
            path = self.root / name
            if "." in name:
                path.write_text("original", encoding="utf-8")
            else:
                path.mkdir()
                (path / "local.c").write_text("local", encoding="utf-8")
        self.release = self.root / "build/release"
        (self.release / "validation").mkdir(parents=True)
        (self.release / "firmware").mkdir()
        records = build.snapshot()
        images = []
        for offset, name in build.OFFSETS.items():
            data = name.encode()
            (self.release / "firmware" / name).write_bytes(data)
            images.append({"offset": hex(offset), "path": "firmware/" + name,
                           "size": len(data), "sha256": build.digest(data)})
        manifest = {"build": {"status": "passed"}, "version": "test", "images": images,
                    "application": images[-1], "source_sha256": build.digest(
                        json.dumps(records, ensure_ascii=True, separators=(",", ":")).encode())}
        build.write_json(self.release / "manifest.json", manifest)
        build.write_json(self.release / "validation/source-files.json", records)

    def test_changed_source_requires_rebuild(self):
        verify.verify(self.release)
        (self.root / "CMakeLists.txt").write_text("edited version", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "compile again"):
            verify.verify(self.release)

    def test_missing_or_extra_source_is_rejected(self):
        path = self.root / "firmware/local.c"
        path.unlink()
        with self.assertRaisesRegex(ValueError, "compile again"):
            verify.verify(self.release)
        path.write_text("local", encoding="utf-8")
        (self.root / "firmware/added.c").write_text("new source", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "compile again"):
            verify.verify(self.release)
        (self.root / "CMakeLists.txt").unlink()
        with self.assertRaisesRegex(ValueError, "Missing local source"):
            build.snapshot()

    def test_other_workspace_component_is_rejected(self):
        idf = self.root.parent / "idf"
        project = {"project_path": str(self.root), "build_dir": str(self.root / "build"),
                   "build_component_paths": [str(self.root / "components"), str(idf / "components") ]}
        build.check_project_paths(project, idf)
        project["build_component_paths"].append(str(self.root.parent / "other-workspace/components"))
        with self.assertRaisesRegex(ValueError, "outside this workspace"):
            build.check_project_paths(project, idf)


if __name__ == "__main__":
    unittest.main()
