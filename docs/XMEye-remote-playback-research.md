# XMEye remote playback: static interoperability research

2026-09-28. Analysis and original documentation by OpenAI Codex under the project owner’s direction. This is a research result, not a claim that OpenWatch can play NVR recordings over cloud.

## Evidence

Read-only static inspection of the owner-supplied `VMS.zip`, using the previously extracted `NetSdk.dll` and matching `NetSdk.pdb`. No vendor DLL was executed, linked or bundled; no device database or stored password was used. Relevant SDK symbol names and relative virtual addresses are listed below so the findings can be checked against this exact binary. Addresses are not portable to another SDK build. The prior network capture contains live viewing, not a verified playback exchange.

## What the SDK shows

- `CPlayBack::PlayBackByTime` (RVA 0x42c60) constructs a filename-shaped selector using `%02d_%04d-%02d-%02d %02d:%02d:%02d`. The first number comes from the supplied search structure’s first integer, used as its channel. The start date/time supplies the remaining fields. This is a significant difference from assuming a separate JSON `Channel` field selects time-based playback. The wire channel numbering must still be checked against a successful playback capture.
- The by-name path (RVA 0x449xx) preserves the recorder’s returned file information. Both paths call `CDvrDevice::device_open_channel` (RVA 0x61850) with internal channel type 19. This is an SDK connection type, not camera number 19.
- `CPackSenddata::sendSubDownload_comm` (RVA 0x79f40) builds command 1424 (0x590), the playback data-connection claim. `sendDownload_comm` (RVA 0x7a4a0) builds command 1420 (0x58c), playback control. They serialize `PlayBackControl` under the playback operation name. The existing separate command/media transport is relevant to implementing this through CloudID.
- The playback JSON serializer (RVA 0x1870xx) includes top-level `Action`, `StartTime`, `EndTime`; nested `Parameter.TransMode`, `PlayMode`, `Value`, `FileName`, `StreamType`, `IntelligentPlayBackEvent`, and `IntelligentPlayBackSpeed`. Its inspected body contains no standalone `Channel` key. Time-based selection therefore needs the SDK’s selector convention checked, rather than adding guessed fields.
- SDK playback callbacks include buffering and pause/resume handling. A robust player must service both cloud associations and handle flow control while seeking or paused; reusing the live stream UI alone is insufficient.

## Conclusions and limits

There is a concrete SDK playback path worth implementing, including by-time selection that may avoid dependence on a complete file list. It does not prove why earlier channel 2/3 searches returned 119, nor that this NVR firmware accepts every SDK variant. The claim/control sequence, enum spellings, response IDs, channel numbering, seek units, stop acknowledgement and end-of-stream behavior need a successful playback trace or explicit recorder test before enabling the feature. Static symbol names alone do not establish those details.

Next validation: capture one successful official VMS remote session opening channel 2 at a known recorded time, then pause, resume, seek and stop. Compare the file query, selector and claim/start payloads; replay only those read-only operations using freshly authenticated sessions. Keep this separate from the existing working read-only WFS disk player. No remote playback UI has been re-enabled by this research.

See [CloudID protocol notes](XMEye-CloudID-P2P.md) and [existing source attributions](sources.md). Private captures, identifiers, credentials and vendor binaries remain outside the repository and package.


## Experimental implementation update

Playback1 now implements ByTime on local TCP and CloudID relay, at the owner’s request. Static enum-table inspection confirms spelling `ByTime` (1) and `ByName` (0). The new UI bypasses file search, so it does not claim the prior channel-specific search failure is fixed. The source uses zero-based protocol channel numbers consistently with live viewing; verification against the real recorder remains necessary. The original caution above describes the research state before this prototype was added.

## Playback2: channel routing in the DVRIP header

Source: the user's private `xmeye-playback.pcapng`, supplied in **Review Xmeye Network Capture** (September 29, 2026). The capture is not redistributed. Independently checked the start of XMIP data slices after removing the observed XOR layer; no credentials or media are included here.

The little-endian 16-bit field at DVRIP offsets 12–13 is a playback channel selector for command 1420. Observed command counts for values 0, 1, 2 were 2, 5, 1 respectively. Playback media (1422) also carried those three channel values. The single Claim (1424) and all three file queries (1440) used zero in this header field. Replies to playback commands (1421) used zero even for nonzero-channel requests, so response headers must not be required to echo the channel.

OpenWatch previously zero-filled this field for all outgoing playback commands. Playback2 sets it to the requested zero-based channel for 1420, including best-effort Stop/DownloadStop during teardown, over both TCP and encrypted XMIP transport. Claim, queries, login and keepalive headers remain unchanged. The file-name selector remains as derived from the SDK. Local CH3 and encrypted-cloud CH2 mock tests check the actual wire header, not only the JSON selector.

This is a targeted fix for repeated CH1 playback. It does not yet implement the original VMS's reuse of one playback-media association across channel switches. Actual recorder validation is still needed. The older conversation's encryption limitation is no longer current for OpenWatch: its cloud control path already encrypts playback requests with the negotiated session key.
