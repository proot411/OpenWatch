# AGENTS.md

This file provides repository-level context for AI coding agents working on OpenWatch.

Read this file before modifying DVRIP, XMEye CloudID, playback, recorder search, ONVIF, media handling, or documentation.

## Project purpose

OpenWatch is an experimental native desktop VMS written in C++17 using Qt 6, FFmpeg, OpenSSL and SDL2.

Primary targets are:

- Xiongmai/XM/XMEye DVR/NVR devices;
- ONVIF IP cameras;
- RTSP and other FFmpeg-compatible streams.

The project deliberately avoids requiring vendor ActiveX/browser plugins.

OpenWatch is an interoperability project, not a reimplementation of the proprietary XMEye SDK.

## AI development

Much of the OpenWatch source, tests and documentation has been generated or revised by AI coding agents under the project owner's direction.

That makes regression discipline especially important.

Do not assume existing AI-generated comments or documentation are correct merely because they are already in the repository.

Prefer:

1. existing passing tests;
2. observed physical-recorder behavior;
3. supplied packet captures;
4. independently verified protocol references;
5. clearly labelled hypotheses.

Never turn an inference into a documented fact without evidence.

## Primary real test recorder

The project owner's current primary recorder is:

```text
Xmeye N1009KL
10 Channel NVR
3 ONVIF cameras configured
```

The three cameras have been configured on the recorder and used for recording/playback testing.

Treat this recorder as the project's current real-device validation target.

It is **not** representative evidence for every Xiongmai/XMEye firmware family.

Do not rename it to another inferred board or firmware model unless the owner provides evidence.

Historical documentation may mention earlier firmware identifiers or investigation targets. Preserve those only where they describe historical evidence.

## Evidence levels

When implementing or documenting recorder behavior, distinguish:

### Synthetic

Behavior only demonstrated against project test fixtures or simulated network peers.

### Static research

Behavior derived from SDK symbols, disassembly, headers or documentation.

### Capture-backed

Behavior observed in packet captures from an official client.

### Owner-confirmed

The project owner tested an OpenWatch build and reported the result.

### Physical verified

A reproducible operation was performed against the owned recorder/cameras during the project.

Never describe synthetic or static research as physically verified.

## Repository architecture

High-level flow:

```text
                              ┌──────────────────┐
                              │      Qt UI       │
                              └────────┬─────────┘
                                       │
                ┌──────────────────────┼─────────────────────┐
                │                      │                     │
                ▼                      ▼                     ▼
         Device registry          Discovery            Playback UI
                │              XM / ONVIF                  │
                │                                            │
                ▼                                            ▼
        Stream URL / source                        Network / WFS playback
                │                                            │
        ┌───────┴────────┐                           ┌───────┴───────┐
        ▼                ▼                           ▼               ▼
   DVRIP/Sofia        FFmpeg URL                DVRIP playback      WFS
        │            RTSP/HLS/etc.                    │
        │                │                            │
        └────────┬───────┘                     local or CloudID
                 ▼
         FFmpeg demux/decode
                 │
       ┌─────────┼──────────┐
       ▼         ▼          ▼
     video      audio     recorder
       │         │          │
       ▼         ▼          ▼
 Qt framebuffer SDL2        MKV
```

## Important source areas

Before editing, inspect the current tree rather than relying only on this list because filenames can evolve.

Major areas currently include:

```text
src/main.cpp
    Main application/window wiring and top-level UI behavior.

src/dvrip.*
    Local DVRIP/Sofia protocol implementation.

src/xmcloud.*
    XMEye CloudID, rendezvous, relay and XMIP transport.

src/networkplayback.*
    Network recorder playback behavior.

src/playbackwindow.*
    Combined playback UI.

src/archivedialog.*
src/archivetimeline.*
    Recorder/disk playback and timeline related code.

src/discovery.*
src/discoverydialog.*
    XM discovery and ONVIF discovery/profile resolution.

src/controls.*
    XM and ONVIF camera controls/PTZ.

src/audio.*
    FFmpeg audio decode / SDL output.

src/registry.*
src/vault.*
    Device registry and encrypted `.owv` persistence.

src/video* / stream worker related files
    FFmpeg demux/decode, DVRIP media handling and recording.

tests/
    Protocol and integration regression tests.

tools/
    Recorder/cloud/WFS investigation helpers.

docs/
    Architecture, protocol and evidence documentation.
```

Do not invent an architectural component merely because it would be conventional in another VMS.

Check the actual source.

## Live DVRIP behavior

Local XM recorder access normally uses TCP port `34567`.

General live sequence:

```text
TCP connect
→ login
→ channel/session setup
→ OPMonitor Claim
→ OPMonitor Start
→ Sofia media frames
→ FFmpeg decode
```

Multiple grid cells generally use independent stream workers/connections.

The NVR may advertise its maximum input capacity instead of the number of cameras that are actually configured or online.

Never infer “10 online cameras” simply because the N1009KL reports ten channels.

UI channels are one-based for humans.

Protocol channels are generally zero-based.

## Sofia media

The DVRIP media path carries Sofia-framed H.264/H.265 elementary video.

