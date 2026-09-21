# OpenWatch architecture

OpenWatch is an incremental native Qt 6 / C++17 VMS implementation. This first milestone establishes the Sofia/DVRIP and RTSP video path.

Device URL → protocol adapter → FFmpeg demuxer → compressed packets → decoder → latest-frame mailbox → Qt grid
                                                ↘ MKV remux recorder

Each occupied cell owns one worker thread. Network deadlines and cancellation keep shutdown bounded. The UI polls a latest-frame mailbox, so slow painting cannot accumulate queued video frames. Hardware decoding is attempted through FFmpeg device contexts, with software fallback at decoder initialization. Frames are downloaded and converted to RGB for portable Qt rendering; zero-copy GPU rendering is a future optimization.

DVRIP uses a TCP session, Sofia password digest, monitor claim/start, bounded 20-byte message framing and bounded media-frame assembly. Its elementary H.264/H.265 output enters FFmpeg via custom AVIO. RTSP/RTMP/HLS and local media enter the same demux/decode path directly. Recording remuxes compressed video to MKV, beginning on a keyframe. DVRIP timing is synthesized from the reported frame rate; audio is not implemented.

Device lists can be saved explicitly as passphrase-encrypted files; plaintext credentials exist only in process memory. There is no automatic startup unlock or OS keychain integration. No cloud service or telemetry is used.

Future services: durable SQLite recording index and retention; scheduling/event rules; role enforcement; audio and extended PTZ;  tray and multi-monitor layout persistence. These are explicit future work, not implemented capabilities.

## v0.3 connection lifecycle and navigation

A worker runs bounded connection attempts and retries transient network/decode failures using cancellable 50 ms waits with exponential delay capped at 30 seconds. Each attempt releases its FFmpeg contexts and DVRIP socket before reconnecting. File EOF and explicit stop are terminal. Known authentication rejection, invalid protocols and recording errors are terminal. Recording restarts in a uniquely named sibling MKV, preserving the completed segment. Simultaneous shutdown cancels all workers before joining them.

Navigation temporarily rearranges existing cell widgets rather than replacing their workers. Focus/unfocus does not close hidden connections. The explicit layout control retains its original behavior of disconnecting channels removed from the layout.

## Discovery and ONVIF

The discovery module uses event-driven UDP sockets and a six-second scan window. SOAP service/profile lookup runs in a separate worker thread with cancellation and bounded replies. The dialog returns selected resolved URLs to the existing grid/video pipeline. Discovery does not save credentials, change camera configuration or contact cloud services. Multi-language support is out of scope per user preference.

## Device entry identity (v0.5)

Each connected/list-dragged entry receives an in-memory UUID. Cells refer to that UUID rather than a mutable list row. Drag-and-drop resolves the UUID at drop time, so deleting an earlier row cannot redirect a drop or detach another entry's views. Removal cancels all matching workers before clearing their state and removing the URL from the session list. A local playback file has no device-entry identity.

## Device files and camera control (v0.7)

`vault.cpp` uses OpenSSL EVP AES-256-GCM. OWV1 files contain a 4-byte version marker, random 16-byte salt, random 12-byte nonce, ciphertext and 16-byte authentication tag. PBKDF2-HMAC-SHA256 derives a 32-byte key with 600,000 iterations. The complete 32-byte header is authenticated as AAD. Derived key buffers are cleansed, file sizes are bounded, and QSaveFile commits atomically. Passphrases are not persisted. Authentication and JSON/URL validation precede changes to the active list. This has not received an independent security audit.

`registry.h` serializes UUID, label, URL, group and ONVIF endpoint. Filtering does not reorder URL indices or change active workers. Credentials stay inside the encrypted payload; group files are manually saved, not automatically updated.

`controls.cpp` uses separate short-lived worker sessions so PTZ does not take over a live monitor socket. XM uses OPPTZControl (1400), SystemInfo (1020) and explicit OPTimeSetting (1450). Stop is attempted after each XM and ONVIF movement even when the acknowledgement fails or the dialog is closed. ONVIF uses the selected media profile's PTZ configuration, GetConfigurationOptions, ContinuousMove with a device timeout and Stop, plus preset commands. Nonstandard velocity spaces are disabled. Network failures cannot guarantee delivery of XM Stop; errors report this explicitly. Physical firmware validation remains necessary.

## v0.8 audio and keyboard control

CameraControls objects are retained by stable device identity after their dialogs are hidden. Workspace shortcuts dispatch only to the selected cell's cached object, preserving its selected ONVIF profile, endpoint, capability flags, timeout and clock offset. Source changes, removal, list replacement and recorder/discovery replacement invalidate cached controls. Commands remain serialized and bounded; auto-repeat does not enqueue moves.

Video demux now routes audio packets to AudioDecoder when the selected cell is unmuted. libavcodec decodes and libswresample converts to 48 kHz stereo S16; AudioOutput submits to SDL2's default output device. Muting clears/closes the output, errors remain separate from video status, and a 250 ms queued-audio threshold drops stale buffered audio. Disconnect/stop clears sound. Audio is not remuxed into the video-only recording path. Direct DVRIP elementary-video input still omits audio. SDL2 and libswresample are added to builds and CI dependencies.
