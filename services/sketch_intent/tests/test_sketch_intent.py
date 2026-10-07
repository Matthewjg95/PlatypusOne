"""Tests for the deterministic sketch intent resolver (synthetic data only).

Run: PYTHONPATH=services/sketch_intent python3 -m unittest discover -s services/sketch_intent/tests
Set MESH2CAD_ROOT to a Mesh2CAD checkout carrying ``mra.sketch_assets``
(PR #16) to also validate exports with Mesh2CAD's own model.
"""

from __future__ import annotations

import json
import math
import os
import subprocess
import sys
import tempfile
import unittest
from collections import Counter
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

from fixtures import dev_cases, realize  # noqa: E402
from sketch_intent import PROPOSAL_SCHEMA, RESOLVER_VERSION  # noqa: E402
from sketch_intent.contract import (  # noqa: E402
    InputError,
    Params,
    load_observation,
    observation_from_dict,
)
from sketch_intent.export import (  # noqa: E402
    ExportError,
    accepted_geometry,
    mesh2cad_export,
    to_sketch_asset,
)
from sketch_intent.overlay import render  # noqa: E402
from sketch_intent.resolve import resolve  # noqa: E402
from sketch_intent.review import ReviewError, apply_decisions  # noqa: E402

CASES = {c.name: c for c in dev_cases()}
_CACHE: dict[str, tuple[dict, object, dict]] = {}


def run(name: str):
    """(input dict, observation, proposal) for a dev fixture, cached."""
    if name not in _CACHE:
        data, _ = realize(CASES[name])
        obs = observation_from_dict(data, f"sha-{name}")
        _CACHE[name] = (data, obs, resolve(obs, Params()))
    return _CACHE[name]


def decisions(p, ctype):
    return Counter(c["decision"] for c in p["constraints"] if c["type"] == ctype)


def kinds_by_loop(p):
    return [
        [x["kind"] for x in p["primitives"] if x["loop_id"] == lp["loop_id"]]
        for lp in p["evidence"]["loops"]
    ]


def cyclic_equal(a, b):
    return len(a) == len(b) and any(a[i:] + a[:i] == b for i in range(len(a)))


class ContractTests(unittest.TestCase):
    def test_shadowscan_outline_preserves_points_and_unknown_fields(self):
        data = {
            "format": "shadowscan-outline",
            "units": "px",
            "scale_mm_per_unit": 0.11,
            "outlines": [[[0, 0], [10, 0], [10, 10], [0, 10]]],
            "session": "abc",
        }
        obs = observation_from_dict(data, "s")
        self.assertEqual(obs.loops[0].points.tolist(), [[0, 0], [10, 0], [10, 10], [0, 10]])
        self.assertEqual(obs.scale_mm_per_px, 0.11)
        self.assertEqual(obs.carried["session"], "abc")
        self.assertIn("UNKNOWN", obs.calibration["provenance"])

    def test_shadowscan_scan_scale_only_when_mm(self):
        raw = {"outer": [[0, 0], [10, 0], [10, 10]], "inner": []}
        mm = {"units": "MM", "imageTransform": {"unitsPerPixel": 0.2}}
        self.assertEqual(
            observation_from_dict(
                {"format": "shadowscan-scan", "raw": raw, "mmSketch": mm}
            ).scale_mm_per_px,
            0.2,
        )
        px = {"units": "PX", "imageTransform": {"unitsPerPixel": 0.2}}
        obs = observation_from_dict({"format": "shadowscan-scan", "raw": raw, "mmSketch": px})
        self.assertIsNone(obs.scale_mm_per_px)
        self.assertEqual(obs.calibration["status"], "uncalibrated")

    def test_contour_observation_roles_ids_unresolved(self):
        data = {
            "schema": "platypus.contour_observation/1",
            "units": "px",
            "observation_id": "obs-1",
            "loops": [
                {
                    "loop_id": "a",
                    "role": "outer",
                    "closed": False,
                    "points": [[0, 0], [5, 0], [5, 5]],
                }
            ],
            "calibration": {"mm_per_px": None, "provenance": ["none"]},
            "unresolved": [{"name": "scale"}],
        }
        obs = observation_from_dict(data)
        self.assertFalse(obs.loops[0].closed)
        self.assertEqual(obs.source_ids, {"observation_id": "obs-1"})
        self.assertEqual(obs.unresolved, [{"name": "scale"}])
        self.assertFalse(obs.calibrated)

    def test_rejects_bad_input(self):
        bad = [
            {"format": "nope"},
            {"format": "shadowscan-outline", "units": "mm", "outlines": [[[0, 0], [1, 0], [1, 1]]]},
            {"format": "shadowscan-outline", "outlines": [[[0, 0], [1, 0]]]},
            {"format": "shadowscan-outline", "outlines": [[[0, 0], [1, True], [1, 1]]]},
            {"format": "shadowscan-outline", "outlines": [[[0, 0], [1, float("nan")], [1, 1]]]},
            {
                "format": "shadowscan-outline",
                "scale_mm_per_unit": -1,
                "outlines": [[[0, 0], [1, 0], [1, 1]]],
            },
            {
                "format": "shadowscan-outline",
                "scale_mm_per_unit": True,
                "outlines": [[[0, 0], [1, 0], [1, 1]]],
            },
        ]
        for d in bad:
            with self.subTest(d=d), self.assertRaises(InputError):
                observation_from_dict(d)

    def test_params_validation(self):
        with self.assertRaises(InputError):
            Params(tolerance_px=0).validate()
        with self.assertRaises(InputError):
            Params(question_factor=0.5).validate()

    def test_roles_assigned_by_containment(self):
        _, obs, _ = run("plate_two_holes_and_slot")
        self.assertEqual([lp.role for lp in obs.loops], ["outer", "hole", "hole", "hole"])


