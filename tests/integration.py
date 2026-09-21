"""Exercise native decode/remux and fragmented DVRIP against a loopback fixture."""
import os, base64, json, pathlib, re, socket, struct, subprocess, sys, tempfile, threading, time

def exact(conn, count):
    result = b''
    while len(result) < count:
        data = conn.recv(count - len(result))
        if not data: raise EOFError()
        result += data
    return result

def read(conn):
    header = exact(conn, 20)
    size = struct.unpack_from('<I', header, 16)[0]
    assert size < 8192
    return struct.unpack_from('<H', header, 14)[0], json.loads(exact(conn, size).rstrip(b'\0\n'))

def send(conn, command, payload):
    packet = struct.pack('<BBHII BBHI', 255, 1, 0, 0x1234, 0, 0, 0, command, len(payload)) + payload
    conn.sendall(packet[:7]); conn.sendall(packet[7:23]); conn.sendall(packet[23:])

def probe(path):
    info = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-show_streams', '-show_format', '-of', 'json', str(path)]))
    assert info['streams'][0]['codec_name'] == 'h264'
    assert float(info['format']['duration']) > 0
    subprocess.run(['ffmpeg', '-v', 'error', '-i', str(path), '-f', 'null', '-'], check=True)

with tempfile.TemporaryDirectory() as root:
    root = pathlib.Path(root)
    source = root / 'source.mp4'
    subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i', 'testsrc2=size=320x180:rate=25', '-t', '8', '-c:v', 'libx264', '-bf', '0', '-g', '25', '-x264-params', 'aud=1', '-pix_fmt', 'yuv420p', str(source)], check=True)
    subprocess.run([sys.argv[1], source.as_uri(), str(root / 'local.mkv')], check=True)
    probe(root / 'local.mkv')
    raw = root / 'stream.h264'
    subprocess.run(['ffmpeg', '-v', 'error', '-i', str(source), '-t', '1', '-c', 'copy', '-bsf:v', 'h264_mp4toannexb', '-f', 'h264', str(raw)], check=True)
    payload = raw.read_bytes()
    listener = socket.socket(); listener.bind(('127.0.0.1', 0)); listener.listen(1); listener.settimeout(15)
    errors = []
    def serve():
        try:
            conn, _ = listener.accept()
            with conn:
                conn.settimeout(10)
                cmd, body = read(conn); assert cmd == 1000 and body['PassWord'] == '6QNMIQGe'
                send(conn, 1001, b'{"Ret":100,"SessionID":"0x00001234"}\n\0')
                cmd, body = read(conn); assert cmd == 1413 and body['OPMonitor']['Action'] == 'Claim'
                send(conn, 1414, b'{"Ret":100}\n\0')
                cmd, body = read(conn); assert cmd == 1410 and body['OPMonitor']['Parameter']['Channel'] == 0
                send(conn, 1411, b'{"Ret":100}\n\0')
                frame = b'\x00\x00\x01\xfc\x02\x19\x28\x16' + b'\0' * 4 + struct.pack('<I', len(payload)) + payload
                for _ in range(150):
                    for offset in range(0, len(frame), 4096): send(conn, 1412, frame[offset:offset+4096])
                    time.sleep(.08)
        except (BrokenPipeError, ConnectionResetError): pass
        except Exception as error: errors.append(error)
    thread = threading.Thread(target=serve, daemon=True); thread.start()
    subprocess.run([sys.argv[1], f'dvrip://admin:admin@127.0.0.1:{listener.getsockname()[1]}?channel=0', str(root / 'dvrip.mkv')], check=True)
    thread.join(timeout=12); listener.close()
    assert not errors, errors
    probe(root / 'dvrip.mkv')
    # Minimal interleaved-RTP RTSP camera fixture, including FU-A fragmentation.
    nals = [n for n in re.split(b'\x00\x00\x00?\x01', payload) if n]
    sps = next(n for n in nals if n[0] & 31 == 7)
    pps = next(n for n in nals if n[0] & 31 == 8)
    groups = []
    for nal in nals:
        if nal[0] & 31 == 9 or not groups: groups.append([])
        groups[-1].append(nal)
    rtsp = socket.socket(); rtsp.bind(('127.0.0.1', 0)); rtsp.listen(1); rtsp.settimeout(15)
    errors.clear()
    def serve_rtsp():
        try:
            conn, _ = rtsp.accept()
            with conn:
                conn.settimeout(10)
                stream = conn.makefile('rb')
                while True:
                    request = stream.readline().decode().strip()
                    if not request: return
                    method = request.split()[0]; headers = {}
                    while True:
                        line = stream.readline().decode().strip()
                        if not line: break
                        key, value = line.split(':', 1); headers[key.lower()] = value.strip()
                    if int(headers.get('content-length', '0')): stream.read(int(headers['content-length']))
                    body = b''; extra = ''
                    if method == 'DESCRIBE':
                        body = ('v=0\r\no=- 0 0 IN IP4 127.0.0.1\r\ns=Fixture\r\nc=IN IP4 127.0.0.1\r\nt=0 0\r\nm=video 0 RTP/AVP 96\r\na=rtpmap:96 H264/90000\r\na=fmtp:96 packetization-mode=1;sprop-parameter-sets=' + base64.b64encode(sps).decode() + ',' + base64.b64encode(pps).decode() + '\r\na=control:track1\r\nm=audio 0 RTP/AVP 0\r\na=rtpmap:0 PCMU/8000/1\r\na=control:track2\r\n').encode()
                        extra = 'Content-Type: application/sdp\r\n'
                    elif method == 'SETUP': extra = 'Transport: ' + headers['transport'] + '\r\n'
                    elif method == 'OPTIONS': extra = 'Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN\r\n'
                    response = f"RTSP/1.0 200 OK\r\nCSeq: {headers['cseq']}\r\nSession: 12345678\r\n{extra}Content-Length: {len(body)}\r\n\r\n".encode() + body
                    conn.sendall(response)
                    if method == 'PLAY': break
                sequence = 0
                for index in range(200):
                    group = groups[index % len(groups)]
                    for ni, nal in enumerate(group):
                        if len(nal) <= 1200: fragments = [nal]
                        else:
                            chunks = [nal[i:i+1198] for i in range(1, len(nal), 1198)]
                            fragments = [bytes([(nal[0] & 224) | 28, (nal[0] & 31) | (128 if j == 0 else 0) | (64 if j == len(chunks)-1 else 0)]) + chunk for j, chunk in enumerate(chunks)]
                        for fi, fragment in enumerate(fragments):
                            last = ni == len(group)-1 and fi == len(fragments)-1
                            packet = struct.pack('!BBHII', 128, 96 | (128 if last else 0), sequence % 65536, index * 3600, 123) + fragment
                            conn.sendall(b'$\0' + struct.pack('!H', len(packet)) + packet); sequence += 1
                    audio = struct.pack('!BBHII', 128, 0, index % 65536, index * 320, 456) + bytes([0x80, 0x00]) * 160
                    conn.sendall(b'$\x02' + struct.pack('!H', len(audio)) + audio)
                    time.sleep(.04)
        except (BrokenPipeError, ConnectionResetError): pass
        except Exception as error: errors.append(error)
    thread = threading.Thread(target=serve_rtsp, daemon=True); thread.start()
    subprocess.run([sys.argv[1], f'rtsp://127.0.0.1:{rtsp.getsockname()[1]}/live', str(root / 'rtsp.mkv'), '--audio'], check=True, env={**os.environ, 'SDL_AUDIODRIVER': 'dummy'})
    thread.join(timeout=12); rtsp.close()
    assert not errors, errors
    probe(root / 'rtsp.mkv')
    # AAC decoding from a video+audio container through the same native worker.
    avfile=root / 'aac-source.mkv'
    subprocess.run(['ffmpeg','-v','error','-f','lavfi','-i','testsrc2=size=160x90:rate=10','-f','lavfi','-i','sine=frequency=440:sample_rate=44100','-t','4','-c:v','libx264','-preset','ultrafast','-c:a','aac',str(avfile)],check=True)
    subprocess.run([sys.argv[1],str(avfile),'%s' % (root / 'aac-record.mkv'),'--audio'],check=True,env={**os.environ,'SDL_AUDIODRIVER':'dummy'})
    # Recorder scan followed by four concurrent independently addressed channels.
    nvr = socket.socket(); nvr.bind(('127.0.0.1', 0)); nvr.listen(8); nvr.settimeout(15)
    seen = []; errors.clear(); workers = []
    def nvr_stream(conn, scan_only):
        try:
            with conn:
                conn.settimeout(10)
                cmd, body = read(conn); assert cmd == 1000 and body['PassWord'] == '6QNMIQGe'
                send(conn, 1001, b'{"Ret":100,"ChannelNum":4}\n\0')
                if scan_only: return
                cmd, body = read(conn); assert cmd == 1413
                params = body['OPMonitor']['Parameter']; channel = params['Channel']
                assert params['StreamType'] == 'Extra1'; seen.append(channel)
                send(conn, 1414, b'{"Ret":100}\n\0')
                cmd, body = read(conn); assert cmd == 1410 and body['OPMonitor']['Parameter']['Channel'] == channel
                send(conn, 1411, b'{"Ret":100}\n\0')
                frame = b'\x00\x00\x01\xfc\x02\x19\x28\x16' + b'\0'*4 + struct.pack('<I', len(payload)) + payload
                for _ in range(150):
                    for offset in range(0, len(frame), 4096): send(conn, 1412, frame[offset:offset+4096])
                    time.sleep(.08)
        except (BrokenPipeError, ConnectionResetError): pass
        except Exception as error: errors.append(error)
    def serve_nvr():
        try:
            for i in range(5):
                conn, _ = nvr.accept(); worker = threading.Thread(target=nvr_stream, args=(conn, i == 0), daemon=True); workers.append(worker); worker.start()
        except Exception as error: errors.append(error)
    server = threading.Thread(target=serve_nvr, daemon=True); server.start()
    subprocess.run([sys.argv[1], f'dvrip://admin:admin@127.0.0.1:{nvr.getsockname()[1]}', '--recorder'], check=True)
    server.join(timeout=15)
    for worker in workers: worker.join(timeout=12)
    nvr.close(); assert not errors, errors; assert sorted(seen) == [0, 1, 2, 3], seen
    # Interrupt a real decoded/recorded stream, then accept its reconnect.
    recovery = socket.socket(); recovery.bind(('127.0.0.1', 0)); recovery.listen(4); recovery.settimeout(15)
    errors.clear(); connections = []
    def recovery_server():
        try:
            for attempt in range(2):
                conn, _ = recovery.accept(); connections.append(attempt)
                with conn:
                    conn.settimeout(8)
                    cmd, body = read(conn); assert cmd == 1000
                    send(conn, 1001, b'{"Ret":100}\n\0')
                    cmd, body = read(conn); assert cmd == 1413
                    send(conn, 1414, b'{"Ret":100}\n\0')
                    cmd, body = read(conn); assert cmd == 1410
                    send(conn, 1411, b'{"Ret":100}\n\0')
                    frame = b'\x00\x00\x01\xfc\x02\x19\x28\x16' + b'\0'*4 + struct.pack('<I', len(payload)) + payload
                    try:
                        for _ in range(40 if attempt == 0 else 150):
                            for offset in range(0, len(frame), 4096): send(conn, 1412, frame[offset:offset+4096])
                            time.sleep(.05)
                    except (BrokenPipeError, ConnectionResetError): pass
            recovery.settimeout(2)
            try:
                conn, _ = recovery.accept(); conn.close(); raise AssertionError('Reconnected after manual stop')
            except socket.timeout: pass
        except Exception as error: errors.append(error)
    server = threading.Thread(target=recovery_server, daemon=True); server.start()
    original = root / 'recovery.mkv'
    subprocess.run([sys.argv[1], f'dvrip://admin:admin@127.0.0.1:{recovery.getsockname()[1]}', '--reconnect', str(original)], check=True)
    server.join(timeout=15); recovery.close(); assert not server.is_alive(); assert not errors, errors; assert connections == [0, 1]
    segments = list(root.glob('recovery-reconnect-*.mkv')); assert len(segments) == 1, segments
    probe(original); probe(segments[0])
print('Video tests, four-channel autoplay, reconnect, recording segments and manual stop passed.')
