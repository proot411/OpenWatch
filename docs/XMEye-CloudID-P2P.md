# XMEye CloudID relay prototype

Date: 2026-09-28. Implemented by OpenAI Codex under the project owner's direction, in `OpenWatch_test` only. Original implementation code is covered by the repository license. Vendor binaries, symbols, packet captures, device identifiers, public/private camera addresses, credentials and session material are not included in source or packages.

## Scope and status

This implements the older Windows VMS accountless CloudID route, rather than the Android account/RPS route. It attempts server lookup, rendezvous, UDP relay association, XMIP reliable transfer, encrypted device login, channel enumeration and live video through the existing decoder. This remains an experimental implementation pending actual authenticated video verification on the owner's recorder. A relay handshake alone does not establish that video works.

Select **XMEye recorder → XMEye cloud / serial number — experimental relay**. Enter the recorder's serial number and device username/password. Start with **Channel 1 — first connection test** and **Substream**. Choosing **All detected channels** attempts every reported input (subject to the existing 64-cell limit), so it can consume substantial upstream bandwidth and recorder sessions. No XMEye account credentials are used.

The cloud URL carries an explicit `cloudId` query value; `xmeye-cloud` is a placeholder host and is never resolved. This preserves serial-number case. The channel URL builder preserves cloud connection parameters. No private IP is dialled as a fallback. Cloud transport is only started by explicitly connecting a cloud entry; it does not run on ordinary startup, local camera discovery or local DVRIP connections.

Not implemented: successful direct UDP hole punching, NAT classification, Android RPS/TCP 6611, cloud audio, AES-encrypted video variants, broad regional/firmware compatibility, relay quota handling, or shared control-session pooling across grid cells. Playback1 adds a new single-camera, direct-time NVR playback UI for local/cloud connections, with simulated tests passed and real-recorder validation pending; it does not restore the old recording-list UI. Read-only WFS disk playback remains available separately.

## Live handshake result

On 2026-09-28, with the owner’s explicit permission, the standalone probe completed bootstrap, serial lookup, rendezvous and relay association against the real recorder. It received and decoded the recorder’s capability reply advertising RSA with a 1024-bit key. No username or password was sent. This verifies the remote handshake and capability exchange; authenticated live video was subsequently verified by the owner. See the production release notes for current playback support.

## Evidence used

