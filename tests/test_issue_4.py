"""Regression test suite for bounty Issue #4.

Defect under test: the ``Page`` pagination guard in ``github/github.go``
must defensively validate argument boundaries (zero, negative, and
out-of-range page/per-page values) instead of letting invalid values
propagate to the GitHub API, and must preserve the exact pass-through
contract for valid inputs.

The suite exercises the real Go implementation by copying the repository's
``github/github.go`` into a temporary Go module and running a small harness
that prints the normalized results for a matrix of boundary and normal
inputs. No dummy files are left in the repository.

Run with:
    python3 -m unittest tests/test_issue_4.py
"""

import os
import shutil
import subprocess
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GITHUB_GO = os.path.join(REPO_ROOT, "github", "github.go")

GO_MOD = "module pgtest\n\ngo 1.21\n"

GO_HARNESS = """package main

import (
	"fmt"
	"math"

	"pgtest/github"
)

func main() {
	cases := [][2]int{
		// Normal, in-range inputs (must pass through unchanged).
		{1, 30},
		{2, 50},
		{7, 100},
		// Boundary / invalid inputs (must be defensively coerced).
		{0, 0},
		{-3, -10},
		{1, 0},
		{0, 30},
		{1, 101},
		{2, 1000000},
		// Machine integer extremes (must not crash or overflow).
		{math.MinInt, math.MinInt},
		{math.MaxInt, math.MaxInt},
	}
	for _, c := range cases {
		lo := github.Page(c[0], c[1])
		fmt.Printf("%d,%d=%d,%d|%s\\n", c[0], c[1], lo.Page, lo.PerPage, lo.Values().Encode())
	}
}
"""


@unittest.skipUnless(shutil.which("go"), "Go toolchain is required to execute the github.Page contract")
class TestIssue4PageDefensiveGuards(unittest.TestCase):
    """Verify github.Page handles boundary inputs gracefully and keeps
    100% backward compatibility for valid payloads."""

    @classmethod
    def setUpClass(cls):
        cls._tmpdir = tempfile.mkdtemp(prefix="issue4_pgtest_")
        pkg_dir = os.path.join(cls._tmpdir, "github")
        os.makedirs(pkg_dir)
        with open(os.path.join(cls._tmpdir, "go.mod"), "w") as f:
            f.write(GO_MOD)
        # Test the real repository sources, copied verbatim into the module.
        github_dir = os.path.dirname(GITHUB_GO)
        for name in os.listdir(github_dir):
            if name.endswith(".go") and not name.endswith("_test.go"):
                shutil.copyfile(os.path.join(github_dir, name),
                                os.path.join(pkg_dir, name))
        with open(os.path.join(cls._tmpdir, "main.go"), "w") as f:
            f.write(GO_HARNESS)
        cls.rows = cls._run_harness(cls._tmpdir)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls._tmpdir, ignore_errors=True)

    @staticmethod
    def _run_harness(cwd):
        env = dict(os.environ, GOPROXY="off", GOFLAGS="")
        proc = subprocess.run(
            ["go", "run", "."],
            cwd=cwd,
            env=env,
            capture_output=True,
            text=True,
            timeout=180,
        )
        if proc.returncode != 0:
            raise AssertionError(
                "go harness failed to build/run (unhandled exception path):\n"
                + proc.stderr
            )
        rows = {}
        for line in proc.stdout.strip().splitlines():
            left, query = line.split("|", 1)
            inputs, outputs = left.split("=", 1)
            in_page, in_per = (int(x) for x in inputs.split(","))
            out_page, out_per = (int(x) for x in outputs.split(","))
            rows[(in_page, in_per)] = (out_page, out_per, query)
        return rows

    def result_for(self, page, per_page):
        self.assertIn(
            (page, per_page),
            self.rows,
            f"harness produced no row for input Page({page}, {per_page})",
        )
        return self.rows[(page, per_page)]

    def test_01_boundary_and_null_defense(self):
        """Zero, negative, and over-limit inputs are safely coerced instead
        of propagating to the API or crashing."""
        self.assertEqual(self.result_for(0, 0)[:2], (1, 30))
        self.assertEqual(self.result_for(-3, -10)[:2], (1, 30))
        self.assertEqual(self.result_for(1, 0)[:2], (1, 30))
        self.assertEqual(self.result_for(0, 30)[:2], (1, 30))
        self.assertEqual(self.result_for(1, 101)[:2], (1, 100))
        self.assertEqual(self.result_for(2, 1000000)[:2], (2, 100))

    def test_02_normal_execution_contract(self):
        """Valid in-range pagination arguments pass through unchanged,
        preserving full backward compatibility."""
        self.assertEqual(self.result_for(1, 30)[:2], (1, 30))
        self.assertEqual(self.result_for(2, 50)[:2], (2, 50))
        self.assertEqual(self.result_for(7, 100)[:2], (7, 100))

    def test_03_zero_crash_guarantee(self):
        """Machine integer extremes are contained: no panic/overflow, and
        every normalized result stays inside the API-safe envelope with a
        well-formed query encoding."""
        extreme_rows = [
            (inp, out) for inp, out in self.rows.items() if abs(inp[0]) > 2**40
        ]
        self.assertEqual(len(extreme_rows), 2, "expected both int-extreme rows")
        for (in_page, _), (out_page, out_per, query) in extreme_rows:
            self.assertGreaterEqual(out_page, 1)
            self.assertTrue(1 <= out_per <= 100)
            self.assertEqual(query, f"page={out_page}&per_page={out_per}")

    def test_04_integration_consistency(self):
        """The patched source still compiles as a whole, the harness is
        deterministic across runs, and the Page guard lives in the target
        file (in-place repair, no wrapper scripts)."""
        rerun = self._run_harness(self._tmpdir)
        self.assertEqual(self.rows, rerun, "github.Page must be deterministic")
        with open(GITHUB_GO) as f:
            src = f.read()
        self.assertIn("func Page(page, perPage int) ListOptions", src)
        remedy = os.path.join(REPO_ROOT, "scripts", "issue_4_remedy.py")
        self.assertFalse(os.path.exists(remedy), "foreign dummy remedy file detected")


if __name__ == "__main__":
    unittest.main()
