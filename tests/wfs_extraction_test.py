import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import extract_wfs_test_clips as wfs


class ExtractionTests(unittest.TestCase):
    def test_noncontiguous_fragments_preserved(self):
        source = io.BytesIO(b'AAAABBBBCCCCDDDD')
        clips = [{'channel_candidate': 1, 'primary_fragment': 7,
                  'offsets': [12, 0], 'candidate_recording_bytes': 6}]
        with tempfile.TemporaryDirectory() as temp, patch.object(wfs, 'FRAGMENT_SIZE', 4):
            folder = Path(temp)
            wfs.extract(source, folder, clips)
            self.assertEqual((folder/'channel-1-record-7.raw').read_bytes(), b'DDDDAAAA')
            self.assertEqual(source.getvalue(), b'AAAABBBBCCCCDDDD')
            report = json.loads((folder/'manifest.json').read_text())
            self.assertEqual(report['recordings'][0]['sha256'], hashlib.sha256(b'DDDDAAAA').hexdigest())
            with self.assertRaises(FileExistsError):
                wfs.extract(source, folder, clips)

    def test_short_read_never_marked_complete(self):
        clips = [{'channel_candidate': 1, 'primary_fragment': 7, 'offsets': [0]}]
        with tempfile.TemporaryDirectory() as temp, patch.object(wfs, 'FRAGMENT_SIZE', 4):
            with self.assertRaises(ValueError):
                wfs.extract(io.BytesIO(b'A'), Path(temp), clips)
            self.assertFalse((Path(temp)/'manifest.json').exists())

    def test_reject_invalid_plan(self):
        clip = {'primary_fragment': 7, 'channel_candidate': 1, 'issues': [],
                'last_fragment_blocks_raw': 1, 'fragments': [0]}
        with patch.object(wfs, 'SELECTED', {7: 1}), patch.object(wfs, 'analyze', return_value={'candidate_recordings': [clip]}):
            self.assertEqual(wfs.make_plan(b'')[0]['candidate_recording_bytes'], 512)
            for update in [{'issues': ['cycle']}, {'channel_candidate': 2},
                           {'last_fragment_blocks_raw': 0}, {'fragments': [10**12]},
                           {'fragments': [0]*100}]:
                old = clip.copy();clip.update(update)
                with self.assertRaises(ValueError): wfs.make_plan(b'')
                clip.clear();clip.update(old)

if __name__ == '__main__':
    unittest.main()
