# Protocol notes

The protocol implementation is independently written around the publicly documented wire behavior visible in [go2rtc's DVRIP client](https://github.com/AlexxIT/go2rtc/blob/master/pkg/dvrip/client.go), [its video producer](https://github.com/AlexxIT/go2rtc/blob/master/pkg/dvrip/producer.go), and the [Python DVRIP project](https://github.com/alexshpilkin/dvrip). These projects are useful independent interoperability references. No upstream implementation files are vendored.

TCP defaults to port 34567. Messages have a 20-byte little-endian header: marker 0xff, version/reserved bytes, session at offset 4, sequence at 8, fragmentation fields at 12/13, command at 14, payload length at 16. JSON requests end with LF and NUL. Login uses command 1000, monitor claim 1413, monitor start 1410 and keepalive 1006. The eight-character Sofia digest derives from adjacent MD5-byte sums modulo the 62-character alphanumeric alphabet. It is a legacy authentication encoding, not modern password protection.

Media framing recognizes 00 00 01 followed by FC/FE (16-byte video header), FD (8-byte predictive video header), FA/F9 (8-byte audio/other header). Audio is skipped. Fragmented media and multiple media packets per chunk are buffered. Payload lengths are capped at 8 MiB and aggregate media buffering at 16 MiB. Video codec codes 02/12 select H.264; 03/13/43/53 select H.265. Unsupported types fail closed with a status message. DVRIP video timestamps are synthesized using the advertised frame rate, not a synchronized wall clock.

Only the common single-socket monitor flow is implemented. Multi-socket firmware variants, JSON fragmentation across application messages, session/response sequence correlation, unusual authentication challenges and all proprietary encrypted media variants remain unsupported. Stream-time JSON messages are currently skipped; richer event and error dispatch is future work. The tested 'protocol version' is the synthetic fixture matching this documented framing; no actual firmware version has been verified.

P2P is not implemented. ONVIF discovery and live stream resolution were added in v0.4. Authorized future P2P research should use owned devices, captured official-client traffic and a controlled test account to document rendezvous, authentication, NAT traversal and transport encryption. Do not infer broad cloud compatibility from one firmware capture. V1 currently supports local/VPN/user-managed forwarded addresses only.

## Local recorder enumeration (v0.2)

Read `ChannelNum` from the authenticated login response. If absent/zero, query command 1020 (`SystemInfo`) and use the larger of `VideoInChannel` and `DigChannel`. Counts must be integral and within 0–256; missing counts fail explicitly rather than guessing. Digital/analog count semantics vary on hybrid firmware, so fallback accuracy still needs real-device testing. The fallback command and fields follow [the DVRIP system-info model](https://github.com/alexshpilkin/dvrip/blob/master/dvrip/info.py). Each generated stream uses a separate authenticated monitor connection with channels 0 through count−1. UI labels start at CH 1. Only the first 64 channels are opened if the recorder advertises more; the interface reports this limit.

The v0.2 recorder dialog enumerates channels from a supplied IP. The separate v0.4 discovery dialog adds LAN discovery. Neither resolves cloud serial numbers or verifies every channel’s online state. Substream failure is reported per cell; select Main stream and retry if necessary.

## Header compatibility fix (v0.2.1)

Inbound headers now accept versions 0 and 1. Earlier builds incorrectly treated the version byte as a reserved zero and rejected valid version-1 replies before parsing login results. The [Python DVRIP packet implementation](https://github.com/alexshpilkin/dvrip/blob/master/dvrip/packet.py) uses version 1; [go2rtc](https://github.com/AlexxIT/go2rtc/blob/master/pkg/dvrip/client.go) sends version 0. Outgoing requests retain version 0. The 20-byte header, 0xFF magic and bounded payload-length checks remain enforced. Unknown versions and non-DVRIP responses now have distinct, credential-free error messages.

## XM and ONVIF discovery (v0.4)

XM sends the empty command 1530 broadcast to UDP 34569 and parses 1531 `NetWork.NetCommon` replies, matching the public [OpenIPC discovery implementation](https://github.com/OpenIPC/dms/blob/master/dms.py). The advertised TCP port is used for authenticated channel enumeration. ONVIF sends WS-Discovery probes to 239.255.255.250:3702 on active IPv4 interfaces. Responses must reference an outstanding probe ID and advertise an HTTP(S) endpoint matching their sender. Results are deduplicated per protocol and endpoint, with a maximum of 512 services.

ONVIF uses SOAP 1.2 GetSystemDateAndTime, GetServices (GetCapabilities fallback), Media/Media2 GetProfiles and GetStreamUri. UsernameToken digests use a fresh nonce, adjusted UTC timestamp and SHA-1 as defined by WS-Security; HTTP authentication is handled by Qt. TLS validation remains enabled. XML is size/depth bounded and DTD/entity declarations are rejected. HTTP requests have an eight-second deadline and support cancellation.

References: [ONVIF programmer guide](https://www.onvif.org/wp-content/uploads/2016/12/ONVIF_WG-APG-Application_Programmers_Guide-1.pdf), [Media2 WSDL](https://www.onvif.org/ver20/media/wsdl/media.wsdl). No complete Profile S/T certification is claimed; Audio, events and broad configuration remain outside this implementation; experimental PTZ was added in v0.7.

## Unencrypted Sofia file reader (v0.6)

A sequential local reader handles `00 00 01 FC/FE` keyframes, `FD` delta frames and `FA/F9` audio/info blocks. Keyframes supply codec (low nibble, 2=H.264, 3=H.265) and frame rate (low five bits). Payloads are length-delimited and limited to 8 MiB. Video must begin with an Annex-B start code. The video elementary stream enters FFmpeg through custom AVIO; timestamps are synthesized, not reconstructed from recorder wall-clock fields. Codec changes, unknown blocks and truncation stop playback.

The container description is corroborated by this [independent DVRIP protocol analysis](https://github.com/Moxnatiy/hass-xmeye/blob/main/docs/protocol.md). Only the documented basic frame subset is implemented. Encrypted exports and other index/header layouts are unsupported; the reader must not be described as an XMEye decryptor.

## v0.7 camera control references and limits

XM command structures are based on [OpenIPC/python-dvr](https://github.com/OpenIPC/python-dvr/blob/master/dvrip.py); movement/stop preset conventions vary by firmware. Commands use a separate authenticated local session. No network settings or recorder account changes are sent. ONVIF operations follow the [PTZ WSDL](https://www.onvif.org/ver20/ptz/wsdl/) and [Device WSDL](https://www.onvif.org/ver10/device/wsdl/). This implementation requires advertised standard continuous velocity spaces, does not provide ONVIF certification, and does not implement tours, talk or focus/iris via ONVIF.

XM start/stop values (65535/-1) are also illustrated in this [captured PTZ exchange](https://gist.github.com/knight-of-ni/26be98747cef99cd4fe7).

## v0.7.1 Media2 profile fix

Media2 GetProfiles now supplies Type=All as required to return configuration details by the [ONVIF Media2 specification](https://www.onvif.org/specs/2512/ONVIF-Media2-Service-Spec-v2512.pdf). Read-only field checks reproduced omitted PTZ configuration without Type and returned PTZCFG_000 with Type=All. Both user cameras responded on port 8899 and advertised PTZTimeout PT1S–PT100S. No physical movement was sent. Timeout requests respect this range while an explicit Stop follows each short pulse.

## XMEye P2P investigation

See [XMEye cloud connection investigation](xmeye-p2p.md) for the RPS target, public SDK/API findings, missing handshake evidence and a focused official-client capture procedure. Cloud transport remains unimplemented; the working local/VPN path is unchanged.

## NVR archive playback (experimental)

Search uses OPFileQuery (1440), zero-based Channel, the recorder-local BeginTime/EndTime, Event `*`, DriverTypeMask `0x0000FFFF`, StreamType `0x00000000`, Type `h264` (also used for H.265). The browser requests one day and exposes manual pagination, overlapping the latest returned BeginTime and deduplicating by FileName. It stops with an incomplete-list notice if a full page makes no forward progress or the list reaches its limit. This avoids silently skipping records with equal timestamps, but firmware with different ordering or paging semantics may need adaptation.

Playback logs in on a control socket, shares that session with a separate data socket, claims OPPlayBack (1424) on data, and starts OPPlayBack (1420) on control. ByName uses the exact returned FileName and StartTime/EndTime. Media is routed by message ID (1422/1426), not payload prefixes; video is assembled with the existing bounded Sofia parser and decoded by FFmpeg. A zero-length media message is treated as EOF; other disconnects/timeouts are reported without replaying the file. Stop is sent best-effort when tearing down the session. KeepAlive uses the control socket. No seek or speed commands are sent, and recording files on the NVR are never modified.

References: [OpenIPC python-dvr](https://github.com/OpenIPC/python-dvr/blob/master/dvrip.py) and [measured NBD8008R-U protocol](https://github.com/Moxnatiy/hass-xmeye/blob/main/docs/protocol.md). The latter is a different recorder/firmware from the user's NBD88X16S-KL-V3. Its behavior is implementation guidance, not proof of compatibility with every XM recorder.

### v0.10 playback navigation

The calendar browser automatically pages each selected channel, with cancellation and a 128-page per-channel cap. Gaps and partial coverage remain visible. Up to four archive workers share a requested recorder-local time. Seeking restarts ByName playback using that time as StartTime, including percent-decoded timestamps. Pause stops streams, preserving the last rendered images; resume starts new sessions at the playhead. Decoded frame count and the declared frame rate estimate the playhead; this is approximate, especially across buffering or firmware that ignores StartTime. Adjacent recordings are opened as the playhead crosses file boundaries. No native speed/seek command or frame-level synchronization is claimed.

### v0.11 timestamps and playback rates

Sofia keyframe offset 8 contains packed recorder-local date/time (seconds:6, minutes:6, hours:5, day:5, month:4, year since 2000:6). Calendar fields are validated. UTC is used only as a timezone-neutral numeric representation; no host timezone conversion is applied. Delta-frame times interpolate from each keyframe using the reported frame rate. Byte offsets associate frame metadata with FFmpeg demux packets; packet PTS carries the association through decoding. Timestamp and rendered image are published together, avoiding the use of the newest buffered timestamp for older displayed frames. Missing timestamps are unavailable, not replaced by requested seek time. File end metadata is used only to choose the next file, not to update the displayed clock.

Rates above 1 request DownloadStart with matching DownloadStop. Lower rates retain Start/Stop. Client-side pacing supports .25/.5/1/2/4/8 and remains cancellable, maintaining control keepalives during slow playback. Discontinuous timestamps re-anchor presentation without waiting through large recorded gaps. Download mode is firmware-dependent and throughput limits achievable fast playback; no native Fast/Slow command success is claimed. No footage is saved as part of download-style playback.

### v0.11.1 empty recording searches

Ret 119 is accepted only for OPFileQuery replies (1441) and means no matches for that request, not a permission failure. An empty result retries the same channel, date range, Event and Type while omitting optional DriverTypeMask and StreamType, matching the minimal request used by alexshpilkin/dvrip (`dvrip/files.py`). The retry never substitutes a different channel or date. Permission errors and login failures remain errors. Per-channel empty results are displayed even when another selected channel has recordings. A persistent empty response despite confirmed footage requires further device-specific investigation; this fallback does not prove physical compatibility.

### v0.11.2 narrow-window search probe

If both whole-range searches return no files, a request longer than one hour (up to one day) is retried in overlapping one-hour windows on the same channel. Each window uses the standard and minimal forms. Results are deduplicated by filename; scanning stops on a full result page so the existing browser pagination can continue. Cancellation is checked between windows and control keepalives are maintained. This is a compatibility probe prompted by confirmed channel-2 footage from September 11 noon to September 12 11am; narrow-window requirements are not yet verified on the physical recorder. No channel-number substitution or device configuration change is made.
