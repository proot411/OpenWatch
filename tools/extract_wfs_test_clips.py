#!/usr/bin/env python3
"""Read-only extraction experiment for the inspected HGST; writes only new files.
Keeps full fragments, including tails, for validating the candidate last length.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
from analyze_wfs_index import analyze
from collect_wfs_samples import EXPECTED_HEADER, EXPECTED_SIZE, read_exact

INDEX_OFFSET = 5242880
INDEX_LENGTH = 2097152
DATA_OFFSET = 1848508416
FRAGMENT_SIZE = 2097152
SELECTED = {16746: 1, 16747: 2, 16745: 3, 52341: 3}
MAX_BYTES = 32 * 1024 * 1024


def make_plan(index):
    clips = {c['primary_fragment']: c for c in analyze(index)['candidate_recordings']}
    result = []
    for primary, channel in SELECTED.items():
        if primary not in clips:
            raise ValueError('Selected recording missing')
        clip = clips[primary]
        if clip['issues'] or clip['channel_candidate'] != channel:
            raise ValueError('Selected recording failed chain/channel checks')
        tail = clip['last_fragment_blocks_raw'] * 512
        if not 0 < tail <= FRAGMENT_SIZE:
            raise ValueError('Invalid candidate final-fragment size')
        offsets = [DATA_OFFSET + f * FRAGMENT_SIZE for f in clip['fragments']]
        if any(o < DATA_OFFSET or o + FRAGMENT_SIZE > EXPECTED_SIZE for o in offsets):
            raise ValueError('Fragment outside disk')
        result.append({**clip, 'offsets': offsets,
                       'candidate_recording_bytes': (len(offsets)-1)*FRAGMENT_SIZE + tail})
    if sum(len(c['offsets'])*FRAGMENT_SIZE for c in result) > MAX_BYTES:
        raise ValueError('Read budget exceeded')
    return result


def extract(source, output, clips):
    for clip in clips:
        name = f"channel-{clip['channel_candidate']}-record-{clip['primary_fragment']}.raw"
        digest = hashlib.sha256()
        with (output / name).open('xb') as target:
            for offset in clip['offsets']:
                chunk = read_exact(source, offset, FRAGMENT_SIZE)
                target.write(chunk); digest.update(chunk)
        clip['file'] = name
        clip['sha256'] = digest.hexdigest()
    with (output / 'manifest.json').open('x') as target:
        json.dump({'status': 'Full-fragment research copies; final lengths require validation',
                   'recordings': clips}, target, indent=2)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source')
    p.add_argument('--samples', required=True, type=Path)
    p.add_argument('--output-parent', required=True, type=Path)
    a = p.parse_args()
    try:
        manifest = json.loads((a.samples/'manifest.json').read_text())
        saved = (a.samples/'index_candidate.bin').read_bytes()
        expected = next(s['sha256'] for s in manifest['samples'] if s['file']=='index_candidate.bin')
        if len(saved) != INDEX_LENGTH or hashlib.sha256(saved).hexdigest() != expected:
            raise ValueError('Saved index checksum or size mismatch')
        clips = make_plan(saved)
        required = sum(len(c['offsets'])*FRAGMENT_SIZE for c in clips)
        if shutil.disk_usage(a.output_parent).free < required + 16*1024*1024:
            raise ValueError('Not enough output space')
        with open(a.source, 'rb', buffering=0) as source:
            source.seek(0, os.SEEK_END)
            if source.tell() != EXPECTED_SIZE:
                raise ValueError('Disk size changed or wrong disk')
            if hashlib.sha256(read_exact(source,0,65536)).hexdigest() != EXPECTED_HEADER:
                raise ValueError('Disk header changed or wrong disk')
            if read_exact(source, INDEX_OFFSET, INDEX_LENGTH) != saved:
                raise ValueError('Disk index changed; collect fresh samples before extraction')
            # New private directory only. No mounts, chmod on devices, or writes to source.
            folder = Path(tempfile.mkdtemp(prefix='wfs-test-clips-', dir=a.output_parent))
            extract(source, folder, clips)
        print(f'Copied {required} bytes to {folder}')
    except (OSError, ValueError, KeyError, StopIteration) as exc:
        p.exit(1, f'Extraction stopped: {exc}\n')
    finally:
        if 'folder' in locals() and os.geteuid()==0 and os.environ.get('SUDO_UID','').isdigit():
            uid,gid=int(os.environ['SUDO_UID']),int(os.environ.get('SUDO_GID','0'))
            for file in folder.iterdir():
                file.chmod(0o600);os.chown(file,uid,gid)
            os.chown(folder,uid,gid)

if __name__=='__main__':
    main()
