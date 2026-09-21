#!/usr/bin/env python3
"""Read-only local/VPN DVRIP archive compatibility check. No cloud access.
Credentials are prompted, not stored. Video samples exist only in memory.
"""
import datetime as dt
import getpass
import hashlib
import json
import shutil
import socket
import struct
import subprocess
import time

LIMIT = 8 * 1024 * 1024

class DVRIP:
    def __init__(self, host, port, session=0):
        self.sock = socket.create_connection((host, port), timeout=5)
        self.sock.settimeout(5)
        self.session, self.sequence = session, 0

    def exact(self, size):
        out = bytearray()
        while len(out) < size:
            part = self.sock.recv(size - len(out))
            if not part:
                raise RuntimeError('Recorder disconnected')
            out.extend(part)
        return bytes(out)

    def packet(self):
        header = self.exact(20)
        magic, version, _, session, _, _, _, kind, size = struct.unpack('<BBHIIBBHI', header)
        if magic != 255 or version not in (0, 1) or size > LIMIT:
            raise RuntimeError('Unsupported or oversized DVRIP packet')
        self.session = session
        return kind, self.exact(size)

    def send(self, kind, body):
        payload = json.dumps(body, separators=(',', ':')).encode() + b'\n\0'
        header = struct.pack('<BBHIIBBHI', 255, 0, 0, self.session, self.sequence, 0, 0, kind, len(payload))
        self.sequence += 1
        self.sock.sendall(header + payload)

    def reply(self, expected):
        kind, payload = self.packet()
        if kind != expected:
            raise RuntimeError(f'Unexpected response message {kind}; expected {expected}')
        result = json.loads(payload.rstrip(b'\0\r\n'))
        if not isinstance(result, dict) or result.get('Ret') not in (100, 515):
            code = result.get('Ret') if isinstance(result, dict) else 'invalid response'
            raise RuntimeError(f'Recorder rejected request (Ret={code})')
        return result

    def command(self, kind, name, params=None):
        body = {'Name': name, 'SessionID': f'0x{self.session:08x}'}
        if params is not None:
            body[name] = params
        self.send(kind, body)
        return self.reply(kind + 1)

    def login(self, user, password):
        digest = hashlib.md5(password.encode()).digest()
        alphabet = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'
        hashed = ''.join(alphabet[(digest[i] + digest[i + 1]) % 62] for i in range(0, 16, 2))
        self.send(1000, {'EncryptType': 'MD5', 'LoginType': 'DVRIP-Web', 'UserName': user, 'PassWord': hashed})
        return self.reply(1001)

    def close(self):
        self.sock.close()

class Frames:
    def __init__(self):
        self.pending = bytearray()
        self.codec = None
        self.video = bytearray()
        self.count = 0

    def append(self, data):
        if len(self.pending) + len(data) > 2 * LIMIT:
            raise RuntimeError('Archive buffer exceeded limit')
        self.pending.extend(data)
        while len(self.pending) >= 8:
            if self.pending[:3] != b'\0\0\1':
                raise RuntimeError('Unrecognized archive framing; raw sample was not saved')
            kind = self.pending[3]
            if kind in (0xfc, 0xfe):
                if len(self.pending) < 16:
                    return
                prefix, size = 16, struct.unpack_from('<I', self.pending, 12)[0]
            elif kind == 0xfd:
                prefix, size = 8, struct.unpack_from('<I', self.pending, 4)[0]
            elif kind in (0xfa, 0xf9):
                prefix, size = 8, struct.unpack_from('<H', self.pending, 6)[0]
            else:
                raise RuntimeError('Unsupported archive block')
            if size > LIMIT:
                raise RuntimeError('Archive frame exceeded limit')
            if len(self.pending) < prefix + size:
                return
            if prefix == 16:
                codec = self.pending[4] & 15
                if codec not in (2, 3) or (self.codec is not None and self.codec != codec):
                    raise RuntimeError('Unsupported or changing archive codec')
                self.codec = codec
            payload = self.pending[prefix:prefix + size]
            del self.pending[:prefix + size]
            if kind in (0xfc, 0xfe, 0xfd) and self.codec is not None:
                if len(self.video) + len(payload) > LIMIT:
                    raise RuntimeError('Video sample exceeded limit')
                self.video.extend(payload)
                self.count += 1


