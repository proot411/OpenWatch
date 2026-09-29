#!/usr/bin/env python3
"""Read-only WFS header inspection. No mounting, repairs, or media extraction.

Reference: de Sousa & Brito (2022), DOI 10.22533/at.ed.216282205059.
Field interpretations are research candidates, not validated firmware support.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct

SAMPLE_SIZE = 65536


def inspect(data):
    if len(data) < SAMPLE_SIZE:
        raise ValueError('Need a complete 64 KiB header sample')
    if data[:6] not in (b'WFS0.4', b'WFS0.5') or data[510:512] != b'XM':
        raise ValueError('Expected WFS0.4/0.5 and XM signatures were not found')
    fields = {
        'fragment_count': 0x20,
        'block_size_bytes': 0x2c,
        'fragment_size_blocks': 0x30,
        'reserved_fragment_count': 0x38,
        'index_start_raw': 0x44,
        'data_start_raw': 0x48,
    }
    values = {name: struct.unpack_from('<I', data, 0x3000 + offset)[0]
              for name, offset in fields.items()}
    return {
        'format_signature': data[:6].decode('ascii'),
        'sample_bytes': len(data),
        'sample_sha256': hashlib.sha256(data).hexdigest(),
        'candidate_superblock_first_80_bytes_hex': data[0x3000:0x3050].hex(),
        'candidate_fields_from_published_layout': values,
        'candidate_fragment_size_bytes': values['block_size_bytes'] * values['fragment_size_blocks'],
        'interpretation': 'Unverified on this recorder. No index traversal or playback support claimed.',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', help='Recorder disk or existing raw header sample; opened read-only')
    parser.add_argument('--sample', help='Save a NEW 64 KiB sample file; existing paths are never overwritten')
    args = parser.parse_args()
    try:
        # Source is never opened for writing. Reads are bounded, even for a whole disk.
        with open(args.source, 'rb', buffering=0) as source:
            chunks = []
            remaining = SAMPLE_SIZE
            while remaining:
                chunk = source.read(remaining)
                if not chunk:
                    raise ValueError('Source is shorter than 64 KiB')
                chunks.append(chunk)
                remaining -= len(chunk)
        data = b''.join(chunks)
        result = inspect(data)
        if args.sample:
            # O_EXCL prevents overwriting files, symlinks, or device nodes.
            fd = os.open(args.sample, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
            with os.fdopen(fd, 'wb') as output:
                output.write(data)
            result['saved_sample'] = str(Path(args.sample).absolute())
        print(json.dumps(result, indent=2))
    except (OSError, ValueError) as exc:
        parser.exit(1, f'Inspection stopped: {exc}\n')


if __name__ == '__main__':
    main()
