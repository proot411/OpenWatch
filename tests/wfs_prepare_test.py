import sys
from pathlib import Path
import struct
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from prepare_wfs_library import parse, expression

def frame(second,codec=2):
    stamp=(26<<26)|(9<<22)|(15<<17)|(1<<12)|second
    payload=b'\0\0\0\1\x65\xaa'
    return b'\0\0\1\xfc'+bytes([codec,15,0,0])+struct.pack('<II',stamp,len(payload))+payload

class PreparationTests(unittest.TestCase):
    def test_repeated_second_and_padding(self):
        frames,anchors,codec,consumed,_=parse(frame(0)+frame(0)+frame(2)+b'\xff'*128)
        self.assertEqual(len(frames),3);self.assertEqual(len(anchors),2)
        self.assertEqual(anchors[1][0],2);self.assertEqual(consumed,3*len(frame(0)))
        self.assertIn('1000.0',expression(anchors))
    def test_corruption_rejected(self):
        for data in [frame(2)+frame(1),frame(0)[:-1],frame(0)+b'\xff\x12',frame(0)+frame(2,3),b'']:
            with self.assertRaises(ValueError):parse(data)

if __name__=='__main__':unittest.main()