- The owner's **Review Xmeye Network Capture** conversation (retrieved through the app) supplied the flow hypothesis and identified the relevant SDK.
- The owner's successful Windows VMS USB-tether capture (`xmeye-phone.pcapng`) supplied actual bidirectional packets. Offline validation reconstructed 829 DVRIP messages, including 777 video packets, from this recording. That is evidence of parser interoperability with this capture, not a live success claim.
- The owner's VMS ZIP, specifically `NetSdk.dll` and its matching `NetSdk.pdb`, was inspected statically. Symbol names and disassembly explained packet fields, the `udp_slice_header` structure, sparse XOR transform, capability decoding, credential encryption and command encryption policy. The proprietary DLL is not executed, linked, bundled or copied into the open-source tree.
- Prior public references remain in [sources.md](sources.md). [XM's public documentation](https://github.com/xmeye/openplatform-docs) provides background, but does not specify this full wire protocol. Cryptographic primitives use the existing OpenSSL dependency; networking uses Qt Network.

This is an independent interoperability implementation derived from observed protocol behaviour. Symbol names guided investigation; this document is not a redistribution of vendor source code.

## Observed protocol

All integer fields below are little-endian unless explicitly stated. Network endpoints are dotted IPv4 strings with a separate port field. Parsers check lengths, termination, port validity, device/client identity and the expected sender endpoint. All waits and buffering are bounded and cancellable.

| Stage | Request / response | Observed shape |
|---|---|---|
| Bootstrap UDP 7999 | magic `0x2014`, commands 1524 / 1525 | 104-byte request; at least 32-byte reply. Address begins at byte 4; port at byte 28. The request marker `1234567890ab` is a protocol constant seen in the client. |
| Serial directory UDP 8777 | magic `0x2015`, commands `0xB000` / `0xB001` | NUL-terminated serial request. Reply: status at 4, echoed serial at 8 (100 bytes), rendezvous IPv4 at 108 (20 bytes), port at 128. |
| Public endpoint UDP 8765 | magic `0x2012`, commands 1000 / 1001 | 4-byte request; reply IPv4 at 4 (16 bytes), port at 20. |
| Rendezvous | commands 1004 / 1005 | 364-byte request advertises a newly generated client UUID, local connection ID, target serial and public UDP endpoint. Reply 1005 supplies a per-session XOR key. A reciprocal 1004 identifies the recorder and relay. |
| Relay allocation | commands 3001 / 3002 | 212-byte request contains client/device identifiers and connection IDs. Relay endpoint comes from rendezvous, rather than a fixed relay IP. |
| Association | commands 2000 / 2001 | 112-byte messages contain connection ID, UUID and synchronization tags. |
| Reliable data | command 2003 | 12-byte outer envelope plus XMIP slices. The slice length is at outer offset 6. |
| Maintenance / close | commands 2002 / 2004 | Bounded keepalive traffic and best-effort close; no logout or device configuration mutation. |

The bootstrap IPv4 default is the service endpoint observed in the supplied capture, not a device IP. Service infrastructure may change. A `bootstrap` query override exists for controlled testing; the default is not a guarantee of global availability.

### XMIP

A slice contains `XMIP` (4 bytes), sequence number (4), sequence count (1), flags (1) and reserved bytes (2). Flag bits indicate ACK, push, start and end. ACK windows identify consecutive received sequences. The implementation handles retransmission, duplicate packets and bounded out-of-order arrival before delivering complete ordered messages. It requests the observed sparse-XOR stride of four words; the per-association key comes from the rendezvous response. The first 16 words are transformed, then every stride-th word; trailing incomplete words are unchanged. The transform resets for each XMIP data-slice payload, excluding its 12-byte header. Slices are decoded before application-message reassembly.

The receiver is limited to 256 queued slices and 8 MiB per message/byte buffer. Sender writes use bounded retransmission and wait for an ACK. These bounds may reject unusually large or severely disordered traffic rather than allow unlimited memory growth. The relay is explicitly labelled as a relay; the UI never calls it a verified direct P2P connection.

### DVRIP authentication

1. Send the observed capability probe: command 1413 / OPMonitor Claim, channel 0, `CONNECT_ALL`, session 99999 in the DVRIP header and flag byte 12 set to 99.
2. Parse response 1414. The observed capability body is Base64 AES-128-CBC with zero IV and the protocol's static 16-byte wrapper key. Remove zero padding and parse JSON.
3. Require advertised `RSA_V1.5`, RSA login support and AES control support. Parse the `modulus,exponent` public-key representation. Honour `NotEncryptMsgID` and reject an unsupported policy.
4. Generate a new communication key with OpenSSL randomness. RSA PKCS#1 v1.5 encrypt the username, Sofia password digest and communication key independently. Encode each ciphertext as uppercase hex.
5. Put these into `UserName`, `PassWord`, `CommunicateKey`, `EncryptType: MD5`, `LoginType: DVRIP-Web`. Zero-pad and AES-wrap the JSON, Base64-encode it, append `#`, and send command 1000 with flag byte 12 set to 99.
6. Require a successful command response. Subsequent eligible control bodies use the negotiated communication key and the advertised command exclusion list. Media uses a separate relay association, claims the authenticated DVRIP session and starts OPMonitor from the control association.

This is a legacy protocol, not TLS: the server-supplied public key is not backed by authenticated certificate verification, the outer wrapper uses a static protocol key, and XMIP XOR is obfuscation. Do not describe it as end-to-end authenticated encryption. OpenWatch generates fresh IDs/keys and never reuses captured tokens. Credentials are not included in progress messages, test reports or package contents.

## Validation and limits

`cloud_test` exercises bounded parsing, AES/RSA operations, ordered reassembly, malformed input, cancellation, simulated bootstrap/directory/rendezvous/relay exchange, intentional packet loss, duplicate/reordered slices, encrypted login/control and authentication rejection. Existing local protocol and recorder-dialog tests remain relevant.

`cloud_capture_test` is an optional offline test against a private UDP JSON export, not a public fixture. It checks actual captured XMIP/DVRIP framing and authentication-wrapper decoding. The source tree contains only synthetic examples.

`cloud_probe` is an optional handshake/capability-only tool. It reads a serial number from standard input, reports stages without printing identifiers and never sends a password. A live test of this tool must be distinguished from credential authentication and decoded live video in release notes.


## Cloud2 video correction

The initial Cloud1 decoder incorrectly applied sparse XOR to whole reassembled XMIP messages. The correction applies it to each data slice before reassembly, and independently to each outgoing slice. The recorder sends the 20-byte DVRIP header and video chunks in separate slices; retaining that boundary is essential. The private-capture regression now parses Sofia frames and exports an optional new-only private elementary stream for FFmpeg decoding, rather than checking only DVRIP headers.
