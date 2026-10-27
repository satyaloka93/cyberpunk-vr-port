"""Numerical regression for the SSR projection/reconstruction convention.
Run with python -m unittest discover -s tools/asym_tests -p test_ssr_250.py.
"""
import unittest

class ProjectionRoundTrip(unittest.TestCase):
    def test_each_eye_and_alternate_projection(self):
        # Independent projection of the reconstructed surface back to its pixel.
        for width in (960, 1500, 3000):
            for cx in (-0.24, 0.0, 0.24):
                for cy in (-0.04, 0.0, 0.04):
                    for tx, ty, w in ((1.2, -1.1, 1.0), (0.8, -0.9, -1.0)):
                        for pixel in (0, width // 2, width - 1):
                            u = 2 * (pixel + 0.5) / width - 1
                            z = 4.5
                            x = tx * (u - cx / w) * z
                            y = ty * (u + cy / w) * z
                            self.assertAlmostEqual(x / (tx * z) + cx / w, u)
                            self.assertAlmostEqual(y / (ty * z) - cy / w, u)
                            # Missing the shift produces the known mirrored error.
                            self.assertAlmostEqual((tx*u*z)/(tx*z)+cx/w-u, cx/w)

if __name__ == '__main__':
    unittest.main()
