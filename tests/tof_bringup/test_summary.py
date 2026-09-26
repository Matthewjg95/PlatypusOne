import importlib.util
import pathlib
import unittest
spec = importlib.util.spec_from_file_location('summary', pathlib.Path(__file__).resolve().parents[2] / 'tools/tof_bringup/summarize.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class SummaryTest(unittest.TestCase):
    def fixture(self):
        # Synthetic only: one invalid zone, known 10 Hz cadence, known bias.
        return '\n'.join(f'zone,{f},{f*100},2,{z},1,{5 if z else 0},{500+f},1,10,1,{int(z != 0)},0'
                         for f in range(1, 4) for z in range(16))

    def test_metrics_and_invalid_not_zero(self):
        result = module.summarize(self.fixture(), 500)
        self.assertEqual(result['achieved_hz'], 10)
        self.assertEqual(result['zones'][1]['bias_mm'], 2)
        self.assertIsNone(result['zones'][0]['median_mm'])
        self.assertEqual(result['zones'][0]['strict_valid_pct'], 0)

    def test_truncated_and_reboot_rejected(self):
        with self.assertRaises(ValueError):
            module.summarize(self.fixture().rsplit('\n', 1)[0], 500)
        with self.assertRaises(ValueError):
            module.summarize(self.fixture() + '\n' + self.fixture(), 500)

if __name__ == '__main__':
    unittest.main()