def probe(host, port, user, password, channel, date, ffmpeg):
    control = DVRIP(host, port)
    data = None
    params = None
    started = False
    try:
        login = control.login(user, password)
        print('Login accepted.')
        if login.get('DataUseAES'):
            raise RuntimeError('Recorder reports encrypted transport; this probe does not implement it')
        found = control.command(1440, 'OPFileQuery', {
            'BeginTime': date + ' 00:00:00', 'EndTime': date + ' 23:59:59',
            'Channel': channel, 'DriverTypeMask': '0x0000FFFF', 'Event': '*',
            'StreamType': '0x00000000', 'Type': 'h264'})
        files = found.get('OPFileQuery')
        if not isinstance(files, list):
            raise RuntimeError('Recording search returned an unsupported response layout')
        choices = [f for f in files if isinstance(f, dict) and isinstance(f.get('FileName'), str) and f['FileName']]
        if not choices:
            print('Search accepted, but no recordings returned for this date/channel. Try a known recorded day.')
            return False
        print(f'Search accepted: {len(choices)} entries on the first page (not a full archive count).')
        # ByName preserves the requested channel on firmware that ignores Channel in ByTime.
        entry = choices[0]
        params = {'Parameter': {'FileName': entry['FileName'], 'PlayMode': 'ByName', 'TransMode': 'TCP', 'StreamType': 0, 'Value': 0}}
        data = DVRIP(host, port, control.session)
        data.command(1424, 'OPPlayBack', dict(params, Action='Claim'))
        started = True
        control.command(1420, 'OPPlayBack', dict(params, Action='DownloadStart'))
        frames = Frames()
        deadline = time.monotonic() + 10
        while frames.count < 10 and time.monotonic() < deadline:
            kind, payload = data.packet()
            if kind in (1422, 1426):
                frames.append(payload)
            else:
                raise RuntimeError(f'Unexpected archive data message {kind}')
        if not frames.video:
            raise RuntimeError('Playback was accepted but no video keyframe arrived')
        codec = 'h264' if frames.codec == 2 else 'hevc'
        result = subprocess.run([ffmpeg, '-v', 'error', '-f', codec, '-i', 'pipe:0', '-frames:v', '1', '-f', 'framemd5', 'pipe:1'], input=frames.video, capture_output=True, timeout=15)
        decoded = [line for line in result.stdout.splitlines() if line and not line.startswith(b'#')]
        if result.returncode or not decoded:
            raise RuntimeError('Archive data arrived, but FFmpeg could not decode a frame. No sample was saved.')
        print(f'PASS: searched recordings, received {frames.count} video frames, and decoded a {"H.264" if frames.codec == 2 else "H.265"} frame.')
        print('This confirms basic archive access for the tested channel, not seeking, audio or full firmware compatibility.')
        return True
    finally:
        if started and params:
            try:
                control.command(1420, 'OPPlayBack', dict(params, Action='DownloadStop'))
            except Exception:
                print('Playback stop was not acknowledged; closing both connections.')
        if data:
            data.close()
        control.close()


def main():
    print('OpenWatch NVR playback check — local/VPN only; no settings or recordings are changed.')
    ffmpeg = shutil.which('ffmpeg')
    if not ffmpeg:
        raise RuntimeError('FFmpeg is required for the decoding check')
    host = input('NVR private IP reachable through NetBird: ').strip()
    port = int(input('DVRIP port [34567]: ').strip() or '34567')
    user = input('NVR username: ').strip()
    password = getpass.getpass('NVR password (hidden, not saved): ')
    channel = int(input('Channel number shown on the NVR [1]: ').strip() or '1') - 1
    date = input('Known recorded date, using the NVR clock (YYYY-MM-DD): ').strip()
    dt.datetime.strptime(date, '%Y-%m-%d')
    if not host or not user or not 0 <= channel <= 255 or not 1 <= port <= 65535:
        raise ValueError('Invalid connection or channel')
    return 0 if probe(host, port, user, password, channel, date, ffmpeg) else 2

if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (Exception, KeyboardInterrupt) as exc:
        print('Check incomplete:', str(exc) if str(exc) else 'Cancelled')
        raise SystemExit(1)