class ProposalTests(unittest.TestCase):
    def test_schema_and_provenance(self):
        data, obs, p = run("plate_two_holes_and_slot")
        self.assertEqual(p["schema"], PROPOSAL_SCHEMA)
        self.assertEqual(p["resolver_version"], RESOLVER_VERSION)
        self.assertEqual(p["input"]["sha256"], "sha-plate_two_holes_and_slot")
        self.assertEqual(p["input"]["units"], "px")
        self.assertAlmostEqual(p["parameters"]["tolerance_mm"], 0.2)
        self.assertEqual(p["review"]["state"], "pending")

    def test_evidence_is_unmodified(self):
        for name in ("outliers", "missing_segment", "plate_two_holes_and_slot"):
            data, _, p = run(name)
            src = data.get("outlines") or [lp["points"] for lp in data["loops"]]
            with self.subTest(name=name):
                self.assertEqual([lp["points_px"] for lp in p["evidence"]["loops"]], src)

    def test_deterministic(self):
        data, _ = realize(CASES["rounded_rectangle"])
        a = resolve(observation_from_dict(data, "x"), Params())
        b = resolve(observation_from_dict(json.loads(json.dumps(data)), "x"), Params())
        self.assertEqual(json.dumps(a, sort_keys=True), json.dumps(b, sort_keys=True))

    def test_proposal_id_depends_on_input_params_version(self):
        data, _ = realize(CASES["noisy_rectangle"])
        ids = {
            resolve(observation_from_dict(data, "x"), Params())["proposal_id"],
            resolve(observation_from_dict(data, "y"), Params())["proposal_id"],
            resolve(observation_from_dict(data, "x"), Params(tolerance_px=2.5))["proposal_id"],
        }
        self.assertEqual(len(ids), 3)


