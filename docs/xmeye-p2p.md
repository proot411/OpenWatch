# XMEye cloud connection investigation

Status: research blocked on a successful client connection capture or a usable vendor SDK package. No cloud transport is implemented, and the app's cloud option remains unavailable. A serial number must not be treated as a DNS name or an IP address.

## Target and confirmed observations

The user's screenshot identifies NBD88X16S-KL-V3 firmware, with the software-version prefix V4.03.R11.C6380251.1220 and release date 2024-06-17. The screenshot shows Network Mode RPS and a connected cloud status. The full serial number is masked. The user verified that the official app streams remotely with NetBird disconnected. This establishes a working vendor remote path, but does not establish whether the media uses a relay or a direct peer connection negotiated by the cloud. The original screenshot should be used to verify the complete wrapped firmware-version string before firmware-specific assumptions are made.

## Public implementation paths examined

- [XM's public SDK documentation](https://github.com/xmeye/openplatform-docs) describes mobile, web and Windows SDK offerings. It is documentation, not a complete portable open-source transport.
- A published [NetSDK header](https://gist.github.com/fbstj/8cccdc7010d93510311b565295ff16d5) declares H264_DVR_Login_Cloud with serial number, device credentials and UUID. A declaration does not supply the rendezvous, relay or reliable transport implementation. A corresponding library, matching ABI, distribution terms and supported platforms would need verification before an optional SDK adapter is viable.
- [XM's REST specification](https://github.com/xmeye/openplatform-docs/blob/master/docs/DevelopersMustRead/DevelopersMustRead-Protocolspecifications.md) documents signed requests using developer identity/key material. These APIs alone do not document the complete RPS live-video wire path. Do not borrow another application's credentials or assume that API signing supplies media transport.
- The local DVRIP implementations examined do not establish current RPS/P2P compatibility. No complete reusable open-source RPS client was identified in this investigation. This is a search result, not proof that no implementation exists.

## Next required evidence: a successful official-client session

On the Android phone:

1. Use [PCAPdroid](https://github.com/emanuele-f/PCAPdroid). Select **PCAP file** as the dump mode and set the **App Filter** to the official XMEye app only before starting capture.
2. Keep NetBird disconnected on the viewing phone. Use mobile data or the remote site's Internet connection. The NVR-site laptop's routing can remain running.
3. Close XMEye, start capture, reopen XMEye and connect to this NVR by its saved serial-number entry.
4. Show one channel in low quality for about 10 seconds, stop playback, then stop capture. Record the XMEye app version and whether playback succeeded during capture.
5. Transfer the PCAP to the laptop and provide its local path for analysis. Keep the original private: it may contain device identifiers, password-equivalent values, account tokens or video. Do not publicly upload it. Do not install a TLS interception certificate for this first capture.

PCAPdroid uses a local Android VPN interface; it does not route traffic through a remote VPN server. Its non-root mode preserves application data but synthesizes some transport headers and may affect NAT traversal. If streaming fails only during capture, preserve that fact; an interface/router capture of the working official client is the next approach. A capture may still contain encrypted application data, so it is evidence to analyze, not a guarantee of immediate implementation.

Reference: [PCAPdroid capture, app filters and packet-analysis limitations](https://emanuele-f.github.io/PCAPdroid/quick_start).

## Implementation boundary once the handshake is known

1. Identify the bootstrap/lookup exchange, authentication and any relay allocation from the successful session. Use only the user's device and authorized credentials.
2. Implement bounded parsers and synthetic fixtures for the observed framing before sending live requests. Do not replay captured tokens as reusable credentials.
3. Separate transport from DVRIP commands: cloud session establishment, cancellation, keepalive, reconnect and media demultiplexing must be tested independently from the existing local TCP path.
4. Connect the verified session to channel enumeration and the existing video pipeline. Cloud errors must not silently fall back to the private IP during a P2P test.
5. Enable serial-number connection in the UI only after the transport can actually authenticate and receive a stream. Confirm real playback with NetBird disabled on the viewing client. Clearly report negotiated relay/direct mode only when supported by observable protocol evidence.

No cloud-server requests, device logins, packet replay or changes to the NVR were made during this investigation. No new executable is needed for these research notes.

## Follow-up: XMCloudPlatform section

The user pointed to [XMCloudPlatform](https://github.com/xmeye/openplatform-docs/tree/master/docs/XMCloudPlatform). Its substantive document, [XMCloudPlatform-ThirdInterface.md](https://github.com/xmeye/openplatform-docs/blob/master/docs/XMCloudPlatform/XMCloudPlatform-ThirdInterface.md), specifies alarm-message delivery to a third-party HTTP server: a form-encoded POST containing a JSON `param` field with alarm type, time, channel, serial number and event status. This is useful for future cloud alarm integration. It does not specify device lookup, cloud login, NAT traversal, RPS relay negotiation or video streaming. The AuthCode field is explicitly marked to be ignored in that alarm example; it is not documented as a cloud-login credential.
