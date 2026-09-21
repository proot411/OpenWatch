"""Synthesize unencrypted Sofia recording exports and use the native playback worker."""
import json, pathlib, re, struct, subprocess, sys, tempfile
with tempfile.TemporaryDirectory() as temp:
    root=pathlib.Path(temp)
    for codec, encoder, params, aud, code in [('h264','libx264','-x264-params',9,2),('hevc','libx265','-x265-params',35,3)]:
        raw=root/('source.'+codec)
        subprocess.run(['ffmpeg','-v','error','-f','lavfi','-i','testsrc2=size=128x96:rate=10','-t','6','-c:v',encoder,'-threads','1','-bf','0',params,'aud=1:keyint=10'+(':pools=1:frame-threads=1:log-level=error' if codec=='hevc' else ''),'-f',codec,str(raw)],check=True)
        nals=[n for n in re.split(b'\x00\x00\x00?\x01',raw.read_bytes()) if n]
        frames=[]
        for nal in nals:
            kind=(nal[0]&31) if codec=='h264' else ((nal[0]>>1)&63)
            if kind==aud or not frames: frames.append([])
            frames[-1].append(nal)
        recording=root/('xm-'+codec+'.h264')
        with recording.open('wb') as out:
            for group in frames:
                payload=b''.join(b'\0\0\0\1'+n for n in group)
                key=any(((n[0]&31)==5 if codec=='h264' else 16<=((n[0]>>1)&63)<=21) for n in group)
                if key: out.write(b'\0\0\1\xfc'+bytes([code,10,16,12])+b'\0'*4+struct.pack('<I',len(payload)))
                else: out.write(b'\0\0\1\xfd'+struct.pack('<I',len(payload)))
                out.write(payload)
                out.write(b'\0\0\1\xfa\x0e\x02\x02\0\x11\x22')
        exported=root/(codec+'.mkv')
        subprocess.run([sys.argv[1],recording.as_uri(),str(exported)],check=True)
        info=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_streams','-show_format','-of','json',str(exported)]))
        assert info['streams'][0]['codec_name']==codec
        assert 2<float(info['format']['duration'])<6
        subprocess.run(['ffmpeg','-v','error','-i',str(exported),'-f','null','-'],check=True)
print('Sofia H.264/H.265 file playback and MKV recording passed.')
