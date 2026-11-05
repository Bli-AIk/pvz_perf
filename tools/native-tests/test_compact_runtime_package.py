"""Check decoded pixels, alpha, metadata and unchanged runtime records."""
import io
import random
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

from PIL import Image, PngImagePlugin

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import compact_runtime_package as compact


def png():
    image = Image.new("RGBA", (128, 64))
    rng = random.Random(1234)
    image.putdata([(100 + rng.randrange(3), 120 + rng.randrange(3), 140 + rng.randrange(3), (x + y) % 256)
                   for y in range(64) for x in range(128)])
    metadata = PngImagePlugin.PngInfo()
    metadata.add_text("test", "preserve original metadata")
    buffer = io.BytesIO()
    image.save(buffer, format="PNG", compress_level=0, pnginfo=metadata)
    return buffer.getvalue()


class CompactPackageTests(unittest.TestCase):
    def test_lossless_pixels_and_metadata(self):
        source = png()
        cooked, row = compact.compact_png("atlas.png", source, "lossless")
        self.assertLess(len(cooked), len(source))
        self.assertEqual(row["max_rgb_error"], 0)
        with Image.open(io.BytesIO(cooked)) as image:
            self.assertEqual(image.info["test"], "preserve original metadata")

    def test_light_lossy_preserves_alpha_and_bounds_error(self):
        cooked, row = compact.compact_png("compiled/test.atlas.png", png(), "light-lossy")
        self.assertGreater(row["max_rgb_error"], 0)
        self.assertLessEqual(row["max_rgb_error"], 2)
        self.assertTrue(row["alpha_unchanged"])
        self.assertTrue(cooked.startswith(compact.SIGNATURE))

    def test_font_always_lossless(self):
        _, row = compact.compact_png("data/font.png", png(), "light-lossy")
        self.assertEqual(row["max_rgb_error"], 0)

    def test_standalone_alpha_masks_always_lossless(self):
        _, row = compact.compact_png("images/pool_base_.png", png(), "light-lossy")
        self.assertEqual(row["max_rgb_error"], 0)

    def test_package_preserves_paths_non_images_and_input(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, output = root / "main.pak64", root / "main-compact.pak64"
            with zipfile.ZipFile(source, "w") as archive:
                archive.writestr("atlas.png", png())
                archive.writestr("compiled/particle-runtime/test.compiled", b"PTC1\x00\xff")
                archive.writestr("sounds/test.ogg", b"not transcoded")
            before = source.read_bytes()
            compact.compact_package(source, output, "light-lossy")
            self.assertEqual(before, source.read_bytes())
            with zipfile.ZipFile(source) as a, zipfile.ZipFile(output) as b:
                self.assertEqual(a.namelist(), b.namelist())
                for name in a.namelist()[1:]:
                    self.assertEqual(a.read(name), b.read(name))
                self.assertTrue(all(i.compress_type == zipfile.ZIP_STORED for i in b.infolist()))
            with self.assertRaises(ValueError):
                compact.compact_package(source, source)


if __name__ == "__main__":
    unittest.main()