class FixtureBehaviourTests(unittest.TestCase):
    """Each dev fixture's stated expectation (fixtures.dev_cases)."""

    def test_rectangle_lines_and_perpendiculars(self):
        for name in (
            "noisy_rectangle",
            "outliers",
            "small_scale_rectangle",
            "large_sparse_rectangle",
            "uncalibrated_rectangle",
        ):
            _, _, p = run(name)
            with self.subTest(name=name):
                self.assertEqual(kinds_by_loop(p), [["line"] * 4])
                self.assertEqual(decisions(p, "perpendicular"), Counter(proposed=4))
                self.assertTrue(p["proposed_geometry"]["loops"][0]["closed_profile"])

    def test_skewed_quadrilateral_stays_skewed(self):
        _, _, p = run("skewed_quadrilateral")
        self.assertEqual(decisions(p, "perpendicular"), Counter(rejected_by_evidence=4))
        self.assertEqual(decisions(p, "parallel"), Counter(proposed=2))
        ents = p["proposed_geometry"]["loops"][0]["entities"]
        for e, f in zip(ents, ents[1:] + ents[:1], strict=True):
            d1 = np.subtract(e["end"], e["start"])
            d2 = np.subtract(f["end"], f["start"])
            ang = math.degrees(math.acos(abs(d1 @ d2) / np.linalg.norm(d1) / np.linalg.norm(d2)))
            self.assertGreater(abs(90 - ang), 4.5, "a skewed corner must not be squared")

    def test_rounded_rectangle(self):
        _, _, p = run("rounded_rectangle")
        self.assertTrue(cyclic_equal(kinds_by_loop(p)[0], ["line", "arc"] * 4))
        self.assertEqual(decisions(p, "perpendicular"), Counter(proposed=4))
        self.assertEqual(decisions(p, "equal_radius")["proposed"], 6)
        self.assertGreaterEqual(decisions(p, "tangent")["proposed"], 3)
        self.assertTrue(p["proposed_geometry"]["loops"][0]["closed_profile"])

    def test_circle_and_partial_arc(self):
        _, _, p = run("circle")
        self.assertEqual(kinds_by_loop(p), [["circle"]])
        self.assertAlmostEqual(p["primitives"][0]["fit"]["radius_px"], 60, delta=0.3)
        self.assertAlmostEqual(p["primitives"][0]["fit"]["diameter_mm"], 12.0, delta=0.06)
        _, _, p = run("partial_arc")
        self.assertEqual(kinds_by_loop(p), [["arc"]])
        self.assertAlmostEqual(p["primitives"][0]["fit"]["radius_px"], 80, delta=0.6)
        self.assertFalse(p["proposed_geometry"]["loops"][0]["closed_profile"])

    def test_missing_segment_is_not_bridged_silently(self):
        _, _, p = run("missing_segment")
        self.assertEqual(len(p["evidence"]["loops"][0]["gaps"]), 1)
        self.assertEqual(decisions(p, "collinear"), Counter(question=1))
        self.assertFalse(p["proposed_geometry"]["loops"][0]["closed_profile"])
        self.assertTrue(any(q["kind"] == "collinear" for q in p["questions"]))

    def test_outliers_flagged_not_removed(self):
        data, _, p = run("outliers")
        self.assertEqual(len(p["evidence"]["loops"][0]["outliers"]), 4)
        self.assertEqual(p["evidence"]["loops"][0]["n_points"], len(data["outlines"][0]))

    def test_plate_equal_radius_only_where_evidence_supports(self):
        _, _, p = run("plate_two_holes_and_slot")
        self.assertEqual(kinds_by_loop(p)[1:3], [["circle"], ["circle"]])
        self.assertTrue(cyclic_equal(kinds_by_loop(p)[3], ["line", "arc", "line", "arc"]))
        prim = {x["id"]: x for x in p["primitives"]}
        for c in p["constraints"]:
            if c["type"] != "equal_radius":
                continue
            ka = {prim[e]["kind"] for e in c["entities"]}
            with self.subTest(c=c["id"]):
                # hole r=14 vs hole r=14 and slot r=12 vs slot r=12: proposed; hole vs slot: not
                # proposed
                self.assertEqual(c["decision"] == "proposed", len(ka) == 1, c["reason"])
        self.assertTrue(all(lo["closed_profile"] for lo in p["proposed_geometry"]["loops"]))

    def test_freeform_is_unsupported_not_invented(self):
        _, _, p = run("freeform_blob")
        self.assertEqual(kinds_by_loop(p), [["freeform"]])
        self.assertTrue(
            any("freeform" in u["reason"] or u["about"] == ["L0"] for u in p["unsupported"])
        )
        self.assertFalse(p["proposed_geometry"]["loops"][0]["closed_profile"])

    def test_uncalibrated_reports_insufficient_calibration(self):
        _, _, p = run("uncalibrated_rectangle")
        self.assertIsNone(p["input"]["scale_mm_per_px"])
        self.assertIsNone(p["parameters"]["tolerance_mm"])
        self.assertTrue(any(u["about"] == ["input.calibration"] for u in p["unsupported"]))
        self.assertIsNone(p["primitives"][0]["residual"]["max_mm"])

    def test_every_decision_is_in_vocabulary(self):
        for name in CASES:
            _, _, p = run(name)
            with self.subTest(name=name):
                for c in p["constraints"]:
                    self.assertIn(
                        c["decision"],
                        {"proposed", "question", "rejected_by_evidence", "unsupported"},
                    )
                for x in p["primitives"]:
                    self.assertIn(x["decision"], {"proposed", "question", "unsupported"})

    def test_scale_invariance_in_px_terms(self):
        """Same shape, same tolerance in px: tolerance_mm follows the scale; decisions do not."""
        import dataclasses

        base = CASES["noisy_rectangle"]
        a = resolve(observation_from_dict(realize(base)[0], "a"), Params())
        b = resolve(
            observation_from_dict(realize(dataclasses.replace(base, scale_mm_per_px=0.5))[0], "b"),
            Params(),
        )
        self.assertEqual(
            [c["decision"] for c in a["constraints"]], [c["decision"] for c in b["constraints"]]
        )
        self.assertAlmostEqual(b["parameters"]["tolerance_mm"], 1.0)


