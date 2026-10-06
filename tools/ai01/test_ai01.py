"""Regression and failure gates; no hardware or model provider required."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
from typing import Any

import cv2
import numpy as np

from services.vision.python.ai01 import SceneError, analyze
from tools.ai01.capture import capture
from tools.ai01.fixture import fixture


class PlanarTests(unittest.TestCase):
    def test_fixture_and_contract(self) -> None:
        source = Path(__file__).parent / "fixtures" / "plate.png"
        np.testing.assert_array_equal(cv2.imread(str(source)), fixture())
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            records = []
            for name in ("first", "second"):
                records.append(
                    capture(
                        root / name,
                        20,
                        source,
                        timestamp="2026-09-26T00:00:00Z",
                        observation_id="fixture-plate",
                    )
                )
            self.assertEqual(records[0], records[1])
            self.assertEqual((root / "first/source.png").read_bytes(), source.read_bytes())
            for path in (root / "first").iterdir():
                self.assertEqual(path.read_bytes(), (root / "second" / path.name).read_bytes())
            record = records[0]
            derived = {c["id"]: c for c in record["derived"]}
            self.assertAlmostEqual(derived["p-scale"]["value"], 0.25)
            self.assertAlmostEqual(derived["p-length-mm"]["value"], 40)
            self.assertAlmostEqual(derived["p-width-mm"]["value"], 20)
            self.assertAlmostEqual(derived["p-void-1-mm"]["value"], 6, delta=0.15)
            self.assertGreater(derived["p-length-sensitivity"]["value"], 0)
            geometry = json.loads((root / "first/geometry.json").read_text())
            self.assertEqual(len(geometry["line_candidates"]), 4)
            self.assertEqual(len(geometry["circular_void_candidates"]), 1)
            self.assertEqual(record["inferred"], [])
            self.assertIn("thickness", {u["name"] for u in record["unresolved"]})
            self.assertEqual(record["human_review"]["state"], "pending")
            self.assert_contract(record, root / "first")
            with self.assertRaises(FileExistsError):
                capture(root / "first", 20, source)

    def assert_contract(self, record: dict[str, Any], folder: Path) -> None:
        ids = {a["id"] for a in record["artifacts"]}
        claims = record["observed"] + record["derived"] + record["inferred"]
        self.assertEqual(len({c["id"] for c in claims}), len(claims))
        ids.update(c["id"] for c in claims)
        for artifact in record["artifacts"]:
            self.assertTrue((folder / artifact["path"]).is_file())
        for claim in claims:
            self.assertIsInstance(claim["value"], (float, int, bool, str))
            self.assertTrue(claim["method"])
            self.assertTrue(claim["provenance"])
            self.assertTrue(set(claim["provenance"]) <= ids)
        json.dumps(record, allow_nan=False)

    def test_reject_ambiguous_missing_clipped_and_clutter(self) -> None:
        for case in ("ambiguous", "missing", "clipped", "clutter", "no-subject"):
            with self.subTest(case=case):
                image = fixture()
                if case == "ambiguous":
                    image[20:60, 300:340] = 0
                elif case == "missing":
                    image[40:120, 30:110] = 255
                elif case == "clipped":
                    image[100:180, 340:400] = 0
                elif case == "clutter":
                    image[200:210, 20:60] = 0
                else:
                    image[100:180, 180:340] = 255
                with self.assertRaises(SceneError):
                    analyze(image, 20)
        for value in (0, -1, float("nan"), float("inf")):
            with self.assertRaises(ValueError):
                analyze(fixture(), value)

    def test_failure_preserves_source(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source = root / "blank.png"
            cv2.imwrite(str(source), np.full((80, 80, 3), 255, dtype=np.uint8))
            with self.assertRaises(SceneError):
                capture(root / "failed", 20, source)
            self.assertEqual(source.read_bytes(), (root / "failed/source.png").read_bytes())
            self.assertTrue((root / "failed/failure.json").exists())
            self.assertFalse((root / "failed/EngineeringObservation.json").exists())


if __name__ == "__main__":
    unittest.main()
