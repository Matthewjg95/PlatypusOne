"""Regression tests for camera characterization (software behaviour only).

Synthetic fixtures prove the software behaves as specified. They are never
camera-performance evidence.

    python3 -m unittest discover -s tools/camera_characterize/tests -v

Set CAMCHAR_REQUIRE_SCOUT=1 (CI) to fail instead of skip when the scout_measure
binary is missing.
"""

from __future__ import annotations

import copy
import json
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

import cv2
import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

import synth  # noqa: E402
from camchar import (  # noqa: E402
    ANALYSIS_VERSION,
    UNKNOWN,
    analyze,
    contract,
    decision,
    images,
    metrics,
    plan,
    report,
)  # noqa: E402
from camchar import calibration as cal  # noqa: E402
from camchar import capture as capmod  # noqa: E402
from camchar import repeatability as rep  # noqa: E402
from camchar.cli import DEFAULT_CRITERIA, DEFAULT_SPEC, find_scout_measure  # noqa: E402

SCOUT = find_scout_measure(None)
if SCOUT is None and os.environ.get("CAMCHAR_REQUIRE_SCOUT") == "1":
    raise RuntimeError("scout_measure required (build: cmake --build build --target scout_measure)")

GOOD = dict(fx=650, k1=-0.12, k2=0.04, blur_per_m=3.0)
BLURRY = dict(fx=600, k1=-0.30, k2=0.10, blur_per_m=20.0, focus_m=0.6)


def _cam(p):
    return synth.Camera(
        p["fx"], p["k1"], p["k2"], blur_per_m=p["blur_per_m"], focus_m=p.get("focus_m", 0.3)
    )


class Tmp(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="camchar-"))

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)


# --- contract -------------------------------------------------------------------


