#!/usr/bin/env python3
"""Analyze a copied WFS descriptor index; never opens the recorder disk.
Observed variant: 32-byte records, fragment-number links, camera code 2+4*n.
Results are candidates; this does not establish which entries the NVR exposes.
Layout reference: docs/wfs-investigation.md.
"""
import argparse
from collections import Counter
from datetime import datetime
import json
from pathlib import Path
import struct


def timestamp(raw):
    try:
        return datetime(2000 + (raw >> 26), (raw >> 22) & 15,
                        (raw >> 17) & 31, (raw >> 12) & 31,
                        (raw >> 6) & 63, raw & 63).isoformat(' ')
    except ValueError:
        return None


def analyze(data):
    if len(data) % 32 or len(data) > 32 * 1024 * 1024:
        raise ValueError('Index must be a multiple of 32 bytes, at most 32 MiB')
    records = [data[i:i + 32] for i in range(0, len(data), 32)]
    u32 = lambda b, o: struct.unpack_from('<I', b, o)[0]
    clips = []
    for start, b in enumerate(records):
        if b[1] not in (2, 3):
            continue
        count = struct.unpack_from('<H', b, 2)[0] + 1
        chain, visited, errors = [], set(), []
        current = start
        for ordinal in range(count):
            if current >= len(records):
                errors.append('chain leaves captured index'); break
            if current in visited:
                errors.append('cycle'); break
            r = records[current]
            if r[31] != b[31] or u32(r, 24) != start:
                errors.append('channel or owner mismatch'); break
            if ordinal and (r[1] != 1 or u32(r, 4) != chain[-1]
                            or struct.unpack_from('<H', r, 2)[0] != ordinal):
                errors.append('continuation type, previous link or ordinal mismatch'); break
            visited.add(current); chain.append(current)
            current = u32(r, 8)
        code = b[31]
        begin, end = timestamp(u32(b, 12)), timestamp(u32(b, 16))
        if begin is None or end is None or end < begin:
            errors.append('invalid recording dates')
        clips.append({'primary_fragment': start,
                      'channel_candidate': (code - 2) // 4 + 1 if code >= 2 and (code - 2) % 4 == 0 else None,
                      'channel_raw': code, 'begin': begin, 'end': end,
                      'declared_fragments': count, 'validated_fragments': len(chain),
                      'last_fragment_blocks_raw': struct.unpack_from('<H', b, 22)[0],
                      'fragments': chain, 'issues': errors})
    return {'captured_descriptors': len(records),
            'attribute_counts': dict(Counter(b[1] for b in records)),
            'candidate_recordings': clips}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('index', type=Path)
    args = parser.parse_args()
    if args.index.stat().st_size > 32 * 1024 * 1024:
        parser.error('Index exceeds 32 MiB limit')
    print(json.dumps(analyze(args.index.read_bytes()), indent=2))
