# Validation report

Environment: Linux Mint 22.2 x64 (Ubuntu 24.04 base), GCC 13.3, Qt 6.4.2, FFmpeg 6.1.1. Release build completed. Qt offscreen startup completed, and the actual application screenshot was rendered and visually inspected.

## Automated checks

The Qt protocol suite covers known Sofia password digest vectors, exact header encoding, truncated/oversized headers, every split point of a video frame, coalesced frames, audio skipping and oversized media rejection.

The integration suite generates an H.264 fixture and runs the actual native video worker against local media, a loopback DVRIP camera server, and an RTSP server emitting interleaved RTP with FU-A fragmentation. It requires a decoded frame, a nonempty MKV, ffprobe codec/duration checks, and an FFmpeg decode of each resulting recording. The DVRIP server asserts login credentials and monitor commands and intentionally splits network writes and media payloads. These fixtures test only the implemented protocol subset, not hardware compatibility.

## Limits

Physical XM playback was not exercised by the automated suite. A v0.4 live discovery smoke check found one responding ONVIF camera; no camera credentials were used. Hardware device initialization falls back to CPU in this environment; no GPU acceleration performance claim is made. H.265, HTTP/HLS/RTMP, 64 active streams, firmware quirks, credential edge cases, Windows execution and package installation on clean systems remain untested. The GitHub Actions workflow has not been run remotely. The Linux DEB is generated but not installed on the host.

Do not use this milestone as the sole recorder for security-critical deployments. Scheduled recording, durable indexes, retention, alerts and access controls are not implemented.

## v0.2 local recorder checks

Additional tests cover login-based channel counts, SystemInfo fallback for digital-only NVRs, decimal/hex counts, rejected malformed/oversized counts, generated channel URLs, retained credentials/port, main/substream selection, scan-dialog acceptance, serial-number rejection and cancellation. The loopback integration test scans a four-channel NVR, opens four simultaneous monitor connections, verifies channel IDs 0–3 and substream selection, and requires a decoded frame from each channel. Real-recorder compatibility remains unverified.

## v0.2.1 header regression checks

Parser tests accept versions 0/1 and reject unknown versions, bad magic and oversized version-1 packets. Recorder-dialog tests cover login counts and SystemInfo fallback for both versions. The video integration fixtures now send version-1 DVRIP responses for login, monitor commands and fragmented video, including four-channel autoplay. The user's physical recorder response has not been captured, so its exact header version remains unconfirmed.

## v0.3 navigation and recovery

Navigation tests exercise double-click target focus, previous/next assigned channels, wraparound, empty-cell skipping and restoring the grid. A loopback DVRIP fixture disconnects an actively decoded/recorded stream, accepts a second authenticated monitor session, and verifies frame recovery. Both the original recording and the new reconnect segment are decoded for validation. The fixture asserts that manual stop does not open another connection. Physical-recorder dropout behavior and long-duration stability still require field testing.

## v0.4 discovery validation

Tests cover XM discovery framing, advertised service ports, ONVIF probe correlation and sender matching, malformed/oversized/deep XML, authenticated Media and Media2 profiles, GetCapabilities fallback, rejected credentials, cross-host stream rejection, HTTP authentication and cancellation. The WS-Security digest is independently checked in the mock server. Existing playback, reconnect, recorder and navigation suites are retained.

A six-second real-network smoke scan returned one ONVIF camera on the available LAN. Real XM broadcast responses and authenticated real-camera ONVIF stream resolution remain unverified. No universal ONVIF compatibility is claimed.

## v0.5 device management

Tests exercise removing an ONVIF stream referenced by multiple cells, keeping other channel identities after row reindexing, stale drag identities, clearing names/zoom/video state, removing the last entry and no-selection removal. Navigation and protocol regression tests are also run. Connection changes reuse the existing tested stop/start path; saved media files are never deleted by device removal.

## v0.6 zoom and recording tests

Geometry tests verify off-center cursor anchoring, reset to 1×, zoom bounds, letterbox behavior and resize clamping. Reader tests verify codec/frame-rate extraction, clean EOF, oversized frames and truncated payloads. Synthetic H.264 and H.265 Sofia recordings are played through the native worker and recorded to MKV, then checked with ffprobe and decoded with FFmpeg. No original encrypted recording was available; firmware-specific file compatibility remains unverified.

## v0.7 device lists and camera controls

Release build, all nine CTest suites, and offscreen app startup passed on the environment above. The updated workspace screenshot was visually inspected. New tests cover encrypted round trips, randomized ciphertext, wrong passphrases, header/ciphertext/tag tampering, truncated files, invalid/duplicate device identities and cross-host control addresses. Loopback camera simulations assert XM target channel and move/stop parameters, ONVIF profile-token escaping and advertised axis gating, and Stop after both successful and rejected movement requests. Existing video/reconnect, discovery, device removal and zoom regressions passed.

No physical PTZ camera was moved. Preset behavior, focus/iris and setting recorder time still require firmware validation. Windows builds and clean-system package installation were not executed here. Saved device lists require explicit Save/Load; roles, tours, broad remote configuration and dedicated multi-monitor management remain pending.

## v0.7.1 ONVIF compatibility