class ReviewTests(unittest.TestCase):
    def test_accept_proposed_leaves_questions_open(self):
        _, _, p = run("missing_segment")
        r = apply_decisions(p, [], [], by="tester", accept_proposed=True, now="T")
        self.assertEqual(r["review"]["state"], "partial")
        q = [c["id"] for c in p["constraints"] if c["decision"] == "question"]
        self.assertEqual(r["review"]["undecided"], sorted(q))
        self.assertEqual(p["review"]["decisions"], [], "input proposal must not be mutated")

    def test_unsupported_cannot_be_accepted(self):
        _, _, p = run("freeform_blob")
        pid = p["primitives"][0]["id"]
        with self.assertRaises(ReviewError):
            apply_decisions(p, [pid], [], by="tester")

    def test_rejected_by_evidence_needs_override_note(self):
        _, _, p = run("skewed_quadrilateral")
        cid = next(c["id"] for c in p["constraints"] if c["decision"] == "rejected_by_evidence")
        with self.assertRaises(ReviewError):
            apply_decisions(p, [cid], [], by="tester")
        r = apply_decisions(p, [cid], [], by="tester", override_note="drawing says 90 deg", now="T")
        rec = next(d for d in r["review"]["decisions"] if d["item"] == cid)
        self.assertEqual(rec["override"], "drawing says 90 deg")
        self.assertEqual(rec["resolver_decision"], "rejected_by_evidence")

    def test_reviewer_required_and_no_conflicts(self):
        _, _, p = run("noisy_rectangle")
        with self.assertRaises(ReviewError):
            apply_decisions(p, [], [], by=" ")
        cid = p["constraints"][0]["id"]
        with self.assertRaises(ReviewError):
            apply_decisions(p, [cid], [cid], by="t")
        with self.assertRaises(ReviewError):
            apply_decisions(p, ["c999"], [], by="t")


class ExportTests(unittest.TestCase):
    def reviewed(self, name):
        _, obs, p = run(name)
        return obs, apply_decisions(p, [], [], by="tester", accept_proposed=True, now="T")

    def test_rectangle_exports_closed_profile_in_mm(self):
        obs, r = self.reviewed("noisy_rectangle")
        asset, report = to_sketch_asset(r, obs, "rect")
        self.assertEqual(asset["units"], "mm")
        self.assertEqual(len(asset["profiles"]), 1)
        segs = asset["profiles"][0]["segments"]
        self.assertEqual(len(segs), 4)
        for s, t in zip(segs, segs[1:] + segs[:1], strict=True):
            self.assertEqual(s["end"], t["start"])
        lengths = sorted(math.dist(s["start"], s["end"]) for s in segs)
        self.assertAlmostEqual(lengths[0], 12.0, delta=0.1)  # GT 120 px x 0.1 mm/px
        self.assertAlmostEqual(lengths[-1], 20.0, delta=0.1)
        self.assertEqual(asset["provenance"]["input_sha256"], "sha-noisy_rectangle")

    def test_uncalibrated_refuses(self):
        obs, r = self.reviewed("uncalibrated_rectangle")
        with self.assertRaises(ExportError):
            to_sketch_asset(r, obs, "x")

    def test_changed_input_or_params_refuses(self):
        obs, r = self.reviewed("noisy_rectangle")
        obs2 = observation_from_dict(realize(CASES["noisy_rectangle"])[0], "different")
        with self.assertRaises(ExportError):
            accepted_geometry(r, obs2)
        r2 = json.loads(json.dumps(r))
        r2["parameters"]["tolerance_px"] = 3.0
        with self.assertRaises(ExportError):
            accepted_geometry(r2, obs)

    def test_open_or_unreviewed_loops_are_skipped_not_approximated(self):
        obs, r = self.reviewed("missing_segment")
        with self.assertRaises(ExportError):
            to_sketch_asset(r, obs, "x")  # the only loop is open: nothing exportable
        _, obs, p = run("noisy_rectangle")
        with self.assertRaises(ExportError):
            to_sketch_asset(apply_decisions(p, [], [], by="t", now="T"), obs, "x")

    def test_rejecting_a_constraint_changes_export(self):
        _, obs, p = run("noisy_rectangle")
        perp = [c["id"] for c in p["constraints"] if c["type"] == "perpendicular"]
        r = apply_decisions(p, [], perp, by="t", accept_proposed=True, now="T")
        asset, report = to_sketch_asset(r, obs, "x")
        self.assertNotIn(perp[0], asset["provenance"]["accepted_items"])

    def test_plate_with_holes_and_slot(self):
        obs, r = self.reviewed("plate_two_holes_and_slot")
        asset, report = to_sketch_asset(r, obs, "plate")
        self.assertEqual(len(asset["circles"]), 2)
        self.assertEqual({p["role"] for p in asset["profiles"]}, {"outline", "cutout"})
        for c in asset["circles"]:
            self.assertAlmostEqual(c["diameter"], 2.8, delta=0.06)
        slot = next(p for p in asset["profiles"] if p["role"] == "cutout")
        for s in slot["segments"]:
            if s["type"] == "arc":
                self.assertAlmostEqual(
                    math.dist(s["start"], s["center"]), math.dist(s["end"], s["center"]), delta=1e-6
                )

    @unittest.skipUnless(os.environ.get("MESH2CAD_ROOT"), "MESH2CAD_ROOT not set")
    def test_mesh2cad_validates_and_exports(self):
        root = Path(os.environ["MESH2CAD_ROOT"])
        for name in (
            "noisy_rectangle",
            "rounded_rectangle",
            "plate_two_holes_and_slot",
            "skewed_quadrilateral",
        ):
            obs, r = self.reviewed(name)
            asset, _ = to_sketch_asset(r, obs, name)
            with self.subTest(name=name), tempfile.TemporaryDirectory() as td:
                out = mesh2cad_export(asset, root, Path(td) / "a.dxf", Path(td) / "a.svg")
                self.assertEqual(out["validated_by"], "mra.sketch_assets.SketchAsset.from_dict")
                self.assertGreater((Path(td) / "a.dxf").stat().st_size, 100)


