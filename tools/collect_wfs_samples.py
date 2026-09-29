#!/usr/bin/env python3
"""Collect bounded read-only samples for the observed HGST WFS investigation.
Not a general filesystem reader. Never opens the source for writing.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import tempfile

EXPECTED_SIZE = 1000204886016
EXPECTED_HEADER = '8ce4b851403d9ce68e35555ce89149cbe021dbd1ba2e65003c8ccebe1f183cf7'
LIMIT = 8 * 1024 * 1024


def read_exact(source, offset, length):
    source.seek(offset)
    data = source.read(length)
    if len(data) != length:
        raise ValueError(f'Short read at {offset:#x}')
    return data


def plan(header, size):
    if size != EXPECTED_SIZE or hashlib.sha256(header).hexdigest() != EXPECTED_HEADER:
        raise ValueError('Disk size/header does not match the previously inspected HGST. No samples collected.')
    u32 = lambda offset: struct.unpack_from('<I', header, 0x3000 + offset)[0]
    block, fragment = u32(0x2c), u32(0x2c) * u32(0x30)
    index, data, count = u32(0x44) * block, u32(0x48) * block, u32(0x4c)
    if not (block == 512 and fragment == 2097152 and 65536 <= index < data
            and count > 0 and data + count * fragment <= size):
        raise ValueError('Candidate geometry failed validation')
    ranges = [
        ('header', 0, 65536),
        ('index_candidate', index, 2 * 1024 * 1024),
        ('data_candidate', data, 65536),
        ('fragment_128_candidate', data + u32(0x38) * fragment, 1024 * 1024),
        ('fragment_129_candidate', data + (u32(0x38) + 1) * fragment, 1024 * 1024),
        ('alternate_data_candidate', u32(0x58) * block, 65536),
        ('disk_tail', size - 65536, 65536),
    ]
    if sum(length for _, _, length in ranges) > LIMIT:
        raise ValueError('Collection exceeds read budget')
    for _, offset, length in ranges:
        if offset < 0 or offset + length > size:
            raise ValueError('Range outside disk')
    return ranges


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source')
    parser.add_argument('--output-parent', required=True, type=Path,
                        help='Existing directory on your PC; a new private folder is created inside it')
    args = parser.parse_args()
    try:
        with open(args.source, 'rb', buffering=0) as source:
            source.seek(0, os.SEEK_END)
            size = source.tell()
            header = read_exact(source, 0, 65536)
            ranges = plan(header, size)
            # mkdtemp creates a fresh directory; nothing existing is overwritten.
            folder = Path(tempfile.mkdtemp(prefix='wfs-samples-', dir=args.output_parent))
            manifest = {'source_size': size, 'source_header_sha256': EXPECTED_HEADER,
                        'status': 'Research samples; offsets remain hypotheses', 'samples': []}
            for name, offset, length in ranges:
                data = read_exact(source, offset, length)
                path = folder / (name + '.bin')
                with path.open('xb') as output:
                    output.write(data)
                path.chmod(0o600)
                manifest['samples'].append({'file': path.name, 'offset': offset,
                                            'length': length, 'sha256': hashlib.sha256(data).hexdigest()})
            with (folder / 'manifest.json').open('x') as output:
                json.dump(manifest, output, indent=2)
            print('Collected', sum(n for _, _, n in ranges), 'bytes into', folder)
    except (OSError, ValueError) as exc:
        parser.exit(1, f'Collection stopped: {exc}\n')
    finally:
        # Give only this run's new folder/files back to the invoking sudo user.
        # No device permission changes and no recursive traversal of existing paths.
        if 'folder' in locals() and os.geteuid() == 0 and os.environ.get('SUDO_UID', '').isdigit():
            uid = int(os.environ['SUDO_UID'])
            gid = int(os.environ.get('SUDO_GID', '0'))
            for path in folder.iterdir():
                os.chown(path, uid, gid)
            os.chown(folder, uid, gid)


if __name__ == '__main__':
    main()