Release build, discovery_onvif and controls_vault suites, and offscreen startup passed. Regression fixtures require Type=All for Media2 PTZ configuration, verify compatible-configuration and service-lookup fallbacks, and assert camera-default velocity-space omission in explicit compatibility mode. A PT1S–PT100S fixture requires PT1S in ContinuousMove and a subsequent Stop.

Read-only checks against the user's two cameras confirmed ONVIF responses on port 8899, PTZ configuration tokens, standard velocity spaces and a one-second minimum timeout. On the local camera, Media2 without Type returned no PTZ configuration while Type=All returned it. No physical movement, preset changes or speed/position behavior was tested.

## v0.8 live audio and keyboard controls

All nine suites passed after integrating live audio and retained PTZ controls. The RTSP fixture now advertises a G.711 mu-law audio track and sends interleaved RTP audio alongside fragmented H.264. The native worker must produce video and speaker PCM, stop delivering PCM when muted, resume after unmute and stop after disconnect. A generated AAC/video container also exercises audio decoding and 44.1-to-48 kHz resampling. Tests use SDL's dummy audio output, so no sound is played into the room.

Keyboard tests exercise modified arrow, zoom and Stop bindings, preserve plain arrows, reject commands before configurations are loaded, and issue ONVIF movement after hiding the controls dialog. The workspace was rendered and visually inspected. Physical speaker output, real-camera keyboard movements and Windows runtime behavior remain unverified in this update. Recording files remain video-only and direct DVRIP audio is not implemented.

## v0.9.0 NVR archive playback

A loopback simulated DVRIP recorder verifies authenticated OPFileQuery, zero-based channel selection, the shared-session two-socket Claim/Start sequence, the exact returned ByName path, fragmented H.264 and H.265 media decoding, explicit media EOF without replay, and best-effort Stop. It checks actual frames produced by the native FFmpeg worker. The fixture is synthetic; physical NBD88X16S-KL-V3 playback, vendor-specific EOF notifications, timing accuracy and encrypted archives remain unverified. Initial loopback execution was blocked by the sandbox; the test passed after granting network permission for local sockets.

The v0.9.0 Release build completed on Linux. All ten CTest suites passed (the archive suite separately, then all nine existing suites), including live video/audio, reconnect, discovery, PTZ and vault checks. The rebuilt application passed an offscreen startup smoke check. No Windows build or physical NVR connection was performed for this release.

## v0.10.0 playback workspace

Eleven CTest suites passed, including new timeline drag/commit, channel selection, gap and adjacent-recording boundary checks. Archive integration now asserts an interior start-time request, which caught and fixed URL decoding of timestamps. H.264/H.265 simulated archive decoding and all previous regression suites passed. The playback workspace was inspected in an offscreen screenshot; calendar header contrast and the right-edge time label were corrected. Physical recorder seeking and multi-camera alignment remain user verification items. Playback time is estimated from decoded frame count and the reported frame rate; it is not reconstructed from recorder timestamps.

## v0.11 timestamp correction and speed

All eleven suites passed. The synthetic NVR returns timestamped H.264/H.265 footage at 12:22:30 despite an interior request for 12:00:01. Tests check decoded recording time rather than requested time, and exercise all six rates from .25× to 8× with pacing-duration checks, fragmented delivery, EOF, and matching Start/DownloadStart stop commands. Parser checks validate packed date fields, delta interpolation and invalid timestamps. Recorder overlay clock differences, actual device DownloadStart support, achievable remote fast playback and multi-camera alignment still require physical testing. EOF no longer assigns file-end metadata to the displayed clock.

## v0.11.1 recording search compatibility

All eleven suites passed. Loopback protocol tests cover Ret119 empty searches, a filtered search returning119 followed by a successful minimal request, preserving the channel/time range, permission denial, and119 during login remaining an error. The Release executable passed an offscreen startup smoke check. Linux executable and DEB rebuilt; channel2/3 results on the user's physical NVR remain unverified.

## v0.11.2 hourly search compatibility probe

Protocol and archive integration suites passed. A loopback recorder returns119 for both full-day formats but exposes a channel-2 recording for September11 12:00–13:00; the new fallback retrieves it. Tests retain empty results and authentication/permission error handling. H.264/H.265 archive timestamp and six-speed checks passed. Physical channel-2/3 retrieval is not verified; the narrower search remains an experimental compatibility probe.

## v0.11.3 remove NVR playback from the app

Removed the NVR recordings action and excluded ArchiveDialog from the application build at the user's request. Existing live-view, camera-control, recording and snapshot actions remain connected. The Release application rebuilt and passed the offscreen startup smoke check. No protocol changes were made; archive source/tests remain as reference only.

## Documentation and native build script (2026-09-21)

Replaced the README with illustrated current-state documentation, explicit Codex authorship, playback/P2P limitations and a consolidated source list. Six supplied screenshots were copied unchanged into the source tree; relative image/document links and coverage of previously recorded protocol/cloud reference links were checked. The shared native build script passed Bash syntax and invalid-option checks, then completed Linux Release configuration/build, all eleven CTest suites, offscreen startup and TGZ packaging using the existing build tree. CI YAML parses and now invokes the shared script. Windows UCRT64 instructions/tool names were checked against official documentation, but no Windows build/runtime or remote GitHub Actions run was performed. The application remains v0.11.3.
