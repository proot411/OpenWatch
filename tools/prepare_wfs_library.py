#!/usr/bin/env python3
"""Convert copied WFS test clips into a seekable, video-only experimental library.
No raw disk access. Requires ffmpeg/ffprobe. New output directory only.
Timing: interpolate between validated Sofia keyframe wall-clock timestamps;
extrapolate the last GOP using its advertised frame rate. No B-frame support.
FFmpeg setts reference: https://ffmpeg.org/ffmpeg-bitstream-filters.html#setts
"""
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
from analyze_wfs_index import timestamp


def parse(data):
    pos = 0
    payloads, anchors = [], []
    codec = None
    counts = {}
    while pos < len(data):
        if data[pos] == 255 and all(x == 255 for x in data[pos:]):
            break
        if pos + 8 > len(data) or data[pos:pos+3] != b'\0\0\1':
            raise ValueError(f'Invalid frame marker at {pos:#x}')
        kind = data[pos+3]
        if kind not in (252, 254, 253, 250, 249):
            raise ValueError('Unknown frame type')
        header = 16 if kind in (252, 254) else 8
        if pos+header > len(data): raise ValueError('Short header')
        length = (struct.unpack_from('<I', data, pos+(12 if header==16 else 4))[0]
                  if kind in (252,254,253) else struct.unpack_from('<H', data, pos+6)[0])
        if length > 8388608 or pos+header+length > len(data):
            raise ValueError('Truncated or oversized frame')
        if header == 16:
            next_codec, fps = data[pos+4]&15, data[pos+5]&31
            stamp = timestamp(struct.unpack_from('<I', data, pos+8)[0])
            if next_codec not in (2,3) or (codec and codec != next_codec) or not fps or not stamp:
                raise ValueError('Unsupported codec, frame rate or timestamp')
            codec = next_codec
            when = datetime.fromisoformat(stamp)
            if anchors and when < anchors[-1][1]: raise ValueError('Backward keyframe timestamp')
            # Multiple keyframes may have the same whole-second clock value.
            if not anchors or when > anchors[-1][1]:
                anchors.append((len(payloads), when, fps))
        if kind in (252,254,253):
            payload = data[pos+header:pos+header+length]
            if not anchors or not payload.startswith((b'\0\0\1', b'\0\0\0\1')):
                raise ValueError('Missing keyframe or non-Annex-B payload')
            payloads.append(payload)
        counts[str(kind)] = counts.get(str(kind),0)+1
        pos += header+length
    if not payloads or len(anchors)>256: raise ValueError('Missing video or excessive anchor count')
    return payloads, anchors, codec, pos, counts


def expression(anchors):
    base = anchors[0][1]
    i,t,fps = anchors[-1]
    tail = f'{(t-base).total_seconds()*1000}+(N-{i})*{1000/fps}'
    for (i,t,fps),(j,u,_) in reversed(list(zip(anchors,anchors[1:]))):
        span=(u-t).total_seconds()*1000
        if j<=i or span<=0: raise ValueError('Invalid timestamp anchors')
        tail=f'if(lt(N,{j}),{(t-base).total_seconds()*1000}+(N-{i})*{span/(j-i)},{tail})'
    return tail


def run(args):
    r=subprocess.run(args, capture_output=True, text=True, timeout=180)
    if r.returncode: raise ValueError(r.stderr[-3000:])
    return r.stdout


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('collection',type=Path)
    parser.add_argument('--output-parent',required=True,type=Path)
    a=parser.parse_args()
    root=a.collection.resolve()
    manifest=json.loads((root/'manifest.json').read_text())
    folder=Path(tempfile.mkdtemp(prefix='openwatch-test-library-',dir=a.output_parent))
    library={'version':1,'timing':'Keyframe timestamps; interpolated intermediate frames; final GOP estimated', 'clips':[]}
    for item in manifest['recordings']:
        source=(root/item['file']).resolve()
        if source.parent != root or source.stat().st_size>64*1024*1024: raise ValueError('Invalid source path/size')
        raw=source.read_bytes()
        if hashlib.sha256(raw).hexdigest()!=item['sha256']: raise ValueError('Source checksum mismatch')
        size=item['candidate_recording_bytes']
        if not 0<size<=len(raw): raise ValueError('Invalid logical length')
        frames,anchors,codec,consumed,counts=parse(raw[:size])
        stem=f"channel-{item['channel_candidate']}-{item['primary_fragment']}"
        elementary=folder/(stem+('.h264' if codec==2 else '.h265'))
        with elementary.open('xb') as f:
            for frame in frames:f.write(frame)
        probe=json.loads(run(['ffprobe','-v','error','-count_packets','-show_streams','-of','json',str(elementary)]))['streams'][0]
        decoded=json.loads(run(['ffprobe','-v','error','-show_frames','-show_entries','frame=pict_type,pkt_pos','-of','json',str(elementary)]))['frames']
        positions=[int(f.get('pkt_pos',-1)) for f in decoded]
        if (int(probe['nb_read_packets'])!=len(frames) or len(decoded)!=len(frames)
                or any(f.get('pict_type')=='B' for f in decoded)
                or any(p<0 for p in positions) or positions!=sorted(positions)):
            raise ValueError('Packet/frame mapping or B-frame stream unsupported')
        mkv=folder/(stem+'.mkv')
        bsf="setts=time_base=1/1000:ts='"+expression(anchors)+"':duration=0"
        run(['ffmpeg','-v','error','-nostdin','-n','-i',str(elementary),'-map','0:v:0','-c','copy','-bsf:v',bsf,str(mkv)])
        # Fail on decode errors; verify exact decoded frame count after remux.
        run(['ffmpeg','-v','error','-xerror','-nostdin','-i',str(mkv),'-f','null','-'])
        metadata=json.loads(run(['ffprobe','-v','error','-count_frames','-show_streams','-show_format','-of','json',str(mkv)]))
        if int(metadata['streams'][0]['nb_read_frames'])!=len(frames): raise ValueError('Decoded frame count mismatch')
        duration=float(metadata['format']['duration'])
        # Exercise demux seeking and decoding at three interior points.
        for ratio in (.25,.5,.75):
            count=run(['ffmpeg','-v','error','-xerror','-nostdin','-ss',str(duration*ratio),'-i',str(mkv),'-frames:v','1','-f','framemd5','-'])
            if not any(line and not line.startswith('#') for line in count.splitlines()): raise ValueError('Seek yielded no frame')
        clip={'file':mkv.name,'channel':item['channel_candidate'],'start':anchors[0][1].isoformat(' '),
              'index_start':item['begin'],'index_end':item['end'],'duration_ms':round(duration*1000),
              'codec':probe['codec_name'],'frames':len(frames),'width':probe['width'],'height':probe['height'],
              'parsed_bytes':consumed,'padding_bytes':size-consumed,'frame_types':counts,
              'keyframes':len(anchors),'validation':'Full decode and three interior seeks passed'}
        library['clips'].append(clip)
        print(stem,clip['codec'],len(frames),'frames',duration,'seconds; decode and seeks passed',flush=True)
    with (folder/'library.json').open('x') as f:json.dump(library,f,indent=2)
    print('LIBRARY',folder/'library.json')

if __name__=='__main__': main()
