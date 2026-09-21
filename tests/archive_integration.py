"""Loopback archive search and two-socket H.264/H.265 playback. No real credentials."""
import re, time, json, pathlib, socket, struct, subprocess, sys, tempfile, threading
from urllib.parse import urlencode, quote

def exact(conn, size):
    data = b''
    while len(data) < size:
        part = conn.recv(size-len(data))
        if not part: raise EOFError('Unexpected closed socket')
        data += part
    return data

def read(conn):
    h = exact(conn, 20)
    size = struct.unpack_from('<I', h, 16)[0]
    assert size < 8192
    return struct.unpack_from('<H', h, 14)[0], struct.unpack_from('<I', h, 4)[0], json.loads(exact(conn,size).rstrip(b'\0\n'))

def send(conn, command, payload):
    h = struct.pack('<BBHII BBHI',255,1,0,0x1234,0,0,0,command,len(payload))
    packet = h+payload
    for i in range(0,len(packet),537): conn.sendall(packet[i:i+537])

def reply(conn, command, **extra):
    send(conn,command,json.dumps(dict(Ret=100,**extra)).encode()+b'\n\0')

name='/disk/003/test[R][0].h264'
with tempfile.TemporaryDirectory() as temp:
    for codec,encoder,code,rate in [('h264','libx264',2,.25),('hevc','libx265',3,.5),('h264','libx264',2,1),('hevc','libx265',3,2),('h264','libx264',2,4),('hevc','libx265',3,8)]:
        raw=pathlib.Path(temp)/(codec+str(rate)+'.raw')
        subprocess.run(['ffmpeg','-v','error','-f','lavfi','-i','testsrc2=size=128x96:rate=10','-t','2','-c:v',encoder,'-threads','1','-bf','0',*(['-x265-params','aud=1:keyint=10:pools=1:frame-threads=1:log-level=error'] if code==3 else ['-x264-params','aud=1:keyint=10']),'-f',codec,str(raw)],check=True)
        payload=raw.read_bytes()
        listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(3);listener.settimeout(15)
        url=f'dvrip://admin:admin@127.0.0.1:{listener.getsockname()[1]}'
        errors=[]
        def serve():
            try:
                with listener.accept()[0] as query:
                    query.settimeout(10)
                    cmd,sid,body=read(query);assert cmd==1000 and body['PassWord']=='6QNMIQGe'
                    reply(query,1001)
                    cmd,sid,body=read(query);assert cmd==1440 and sid==0x1234
                    q=body['OPFileQuery'];assert q['Channel']==2 and q['Type']=='h264' and q['EndTime']=='2026-09-10 23:59:59'
                    reply(query,1441,OPFileQuery=[dict(FileName=name,BeginTime='2026-09-10 12:00:00',EndTime='2026-09-10 12:00:02')])
                with listener.accept()[0] as control:
                    control.settimeout(10)
                    cmd,sid,body=read(control);assert cmd==1000;reply(control,1001)
                    with listener.accept()[0] as data:
                        data.settimeout(10)
                        cmd,sid,body=read(data);assert cmd==1424 and sid==0x1234
                        p=body['OPPlayBack'];assert p['Action']=='Claim' and p['Parameter']['PlayMode']=='ByName' and p['Parameter']['FileName']==name and p['Parameter']['Channel']==2 and p['StartTime']=='2026-09-10 12:00:01'
                        reply(data,1425)
                        cmd,sid,body=read(control);assert cmd==1420 and sid==0x1234 and body['OPPlayBack']['Action']==('DownloadStart' if rate>1 else 'Start');reply(control,1421)
                        groups=[]
                        for nal in [n for n in re.split(b'\x00\x00\x00?\x01',payload) if n]:
                            kind=(nal[0]&31) if code==2 else ((nal[0]>>1)&63)
                            if kind==(9 if code==2 else 35) or not groups: groups.append([])
                            groups[-1].append(nal)
                        for index,group in enumerate(groups):
                            body=b''.join(b'\0\0\0\1'+n for n in group)
                            key=index%10==0
                            stamp=(26<<26)|(9<<22)|(11<<17)|(12<<12)|(22<<6)|(30+index//10)
                            frame=(b'\0\0\1\xfc'+bytes([code,10,16,12])+struct.pack('<II',stamp,len(body)) if key else b'\0\0\1\xfd'+struct.pack('<I',len(body)))+body
                            for offset in range(0,len(frame),537): send(data,1422,frame[offset:offset+537])
                        send(data,1422,b'')
                        cmd,sid,body=read(control);assert cmd==1420 and body['OPPlayBack']['Action']==('DownloadStop' if rate>1 else 'Stop')
            except Exception as e: errors.append(repr(e))
        thread=threading.Thread(target=serve);thread.start()
        subprocess.run([sys.argv[1],url,'--archive-query'],check=True,timeout=15)
        query=urlencode(dict(speed=rate,channel=2,archiveFile=name,begin='2026-09-10 12:00:01',end='2026-09-10 12:00:02'),quote_via=quote)
        started=time.monotonic()
        subprocess.run([sys.argv[1],url+'?'+query,'--archive-play'],check=True,timeout=15)
        elapsed=time.monotonic()-started
        assert elapsed>3 if rate<1 else elapsed<4, elapsed
        thread.join(12);listener.close();assert not thread.is_alive() and not errors,errors
print('Archive search, shared-session claim/start, fragmented H.264/H.265 decoding, EOF and Stop passed.')