class OverlayAndCliTests(unittest.TestCase):
    def test_overlay_renders_evidence_and_geometry(self):
        for name in (
            "plate_two_holes_and_slot",
            "outliers",
            "missing_segment",
            "freeform_blob",
            "uncalibrated_rectangle",
        ):
            _, _, p = run(name)
            svg = render(p)
            with self.subTest(name=name):
                self.assertTrue(svg.startswith("<svg"))
                self.assertIn("evidence", svg)
                self.assertIn(p["proposal_id"], svg)
        self.assertIn("UNCALIBRATED", render(run("uncalibrated_rectangle")[2]))

    def test_cli_end_to_end(self):
        data, _ = realize(CASES["rounded_rectangle"])
        env = dict(os.environ, PYTHONPATH=str(HERE.parent))
        with tempfile.TemporaryDirectory() as td:
            t = Path(td)
            (t / "in.json").write_text(json.dumps(data))

            def cli(*args):
                return subprocess.run(
                    [sys.executable, "-m", "sketch_intent", *args],
                    env=env,
                    cwd=td,
                    capture_output=True,
                    text=True,
                )

            r = cli("resolve", "in.json", "-o", "p.json", "--svg", "p.svg")
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertEqual(
                cli("resolve", "in.json", "-o", "p.json").returncode, 1
            )  # no silent overwrite
            r = cli("review", "p.json", "-o", "r.json", "--by", "tester", "--accept-proposed")
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertEqual(cli("review", "p.json", "-o", "r.json", "--by", "x").returncode, 1)
            reviewed = json.loads((t / "r.json").read_text())
            self.assertIn("parent_sha256", reviewed["review"])
            q = [c["id"] for c in reviewed["constraints"] if c["decision"] == "question"]
            if q:
                r = cli("review", "r.json", "-o", "r2.json", "--by", "tester", "--reject", *q)
                self.assertEqual(r.returncode, 0, r.stderr)
                final = "r2.json"
            else:
                final = "r.json"
            r = cli("export", final, "--input", "in.json", "-o", "a.sketch.json")
            self.assertEqual(r.returncode, 0, r.stderr)
            asset = json.loads((t / "a.sketch.json").read_text())
            self.assertEqual(asset["schema_version"], "1.1")
            self.assertEqual(
                cli("overlay", final, "--input", "in.json", "-o", "acc.svg").returncode, 0
            )
            (t / "in.json").write_text(json.dumps(data) + " ")  # evidence changed
            r = cli("export", final, "--input", "in.json", "-o", "b.sketch.json")
            self.assertEqual(r.returncode, 1)
            self.assertIn("sha256", r.stderr)

    def test_load_observation_hashes_bytes(self):
        with tempfile.TemporaryDirectory() as td:
            f = Path(td) / "x.json"
            f.write_text(
                json.dumps({"format": "shadowscan-outline", "outlines": [[[0, 0], [9, 0], [9, 9]]]})
            )
            import hashlib

            self.assertEqual(
                load_observation(f).source_sha256, hashlib.sha256(f.read_bytes()).hexdigest()
            )


if __name__ == "__main__":
    unittest.main()
