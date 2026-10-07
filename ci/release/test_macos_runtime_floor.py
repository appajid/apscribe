import unittest

from macos_runtime_floor import deployment_versions, display_version, version_tuple


class DeploymentVersionTests(unittest.TestCase):
    def test_reads_both_modern_and_legacy_load_commands(self):
        output = """Load command 0
      cmd LC_BUILD_VERSION
  cmdsize 32
 platform 1
    minos 14.0
      sdk 27.0
Load command 1
      cmd LC_VERSION_MIN_MACOSX
  cmdsize 16
  version 10.15
      sdk 11.0
Load command 2
      cmd LC_ID_DYLIB
  version 99.0
"""
        self.assertEqual(deployment_versions(output), [(14, 0, 0), (10, 15, 0)])

    def test_compares_and_displays_numeric_versions(self):
        self.assertGreater(version_tuple("14.10"), version_tuple("14.9"))
        self.assertEqual(display_version((27, 0, 0)), "27.0.0")
        self.assertEqual(display_version((14, 2, 1)), "14.2.1")


if __name__ == "__main__":
    unittest.main()
