"""Strict proofs and explicitly bounded, honestly unproven publication policies."""
import os as _devtools_os
from pathlib import Path as _DevToolsPath
DEVTOOLS_ROOT = _DevToolsPath(_devtools_os.environ.get("PVZ_DEVTOOLS_SOURCE_DIR", str(_DevToolsPath(__file__).resolve().parents[3] / "PvzDevTools")))
DEVTOOLS_REFERENCE = DEVTOOLS_ROOT / "tests/reference/python"
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import exact_atlas
from resource_build_config import AtlasSettings

spec = importlib.util.spec_from_file_location(
    "proof_pipeline", DEVTOOLS_REFERENCE / "resourcePipeline/src/build_resources.py")
pipeline = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pipeline)


class AtlasProofPublicationTests(unittest.TestCase):
    def setUp(self):
        self.unit = {"name": "fixture", "settings": AtlasSettings()}
        self.layout = {"width": 10, "height": 20, "exact": {
            "optimal": True, "status": "OPTIMAL", "best_bound": 200,
            "objective_area": 200, "gap": 0.0, "model_version": exact_atlas.MODEL_VERSION,
            "solver": "ortools-cp-sat",
            "solver_version": exact_atlas._version()}}

    def test_complete_current_proof_is_accepted(self):
        pipeline.require_publishable_atlas(self.unit, self.layout)

    def test_unproven_or_foreign_proof_is_rejected(self):
        for patch in ({"optimal": False}, {"status": "FEASIBLE"}, {"status": "UNKNOWN"},
                      {"best_bound": 199}, {"objective_area": 201},
                      {"model_version": "old-model"}, {"solver_version": "old-solver"}):
            with self.subTest(patch=patch):
                layout = dict(self.layout, exact=dict(self.layout["exact"], **patch))
                with self.assertRaisesRegex(ValueError, "publication refused"):
                    pipeline.require_publishable_atlas(self.unit, layout)
        with self.assertRaises(ValueError):
            pipeline.require_publishable_atlas(self.unit, {"width": 10, "height": 20})

    def test_relaxed_policy_accepts_honest_feasible_or_unknown_incumbent(self):
        unit = {"name": "bounded", "settings": AtlasSettings(require_optimal=False, time_limit_seconds=60)}
        for status in ("FEASIBLE", "UNKNOWN"):
            layout = dict(self.layout, exact=dict(self.layout["exact"], optimal=False,
                                                  status=status, best_bound=150, gap=0.25))
            pipeline.require_publishable_atlas(unit, layout)
            with self.assertRaises(ValueError):
                pipeline.require_publishable_atlas(self.unit, layout)
        # Opting out does not discard a real proof obtained within the budget.
        pipeline.require_publishable_atlas(unit, self.layout)

    def test_relaxed_policy_requires_a_positive_finite_budget(self):
        for seconds in (None, 0, -1, float("inf"), float("nan"), 86401, True):
            settings = AtlasSettings(require_optimal=False, time_limit_seconds=seconds)
            with self.subTest(seconds=seconds), self.assertRaises(ValueError):
                pipeline.require_publishable_atlas({"name": "fixture", "settings": settings}, self.layout)
        with self.assertRaises(ValueError):
            pipeline.require_publishable_atlas(
                {"name": "fixture", "settings": AtlasSettings(solver="maxrects", require_optimal=False,
                                                             time_limit_seconds=60)}, self.layout)

    def test_relaxed_mode_does_not_bypass_metadata_validation(self):
        unit = {"name": "bounded", "settings": AtlasSettings(require_optimal=False, time_limit_seconds=60)}
        valid = dict(self.layout["exact"], optimal=False, status="FEASIBLE", best_bound=150, gap=0.25)
        invalid = ({"status": "SOLVING"}, {"status": "INFEASIBLE"}, {"status": "MODEL_INVALID"},
                   {"solver": "maxrects"},
                   {"model_version": "old-model"}, {"solver_version": "old-solver"},
                   {"objective_area": 201}, {"objective_area": True},
                   {"best_bound": -1}, {"best_bound": 201}, {"best_bound": True},
                   {"gap": 0.0}, {"gap": -0.25}, {"gap": 1.25},
                   {"gap": float("nan")}, {"gap": float("inf")}, {"gap": True},
                   {"optimal": "false"}, {"optimal": True},
                   {"status": "OPTIMAL"}, {"status": "LOWER_BOUND_PROVEN"})
        for patch in invalid:
            with self.subTest(patch=patch), self.assertRaises(ValueError):
                pipeline.require_publishable_atlas(
                    unit, dict(self.layout, exact=dict(valid, **patch)))

    def test_proof_marker_requires_consistent_status_bound_and_gap_in_both_modes(self):
        for settings in (AtlasSettings(), AtlasSettings(require_optimal=False, time_limit_seconds=60)):
            unit = {"name": "fixture", "settings": settings}
            for patch in ({"optimal": False}, {"status": "FEASIBLE"}, {"status": "UNKNOWN"},
                          {"best_bound": 199}, {"gap": 0.01}):
                with self.subTest(settings=settings, patch=patch), self.assertRaises(ValueError):
                    pipeline.require_publishable_atlas(
                        unit, dict(self.layout, exact=dict(self.layout["exact"], **patch)))
            proven = dict(self.layout, exact=dict(self.layout["exact"], status="LOWER_BOUND_PROVEN"))
            pipeline.require_publishable_atlas(unit, proven)

    def test_programmatic_config_cannot_publish_native_only_layout(self):
        for settings in (AtlasSettings(solver="maxrects"),
                         AtlasSettings(solver="maxrects", require_optimal=False, time_limit_seconds=60)):
            with self.assertRaises(ValueError):
                pipeline.require_publishable_atlas({"name": "fixture", "settings": settings}, self.layout)

    def test_progress_is_separate_from_existing_runtime_artifacts_and_stops_on_error(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            package = cache / "main.pak64"
            package.write_bytes(b"previous runtime")
            with self.assertRaisesRegex(RuntimeError, "fixture interruption"):
                with pipeline.atlas_proof_progress(cache, "fixture", 2, 9, time.monotonic()) as update:
                    update({"status": "FEASIBLE", "optimal": False, "objective_area": 200})
                    raise RuntimeError("fixture interruption")
            self.assertEqual(package.read_bytes(), b"previous runtime")
            progress = json.loads((cache / "atlas-proof-progress.json").read_text())
            self.assertEqual(progress["atlas"], "fixture")
            self.assertEqual(progress["exact"]["status"], "FEASIBLE")
            self.assertEqual(progress["index"], 2)
            self.assertEqual(progress["phase"], "solving")

    def test_progress_records_the_explicit_bounded_policy(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            with pipeline.atlas_proof_progress(
                    cache, "bounded", 1, 3, time.monotonic(),
                    require_optimal=False, time_limit_seconds=60) as update:
                update({"status": "UNKNOWN", "optimal": False, "objective_area": 200,
                        "best_bound": 150, "gap": 0.25})
            progress = json.loads((cache / "atlas-proof-progress.json").read_text())
            self.assertEqual(progress["phase"], "solving")
            self.assertFalse(progress["require_optimal"])
            self.assertEqual(progress["time_limit_seconds"], 60)


if __name__ == "__main__":
    unittest.main()