class ContractTests(Tmp):
    def _dataset(self):
        return synth.make_dataset(
            self.tmp,
            "d1",
            "SYN-A",
            _cam(GOOD),
            calib_sets=("A",),
            n_calib=2,
            distances=(250,),
            repeats=3,
        )

    def _edit(self, d: Path, fn):
        p = d / "dataset.json"
        m = json.loads(p.read_text())
        fn(m)
        p.write_text(json.dumps(m))

    def test_valid_synthetic_dataset(self):
        ds = contract.load_dataset(self._dataset())
        self.assertTrue(ds.ok, [i.as_dict() for i in ds.issues])
        self.assertEqual(ds.evidence_type, "synthetic_fixture")

    def test_unknown_is_warning_not_guess(self):
        ds = contract.load_dataset(self._dataset())
        warns = {i.where for i in ds.issues if i.level == "WARN" and i.message == "UNKNOWN"}
        self.assertIn("platform.kernel", warns)
        self.assertEqual(ds.manifest["platform"]["kernel"], UNKNOWN)

    def test_missing_required_field_is_error(self):
        d = self._dataset()
        self._edit(d, lambda m: m["platform"].pop("kernel"))
        ds = contract.load_dataset(d)
        self.assertFalse(ds.ok)
        self.assertTrue(any(i.where == "platform.kernel" and i.level == "ERROR" for i in ds.issues))

    def test_missing_capture_field_excludes_frame(self):
        d = self._dataset()

        def drop(m):
            m["frames"][0].pop("lighting")

        self._edit(d, drop)
        ds = contract.load_dataset(d)
        self.assertFalse(ds.frames[0].usable)
        self.assertTrue(all(f.usable for f in ds.frames[1:] if f.get("outcome", "ok") == "ok"))

    def test_sha_mismatch_and_missing_file(self):
        d = self._dataset()
        self._edit(d, lambda m: m["frames"][0].update(sha256="0" * 64))
        (d / json.loads((d / "dataset.json").read_text())["frames"][1]["path"]).unlink()
        ds = contract.load_dataset(d)
        msgs = [i.message for f in ds.frames[:2] for i in f.issues]
        self.assertTrue(any("sha256 mismatch" in m for m in msgs))
        self.assertTrue(any("source file missing" in m for m in msgs))

    def test_path_escape_rejected(self):
        d = self._dataset()
        self._edit(d, lambda m: m["frames"][0].update(path="../../etc/passwd"))
        ds = contract.load_dataset(d)
        self.assertTrue(any("escapes" in i.message for i in ds.frames[0].issues))

    def test_failed_frame_preserved_not_analysed(self):
        ds = contract.load_dataset(self._dataset())
        failed = [f for f in ds.frames if f.get("outcome") == "capture_failed"]
        self.assertEqual(len(failed), 1)
        self.assertFalse(failed[0].usable)

    def test_raw_size_checked(self):
        d = self._dataset()
        raw = d / "frames" / "x.raw"
        raw.write_bytes(b"\0" * 100)

        def add(m):
            f = copy.deepcopy(m["frames"][0])
            f.update(
                frame_id="raw",
                path="frames/x.raw",
                format="raw10p",
                width=16,
                height=4,
                bayer_pattern="RGGB",
                black_level=64,
                white_level=1023,
                sha256=UNKNOWN,
            )
            m["frames"].append(f)

        self._edit(d, add)
        ds = contract.load_dataset(d)
        fr = next(f for f in ds.frames if f.frame_id == "raw")
        self.assertTrue(any("expected 80" in i.message for i in fr.issues))

    def test_bool_is_not_a_number(self):
        d = self._dataset()
        self._edit(d, lambda m: m["targets"]["ref20"].update(reference_mm=True))
        ds = contract.load_dataset(d)
        self.assertTrue(any("positive number" in i.message for i in ds.issues))
        self.assertFalse(contract.is_number(True))
        self.assertTrue(contract.is_number(20.0))
        phys = {"board_w_mm": {"value": True, "status": "measured"}}
        self.assertIsNone(decision._measured(phys, "board_w_mm"))

    def test_bool_dimensions_rejected_for_frames(self):
        d = self._dataset()
        self._edit(d, lambda m: m["frames"][0].update(width=True))
        ds = contract.load_dataset(d)
        self.assertTrue(any("positive integers" in i.message for i in ds.frames[0].issues))

    def test_unverified_dimension_flagged(self):
        d = self._dataset()
        self._edit(
            d, lambda m: m["targets"]["ref20"].update(reference_mm_status="nominal_unverified")
        )
        ds = contract.load_dataset(d)
        self.assertTrue(any("not for absolute accuracy" in i.message for i in ds.issues))
        self.assertFalse(contract.dimension_verified(ds.targets["ref20"], "reference_mm"))


# --- images -----------------------------------------------------------------------


