import unittest
from analyze_stages import analyze
from pathlib import Path

class AnalysisTests(unittest.TestCase):
    def test_real_export(self):
        fixture=Path(__file__).with_name('stage-fixture.txt').read_text()
        rows=analyze(fixture)['intervals']
        self.assertEqual(len(rows),3)
        self.assertEqual(rows[0]['stages']['native_sr']['mean_ms'],3)
        self.assertEqual(rows[0]['stages']['nr_after_full']['mean_ms'],7)
        self.assertEqual(rows[0]['stages']['pipeline_after']['mean_ms'],12)
        self.assertIsNone(rows[1]['stages']['native_sr']['mean_ms'])
        self.assertEqual(rows[2]['stages']['native_sr']['mean_ms'],8)
        self.assertIsNone(rows[2]['stages']['nr_after_full']['mean_ms'])
        self.assertEqual(rows[0]['seconds'],5)
    def test_restarts(self):
        lines=[x for x in Path(__file__).with_name('stage-fixture.txt').read_text().splitlines() if x.startswith('GPU stages')]
        self.assertEqual(len(analyze('\n'.join(lines+lines))['intervals']),6)
        self.assertEqual(len(analyze('\n'.join(lines+['Standalone initializing;']+lines))['intervals']),6)
        # Counter reset with monotonically increasing QPC also establishes baseline.
        reset=lines[0].replace('qpc=100 ', 'qpc=20100 ')
        self.assertEqual(len(analyze('\n'.join(lines+[reset]))['intervals']),3)
    def test_incomplete(self):
        with self.assertRaises(ValueError): analyze('GPU stages v1 pid=1 qpc=10 frequency=1000 native_sr=2.0,1')
if __name__=='__main__': unittest.main()