The parser is deliberately bounded.

Preserve existing limits against:

- oversized DVRIP bodies;
- oversized Sofia frames;
- excessive buffered media;
- malformed headers;
- unexpected codecs.

Do not loosen these bounds merely to make malformed test input pass.

DVRIP video timestamps and recorder playback timestamps are different concerns. Do not mix live synthesized timing with archive timestamp interpretation.

## FFmpeg pipeline

FFmpeg performs demuxing/decoding for common media paths.

Hardware decoding may be attempted depending on the platform/build.

CPU decoding must remain a valid fallback.

The Qt renderer currently receives host-memory image data; initialization of a hardware decoder does not imply a zero-copy rendering pipeline.

Do not claim zero-copy/GPU rendering unless the actual frame path is changed accordingly.

## RTSP audio

RTSP audio is decoded using FFmpeg and played with SDL2.

Current recording remains video-only.

Direct DVRIP live audio is not currently implemented.

Do not route an unsupported DVRIP audio block into the SDL path without identifying and validating the recorder's audio framing/codec behavior.

## XMEye CloudID architecture

The implemented cloud route was derived primarily from the older Windows VMS behavior.

It is not the Android XMEye account/RPS implementation.

Conceptual sequence:

```text
CloudID / serial
      │
      ▼
bootstrap service
      │
      ▼
serial directory
      │
      ▼
public endpoint discovery
      │
      ▼
rendezvous
      │
      ▼
relay allocation
      │
      ▼
association
      │
      ▼
XMIP reliable UDP
      │
      ▼
encrypted DVRIP
```

Control and media can use separate relay associations while sharing the higher-level authenticated DVRIP session semantics.

## Relay versus P2P

Be precise in terminology.

The currently working implementation is a **CloudID relay path**.

OpenWatch does **not** currently implement verified direct NAT hole punching.

Do not describe relay operation as direct P2P merely because vendor marketing, APIs or UI use “P2P” generically.

Preferred wording:

> XMEye CloudID relay

or:

> cloud relay transport

Direct peer connectivity should only be claimed if packet evidence shows the recorder/client communicating directly after traversal.

## XMIP

XMIP is the reliability/framing layer observed inside the CloudID UDP transport.

Important implementation detail:

**The sparse XOR transform resets for each XMIP data-slice payload.**

Do not apply it across a fully reassembled application message.

That mistake previously allowed DVRIP structures to appear partially plausible while corrupting video payloads.

The receiver must continue handling:

- sequence numbers;
- acknowledgements;
- duplicates;
- loss;
- retransmission;
- bounded out-of-order delivery;
- reassembly limits;
- cancellation.

Do not reuse captured sequence numbers, client IDs or session keys.

Generate fresh session material.

## Cloud authentication

The observed cloud DVRIP login negotiates recorder-supported cryptographic behavior.

Current implementation includes observed RSA/AES handling.

This is **not TLS**.

Do not describe the protocol as certificate-authenticated secure transport.

The recorder-provided RSA key is not authenticated through a standard PKI certificate chain.

XMIP XOR is not cryptographic security.

Never log:

- passwords;
- password-equivalent hashes where avoidable;
- communication keys;
- CloudID/serial identifiers in public fixtures;
- captured authentication payloads.

## NVR network playback

Recorder playback is not the same connection sequence as live OPMonitor.

Conceptually:

```text
control connection/association
        │
        ├── authentication
        │
        ├── OPPlayBack Start/Stop
        │
        │
data/media connection/association
        │
        ├── OPPlayBack Claim
        │
        └── playback media
```

Maintain separate control/media semantics.

Do not collapse them into one connection unless capture-backed evidence proves that firmware accepts it.

## Playback channel routing

This is a critical regression area.

Official VMS capture analysis showed that command `1420` uses the DVRIP header field at offsets 12–13 to select the playback camera.

Observed values included:

```text
0 → CH1
1 → CH2
2 → CH3
```

Playback media command `1422` also carried corresponding values.

Playback response headers do not necessarily echo the selected channel.

File query `1440` and Claim `1424` were observed using zero in this field.

Therefore:

- set the requested zero-based playback channel where the implementation currently expects it for playback Start/Stop;
- do not require reply headers to echo that value;
- do not change Claim/query headers just for symmetry;
- retain regression tests for nonzero channels.

The physical N1009KL has three configured ONVIF cameras, so CH2/CH3 playback regressions matter.

## ByTime playback

Vendor SDK research showed official playback constructing a filename-like selector containing a channel and requested timestamp.

Do not replace this with invented JSON fields unless packet evidence supports the change.

Network playback currently favors direct ByTime operation because file-list search behavior is not reliable enough to make listing a prerequisite.

## OPFileQuery

Recording-list searches are firmware-sensitive.

Known behaviors include:

- Ret 119 for no matches in the OPFileQuery context;
- different results with optional fields removed;
- narrower time windows changing results;
- incomplete result sets.

Do not map all Ret 119 responses globally to “empty.”

Context matters.

Authentication/login errors with similar codes remain errors.

Do not claim:

```text
empty OPFileQuery == no recording exists
```

Use:

```text
recorder reported no matching files for this query
```

Direct ByTime playback can still be attempted where appropriate.

## Network playback speed

The current production network playback path stays at `1×`.

Earlier accelerated DownloadStart experiments were insufficiently reliable on the physical recorder.

Do not re-enable 2×/4×/8× network UI merely because synthetic tests pass.

Require a specific physical-recorder validation plan first.

Disk playback is independent and may retain local speed options.

## WFS disk handling

Treat all recorder disk access as read-only.

Code must not:

- format;
- repair;
- rewrite;
- alter allocation structures;
- modify recorder metadata;
- update recording indexes.

WFS research belongs in the disk reader/tools layer, not in normal network playback code.

Do not describe WFS support as universal XMEye filesystem support.

## ONVIF

OpenWatch implements only the portions of ONVIF needed by the project.

Current areas include:

- WS-Discovery;
- device services;
- Media;
- Media2;
- stream URI;
- experimental PTZ/presets.

Some tested cameras require Media2 `Type=All` to return useful PTZ configuration.

Some cameras expose ONVIF on non-port-80 endpoints.

Do not hard-code port 80.

Do not claim ONVIF Profile S/T certification.

## Device vault

`.owv` device files contain credentials and must remain authenticated/encrypted.

Current design uses AES-256-GCM with PBKDF2-HMAC-SHA256.

Do not add plaintext fallback storage.

Do not print decrypted device lists in tests/logs.

Changes to vault format require explicit compatibility and corruption/tamper tests.

## Tests

Before declaring a protocol change successful, run the narrowest relevant suite plus the broader regressions.

Protocol modifications should normally include a synthetic regression reproducing the bug or newly understood behavior.

Examples:

- playback channel change → assert wire header for CH2/CH3;
- XMIP change → loss/reorder/retransmit + capture regression where available;
- DVRIP parser → malformed/truncated/boundary cases;
- ONVIF XML change → malformed/deep/host-validation cases;
- recording change → playable resulting file checked by FFmpeg.

Do not “fix” a failing test by weakening an assertion until you understand what real behavior that assertion represents.

## Private research material

Private inputs previously used during research include:

- XMEye/VMS packet captures;
- playback capture;
- VMS installation archive;
- NetSdk.dll;
- NetSdk.pdb;
- device identifiers;
- credentials;
- captured video.

These are evidence inputs, **not repository dependencies**.

Do not commit them.

Do not add hashes, tokens or identifiers that could expose the owner's recorder unless explicitly sanitized.

OpenWatch must remain buildable without proprietary vendor binaries.

## Vendor code

Static inspection of vendor files can inform interoperability.

Do not:

- link proprietary DLLs into OpenWatch;
- redistribute proprietary SDK binaries without a verified license permitting it;
- copy substantial decompiled vendor implementation into the project;
- represent vendor symbol names as OpenWatch source authorship.

Implement protocol behavior independently.

## Documentation rules

Whenever behavior changes, update whichever of these are affected:

```text
README.md
docs/manual.md
docs/architecture.md
docs/protocol.md
docs/validation.md
docs/XMEye-CloudID-P2P.md
docs/XMEye-remote-playback-research.md
docs/production-*.md
AGENTS.md
```

Do not leave old statements such as:

```text
P2P is not implemented
cloud playback is unavailable
NVR playback was removed
real recorder has not been tested
```

without historical qualification if the current implementation has superseded them.

Historical sections should explicitly say they describe an earlier version.

## Documentation wording

Prefer:

> Direct NAT hole punching is not implemented.

instead of:

> P2P is not implemented.

Prefer:

> Verified on the project's Xmeye N1009KL test recorder.

instead of:

> XMEye recorders support this.

Prefer:

> Owner-confirmed on the current recorder.

instead of:

> Fully supported.

Prefer:

> The recorder returned no files for this query.

instead of:

> No footage exists.

## Safety boundaries

Normal OpenWatch operation should remain read-mostly with respect to recorders.

Commands that modify device state must be deliberate and visible to the user.

Examples include:

- PTZ movement;
- preset writes/deletes;
- recorder clock changes.

Do not silently add:

- firmware update;
- account modification;
- recording deletion;
- disk formatting;
- network reconfiguration;
- factory reset;
- security-setting modification.

Such operations require explicit project-owner requirements and strong safeguards.

## When uncertain

If recorder behavior is unclear:

1. inspect existing tests;
2. inspect current docs;
3. check capture-backed evidence;
4. check supplied research;
5. add instrumentation that does not expose credentials;
6. reproduce with a simulator when possible;
7. ask for a physical test if the conclusion depends on firmware.

Do not guess protocol fields until they appear to work.

A plausible-looking DVRIP response is not enough evidence.

## Definition of done for recorder/protocol changes

A recorder-facing change should normally have:

- source implementation;
- focused automated regression;
- broader regression pass;
- bounded parsing/network behavior;
- no sensitive logging;
- updated docs;
- clearly stated evidence level;
- physical recorder validation where the claim depends on firmware.

If physical validation has not occurred, say so.