class ImageTests(unittest.TestCase):
    def test_raw10p_roundtrip(self):
        rng = np.random.default_rng(3)
        w, h = 16, 4
        px = rng.integers(0, 1024, (h, w), dtype=np.uint16)
        line = w * 5 // 4
        buf = bytearray(line * h)
        for y in range(h):
            for g in range(w // 4):
                vals = px[y, g * 4 : g * 4 + 4]
                base = y * line + g * 5
                for i in range(4):
                    buf[base + i] = int(vals[i]) >> 2
                buf[base + 4] = sum((int(vals[i]) & 3) << (2 * i) for i in range(4))
        np.testing.assert_array_equal(images.unpack_raw10p(bytes(buf), w, h, None), px)

    def test_exposure_clip_and_crush(self):
        lum = np.zeros((10, 10))
        lum[:5] = 1.0
        img = images.Loaded(lum, images._to8(lum), lum >= 0.995, False, 255.0)
        e = metrics.exposure(img)
        self.assertAlmostEqual(e["clipped_pct"], 50.0)
        self.assertAlmostEqual(e["crushed_pct"], 50.0)


# --- calibration --------------------------------------------------------------------


class CalibrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cam = _cam(GOOD)
        tex = synth.raster(synth.BOARD, 6.0, 15.0)
        cls.views = []
        for i, (r, t) in enumerate(synth._calib_poses(12)):
            img = cls.cam.render(tex, 6.0, 15.0, r, t, seed=i)
            det, why = cal.detect(img, synth.BOARD)
            assert det is not None, why
            cls.views.append((f"v{i}", det))

    def test_recovers_known_intrinsics(self):
        r = cal.calibrate(self.views, synth.SIZE, 10)
        self.assertEqual(r["status"], "ok")
        K = np.array(r["camera_matrix"])
        self.assertLess(abs(K[0, 0] - self.cam.K[0, 0]) / self.cam.K[0, 0], 0.01)
        self.assertLess(abs(r["distortion"]["k1"] - self.cam.dist[0]), 0.02)
        self.assertLess(r["rms_reprojection_px"], 0.5)
        self.assertEqual(len(r["per_view"]), 12)
        self.assertGreater(r["coverage"]["max_corner_radius_norm"], 0.8)

    def test_insufficient_views(self):
        r = cal.calibrate(self.views[:4], synth.SIZE, 10)
        self.assertEqual(r["status"], "insufficient")
        self.assertIn("4 accepted views", r["reason"])

    def test_blank_frame_rejected_with_reason(self):
        det, why = cal.detect(np.full((616, 820), 200, np.uint8), synth.BOARD)
        self.assertIsNone(det)
        self.assertIn("no ChArUco corners", why)

    def test_target_svg_matches_detected_layout(self):
        det, why = cal.detect(synth.raster(synth.BOARD, 6.0, 15.0), synth.BOARD)
        self.assertEqual(len(det.ids), 54, why)
        pred = (det.obj[:, :2] + 15.0) * 6.0 - 0.5
        self.assertLess(np.abs(pred - det.img).max(), 0.6)


# --- sharpness ------------------------------------------------------------------------


class SharpnessTests(unittest.TestCase):
    def _scene(self, sigma: float, gain: float = 1.0) -> images.Loaded:
        tex, ppm, margin = synth.scout_texture()
        cam = synth.Camera(650, 0.0)
        r, t = synth._sheet_pose(250, (0, 0), cam)
        img = cam.render(tex, ppm, margin, r, t, noise=0.0).astype(np.float64)
        if sigma > 0:
            img = cv2.GaussianBlur(img, (0, 0), sigma)
        lum = np.clip(img * gain / 255.0, 0, 1)
        return images.Loaded(lum, images._to8(lum), lum >= 0.995, False, 255.0)

    def test_edge_rise_grows_with_blur(self):
        rises = [
            metrics.edge_rise(self._scene(s))["edge_rise_px_median"] for s in (0, 1.0, 2.0, 3.0)
        ]
        self.assertEqual(rises, sorted(rises))
        self.assertGreater(rises[-1], 2.5 * rises[0])

    def test_tenengrad_falls_with_blur(self):
        t = [
            metrics.region_sharpness(self._scene(s))["center"]["tenengrad_norm"]
            for s in (0, 1.5, 3.0)
        ]
        self.assertEqual(t, sorted(t, reverse=True))

    def test_normalised_sharpness_exposure_invariant(self):
        a = metrics.region_sharpness(self._scene(1.0, 1.0))["center"]["tenengrad_norm"]
        b = metrics.region_sharpness(self._scene(1.0, 0.5))["center"]["tenengrad_norm"]
        self.assertLess(abs(a - b) / a, 0.05)

    def test_no_square_no_edge_rise(self):
        lum = np.full((100, 100), 0.8)
        img = images.Loaded(lum, images._to8(lum), lum > 1, False, 255.0)
        self.assertIsNone(metrics.edge_rise(img))


# --- repeatability ----------------------------------------------------------------------


class RepeatabilityTests(unittest.TestCase):
    def test_series_stats(self):
        s = rep.series_stats([10.0, 10.2, 9.8, 10.0])
        self.assertAlmostEqual(s["mean"], 10.0)
        self.assertAlmostEqual(s["std"], float(np.std([10.0, 10.2, 9.8, 10.0], ddof=1)))
        self.assertAlmostEqual(s["range"], 0.4)
        self.assertAlmostEqual(s["relative_spread"], s["std"] / 10.0)
        self.assertIsNone(rep.series_stats([5.0])["std"])

    def _rows(self, values):
        rows = [
            {
                "frame": f"f{i}",
                "measurement": {
                    "ok": True,
                    "subject_length_mm": v,
                    "subject_width_mm": 8.0,
                    "mm_per_pixel": 0.1,
                },
            }
            for i, v in enumerate(values)
        ]
        rows.append({"frame": "bad", "measurement": {"ok": False, "error": "NoReferenceTarget"}})
        return rows

    def test_accuracy_only_with_truth_and_separate(self):
        no = rep.summarize(self._rows([40.1, 40.2, 40.3]), None)
        self.assertEqual(no["accuracy"]["status"], "not_computed")
        self.assertEqual(no["n_refused"], 1)
        self.assertEqual(no["refusals"], {"NoReferenceTarget": 1})
        yes = rep.summarize(self._rows([40.1, 40.2, 40.3]), {"length_mm": 40.0})
        self.assertAlmostEqual(yes["accuracy"]["subject_length_mm"]["mean_error_mm"], 0.2)
        # repeatability is unchanged by truth: no correction applied
        self.assertEqual(no["repeatability"], yes["repeatability"])

    def test_temporal_noise(self):
        rng = np.random.default_rng(0)
        lums = [0.5 + rng.normal(0, 0.01, (50, 50)) for _ in range(6)]
        tn = rep.temporal_noise(lums, 255.0)
        self.assertAlmostEqual(tn["median_pixel_std_norm"], 0.01, delta=0.002)
        self.assertIsNone(rep.temporal_noise(lums[:2], 255.0))

    @unittest.skipIf(SCOUT is None, "scout_measure not built")
    def test_scout_measure_bridge(self):
        tex, ppm, margin = synth.scout_texture()
        cam = synth.Camera(650, 0.0)
        r, t = synth._sheet_pose(250, (0, 0), cam)
        img = cam.render(tex, ppm, margin, r, t, noise=0.0)
        with tempfile.TemporaryDirectory() as d:
            pgm = Path(d) / "x.pgm"
            images.write_pgm(pgm, img)
            out = rep.run_scout(SCOUT, pgm, 20.0)
        self.assertTrue(out["ok"], out)
        self.assertAlmostEqual(out["subject_length_mm"], 40.0, delta=0.6)
        self.assertAlmostEqual(out["subject_width_mm"], 8.0, delta=0.4)


# --- end to end: immutability, report, decision -----------------------------------------


class PipelineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = Path(tempfile.mkdtemp(prefix="camchar-exp-"))
        cls.exp = synth.make_experiment(
            cls.tmp / "exp", {"SYN-A": _cam(GOOD), "SYN-B": _cam(BLURRY)}
        )
        cls.sources = {p: (p.read_bytes(), p.stat().st_mtime_ns) for p in cls.exp.rglob("frames/*")}
        cls.analysis, cls.out = analyze.run(cls.exp, SCOUT, 10)
        crit, sha = decision.load_criteria(DEFAULT_CRITERIA)
        cls.crit, cls.sha = crit, sha
        cls.decision = decision.decide(cls.analysis, crit, sha)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_sources_untouched_and_derived_separate(self):
        for p, (data, mtime) in self.sources.items():
            self.assertEqual(p.read_bytes(), data)
            self.assertEqual(p.stat().st_mtime_ns, mtime)
        self.assertEqual(self.out, self.exp / "derived" / ANALYSIS_VERSION)
        self.assertEqual(self.analysis["source_integrity"], "unchanged")
        written = [p for p in self.exp.rglob("*") if p.is_file() and self.out not in p.parents]
        self.assertTrue(
            all(
                p.name in ("dataset.json", "experiment.json") or "frames" in p.parts
                for p in written
            )
        )
        self.assertFalse(list(self.out.rglob("work")))

    def test_rerun_from_scratch(self):
        sentinel = self.out / "stale.txt"
        sentinel.write_text("stale")
        analysis2, out2 = analyze.run(self.exp, SCOUT, 10)
        self.assertFalse(sentinel.exists())
        a = json.dumps(self.analysis["datasets"], sort_keys=True, default=str)
        b = json.dumps(analysis2["datasets"], sort_keys=True, default=str)
        self.assertEqual(a, b, "analysis must be deterministic")

    def test_refuses_on_dataset_error(self):
        bad = self.tmp / "bad"
        shutil.copytree(self.exp, bad, ignore=shutil.ignore_patterns("derived"))
        p = bad / "datasets/syn-syn-a/dataset.json"
        m = json.loads(p.read_text())
        m["schema"] = "wrong"
        p.write_text(json.dumps(m))
        with self.assertRaises(analyze.AnalysisRefused):
            analyze.run(bad, SCOUT, 10)
        self.assertTrue((bad / "derived" / ANALYSIS_VERSION / "validation.json").is_file())

    def test_calibration_success_and_failure_handled(self):
        a = next(r for r in self.analysis["datasets"] if r["sku"] == "SYN-A")
        b = next(r for r in self.analysis["datasets"] if r["sku"] == "SYN-B")
        self.assertTrue(all(c["status"] == "ok" for c in a["calibrations"]))
        self.assertTrue(all(c["status"] == "insufficient" for c in b["calibrations"]))
        self.assertTrue(all(c["frames_rejected"] for c in b["calibrations"]))
        self.assertTrue(
            (self.out / "syn-syn-a" / a["calibrations"][0]["undistort_preview"]).is_file()
        )

    def test_synthetic_never_assigns_roles(self):
        for role, d in self.decision["decision"].items():
            self.assertEqual(d["outcome"], "INSUFFICIENT EVIDENCE", role)
            self.assertIn("synthetic", d["reason"])

    def test_gate_logic(self):
        d = decision.decide(self.analysis, self.crit, self.sha, _allow_synthetic_roles=True)
        cams = d["cameras"]
        self.assertEqual(cams["SYN-B"]["roles"]["reference"]["verdict"], "NOT_ELIGIBLE")
        if SCOUT is not None:
            self.assertEqual(cams["SYN-A"]["roles"]["reference"]["verdict"], "ELIGIBLE")
            self.assertEqual(d["decision"]["reference"]["outcome"], "PROPOSED")
            self.assertEqual(d["decision"]["reference"]["camera"], "SYN-A")
        # product compactness thresholds are unset until Fusion: never decidable yet
        self.assertEqual(cams["SYN-A"]["roles"]["product"]["verdict"], "INSUFFICIENT")
        self.assertEqual(d["decision"]["product"]["outcome"], "INSUFFICIENT EVIDENCE")

    def test_insufficient_evidence_when_metric_missing(self):
        a = copy.deepcopy(self.analysis)
        for r in a["datasets"]:
            r["series"] = {}
        d = decision.decide(a, self.crit, self.sha, _allow_synthetic_roles=True)
        rows = {c["metric"]: c for c in d["cameras"]["SYN-A"]["roles"]["fallback"]["criteria"]}
        self.assertEqual(rows["repeatability_rel_spread"]["status"], "INSUFFICIENT")
        self.assertNotEqual(d["decision"]["fallback"]["outcome"], "PROPOSED")

    def test_duplicate_calibration_set_cannot_pass_stability(self):
        a = copy.deepcopy(self.analysis)
        sku_a = next(r for r in a["datasets"] if r["sku"] == "SYN-A")
        for c in sku_a["calibrations"]:  # A + A, same mounting
            c["calibration_set"] = "A"
            c["mount_id"] = "mount-A"
        d = decision.decide(a, self.crit, self.sha, _allow_synthetic_roles=True)
        m = d["cameras"]["SYN-A"]["metrics"]
        self.assertIsNone(m["calibration_independent_sets"]["value"])
        self.assertIn("1 distinct physical mounting", m["calibration_independent_sets"]["note"])
        self.assertIsNone(m["calibration_focal_rel_delta"]["value"])
        rows = {c["metric"]: c for c in d["cameras"]["SYN-A"]["roles"]["reference"]["criteria"]}
        self.assertEqual(rows["calibration_independent_sets"]["status"], "INSUFFICIENT")
        self.assertEqual(d["cameras"]["SYN-A"]["roles"]["reference"]["verdict"], "INSUFFICIENT")
        for c in sku_a["calibrations"]:  # no mounting identity at all
            c["mount_id"] = UNKNOWN
        d = decision.decide(a, self.crit, self.sha, _allow_synthetic_roles=True)
        m = d["cameras"]["SYN-A"]["metrics"]["calibration_independent_sets"]
        self.assertIsNone(m["value"])
        self.assertIn("0 distinct physical mounting", m["note"])

    def test_duplicate_set_across_datasets_counts_once(self):
        a = copy.deepcopy(self.analysis)
        sku_a = next(r for r in a["datasets"] if r["sku"] == "SYN-A")
        twin = copy.deepcopy(sku_a)
        twin["dataset_id"] = "syn-syn-a-copy"
        for r in (sku_a, twin):
            r["calibrations"] = [c for c in r["calibrations"] if c["calibration_set"] == "A"]
        a["datasets"].append(twin)
        d = decision.decide(a, self.crit, self.sha, _allow_synthetic_roles=True)
        m = d["cameras"]["SYN-A"]["metrics"]["calibration_independent_sets"]
        self.assertIsNone(m["value"])
        self.assertIn("1 distinct physical mounting", m["note"])

    def test_independent_mountings_pass(self):
        m = self.decision["cameras"]["SYN-A"]["metrics"]
        self.assertEqual(m["calibration_independent_sets"]["value"], 2)
        self.assertIsNotNone(m["calibration_focal_rel_delta"]["value"])

    def test_not_exposed_is_not_a_focus_state(self):
        a = copy.deepcopy(self.analysis)
        sku_a = next(r for r in a["datasets"] if r["sku"] == "SYN-A")
        for fr in sku_a["frames"].values():
            fr["focus_mode"] = "not_exposed"
        d = decision.decide(a, self.crit, self.sha, _allow_synthetic_roles=True)
        m = d["cameras"]["SYN-A"]["metrics"]
        self.assertIsNone(m["focus_state_recorded"]["value"])
        self.assertIs(m["focus_control_exposed"]["value"], False)
        rows = {c["metric"]: c for c in d["cameras"]["SYN-A"]["roles"]["reference"]["criteria"]}
        self.assertEqual(rows["focus_state_recorded"]["status"], "INSUFFICIENT")

    def test_fixed_passes_but_motorized_claiming_fixed_does_not(self):
        m = self.decision["cameras"]["SYN-A"]["metrics"]
        self.assertIs(m["focus_state_recorded"]["value"], True)  # synthetic SYN-A is fixed
        a = copy.deepcopy(self.analysis)
        sku_a = next(r for r in a["datasets"] if r["sku"] == "SYN-A")
        sku_a["camera"]["focus_type"] = "motorized"
        d = decision.decide(a, self.crit, self.sha, _allow_synthetic_roles=True)
        self.assertIsNone(d["cameras"]["SYN-A"]["metrics"]["focus_state_recorded"]["value"])
        for fr in sku_a["frames"].values():
            fr["focus_mode"], fr["focus_position"] = "motorized_position", 512
        d = decision.decide(a, self.crit, self.sha, _allow_synthetic_roles=True)
        self.assertIs(d["cameras"]["SYN-A"]["metrics"]["focus_state_recorded"]["value"], True)
        self.assertIs(d["cameras"]["SYN-A"]["metrics"]["focus_control_exposed"]["value"], True)

    def test_unknown_focus_is_insufficient_auto_is_fail(self):
        a = copy.deepcopy(self.analysis)
        sku_a = next(r for r in a["datasets"] if r["sku"] == "SYN-A")
        first = next(iter(sku_a["frames"].values()))
        first["focus_mode"] = UNKNOWN
        d = decision.decide(a, self.crit, self.sha, _allow_synthetic_roles=True)
        self.assertIsNone(d["cameras"]["SYN-A"]["metrics"]["focus_state_recorded"]["value"])
        first["focus_mode"] = "auto"
        d = decision.decide(a, self.crit, self.sha, _allow_synthetic_roles=True)
        self.assertIs(d["cameras"]["SYN-A"]["metrics"]["focus_state_recorded"]["value"], False)

    def test_report_traceable_and_banner(self):
        md = report.render(self.analysis, self.decision)
        self.assertIn("SYNTHETIC FIXTURE — NOT CAMERA EVIDENCE", md)
        self.assertIn(ANALYSIS_VERSION, md)
        self.assertIn(self.sha[:16], md)
        self.assertIn("`syn-syn-a/calib-A-00`", md)
        self.assertIn("ACCURACY", md)
        self.assertIn("REPEATABILITY", md)
        self.assertNotIn("score", md.lower().replace("no weighted score", ""))


# --- plan / capture ---------------------------------------------------------------------------


class PlanCaptureTests(Tmp):
    def setUp(self):
        super().setUp()
        self.spec = json.loads(DEFAULT_SPEC.read_text())

    def test_matrix_counts(self):
        self.assertEqual(len(plan.expand(self.spec, "B0394")), 74)
        self.assertEqual(len(plan.expand(self.spec, "B0393")), 79)
        distances = {s["working_distance_mm"] for s in plan.expand(self.spec, "B0390")}
        self.assertEqual(distances, {150, 250, 400, 600})

    def test_init_refuses_overwrite_and_unknowns(self):
        d = self.tmp / "ds"
        plan.init_dataset(self.spec, "B0393", d, "b0393-test")
        m = json.loads((d / "dataset.json").read_text())
        self.assertEqual(m["camera"]["silkscreen"], UNKNOWN)
        self.assertEqual(m["bringup"]["focus_control"], UNKNOWN)
        self.assertEqual(m["targets"]["charuco-7x10-20"]["square_mm_status"], "nominal_unverified")
        shots = json.loads((d / "capture_plan.json").read_text())["shots"]
        mounts = {s["mount_id"] for s in shots if "mount_id" in s}
        self.assertEqual(mounts, {"b0393-test/mount-1", "b0393-test/mount-2"})
        with self.assertRaises(FileExistsError):
            plan.init_dataset(self.spec, "B0393", d, "b0393-test")

    def test_capture_appends_hashes_and_preserves_failures(self):
        d = self.tmp / "ds"
        plan.init_dataset(self.spec, "B0390", d, "b0390-test")
        m = json.loads((d / "dataset.json").read_text())
        m["defaults"]["pixel_format"] = "pgm"
        (d / "dataset.json").write_text(json.dumps(m))
        writer = self.tmp / "w.py"
        writer.write_text(
            "import sys\nopen(sys.argv[1],'wb').write(b'P5\\n4 2\\n255\\n'+bytes(8))\n"
            'print(\'{"width": 4, "height": 2, "format": "pgm"}\')\n'
        )
        answers = iter(["", "a note", "dbad focus", "q"])
        capmod.run(d, f"{sys.executable} {writer} {{out}}", None, ask=lambda _: next(answers))
        failing = iter(["", "q"])
        capmod.run(d, "false {out}", None, ask=lambda _: next(failing))
        frames = json.loads((d / "dataset.json").read_text())["frames"]
        self.assertEqual([f["outcome"] for f in frames], ["ok", "discarded", "capture_failed"])
        self.assertEqual(frames[0]["sha256"], contract.sha256_file(d / frames[0]["path"]))
        self.assertEqual(frames[0]["notes"], "a note")
        self.assertEqual(frames[1]["failure_reason"], "bad focus")
        cov = plan.coverage(d, json.loads((d / "dataset.json").read_text()))
        self.assertEqual(cov["captured_ok_frames"], 1)
        self.assertTrue(cov["missing"])

    def test_capture_never_overwrites(self):
        d = self.tmp / "ds"
        plan.init_dataset(self.spec, "B0390", d, "b0390-test")
        first = plan.expand(self.spec, "B0390")[0]
        (d / "frames" / f"{first['plan_cell']}-01.bin").write_bytes(b"existing")
        rc = capmod.run(d, "true {out}", None, ask=lambda _: "")
        self.assertEqual(rc, 1)
        self.assertEqual((d / "frames" / f"{first['plan_cell']}-01.bin").read_bytes(), b"existing")


if __name__ == "__main__":
    unittest.main()
