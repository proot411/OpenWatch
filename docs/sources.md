# Sources and acknowledgements

This is the consolidated reference list from the recorded OpenWatch development work, plus the official dependency/build references used for this documentation update (2026-09-21). It distinguishes implementation references from unsuccessful/removed experiments. Links generally point to moving upstream branches or documentation; they are not pinned compatibility guarantees.

**Authorship:** OpenAI Codex generated and revised the original OpenWatch application, tests, build script and documentation under the project owner's direction. The owner supplied requirements, screenshots and hardware feedback. Referenced projects and third-party libraries belong to their respective authors. No upstream DVRIP implementation source files are vendored into OpenWatch.

## DVRIP / Sofia live view, discovery and controls

| Source | How it informed the project |
| --- | --- |
| [go2rtc DVRIP client](https://github.com/AlexxIT/go2rtc/blob/master/pkg/dvrip/client.go) | Login, DVRIP framing and monitor command interoperability |
| [go2rtc DVRIP producer](https://github.com/AlexxIT/go2rtc/blob/master/pkg/dvrip/producer.go) | Sofia media framing and codec interpretation |
| [alexshpilkin/dvrip](https://github.com/alexshpilkin/dvrip) | Independent protocol model and command reference |
| [DVRIP packet model](https://github.com/alexshpilkin/dvrip/blob/master/dvrip/packet.py) | Header/version behavior, including version 1 replies |
| [DVRIP system-information model](https://github.com/alexshpilkin/dvrip/blob/master/dvrip/info.py) | Recorder channel-count fields |
| [OpenIPC device discovery](https://github.com/OpenIPC/dms/blob/master/dms.py) | XM UDP discovery request/reply behavior |
| [OpenIPC/python-dvr](https://github.com/OpenIPC/python-dvr/blob/master/dvrip.py) | XM PTZ command structures, file-search and playback research |
| [Captured XM PTZ exchange](https://gist.github.com/knight-of-ni/26be98747cef99cd4fe7) | Firmware-specific movement/stop conventions |
| [DVRIP/Sofia reference codes](https://github.com/KostasEreksonas/DVRIP_Sofia_reference_codes) | Interpretation of reply codes, including 119 as no files found |

## ONVIF discovery, profiles and PTZ

- [ONVIF application programmer guide](https://www.onvif.org/wp-content/uploads/2016/12/ONVIF_WG-APG-Application_Programmers_Guide-1.pdf): service discovery and client-operation reference.
- [ONVIF Media2 WSDL](https://www.onvif.org/ver20/media/wsdl/media.wsdl): media profile operations.
- [ONVIF Media2 service specification](https://www.onvif.org/specs/2512/ONVIF-Media2-Service-Spec-v2512.pdf): GetProfiles configuration selection, including `Type=All`.
- [ONVIF PTZ WSDL](https://www.onvif.org/ver20/ptz/wsdl/): continuous movement, spaces, timeouts and preset operations.
- [ONVIF Device WSDL](https://www.onvif.org/ver10/device/wsdl/): device services and information.

These specifications guide a partial client implementation. OpenWatch is not certified for ONVIF Profile S or T.

## Recording and playback research — feature removed from the UI

- [Measured DVRIP protocol on an NBD8008R-U](https://github.com/Moxnatiy/hass-xmeye/blob/main/docs/protocol.md): Sofia timestamps, H.264/H.265 framing, recording queries, shared-session playback sockets, and firmware-specific seek/speed behavior. This is a different recorder/firmware from the user's NBD88X16S-KL-V3.
- [OpenIPC/python-dvr](https://github.com/OpenIPC/python-dvr/blob/master/dvrip.py): `OPFileQuery` and `OPPlayBack` examples.
- [DVRIP file-query model](https://github.com/alexshpilkin/dvrip/blob/master/dvrip/files.py): minimal recording-search fields, used for a compatibility retry.
- [DVRIP playback model](https://github.com/alexshpilkin/dvrip/blob/master/dvrip/playback.py): additional playback-command research.

Synthetic tests passed, but real channel searches remained unreliable. The UI was removed in v0.11.3. These sources are retained to explain the experiments, not to advertise supported playback or encrypted-file decryption.

## Cloud research (not an implemented feature)

- [Xiongmai Open Platform documentation](https://github.com/xmeye/openplatform-docs): vendor SDK/API overview.
- [XMCloudPlatform directory](https://github.com/xmeye/openplatform-docs/tree/master/docs/XMCloudPlatform): vendor documentation specifically supplied by the user.
- [XMCloudPlatform third-party interface](https://github.com/xmeye/openplatform-docs/blob/master/docs/XMCloudPlatform/XMCloudPlatform-ThirdInterface.md): alarm HTTP callbacks, not a complete P2P video handshake.
- [XM REST protocol specification](https://github.com/xmeye/openplatform-docs/blob/master/docs/DevelopersMustRead/DevelopersMustRead-Protocolspecifications.md): signed API requests and developer credentials; insufficient alone for a media transport.
- [Published NetSDK header](https://gist.github.com/fbstj/8cccdc7010d93510311b565295ff16d5): `H264_DVR_Login_Cloud` declaration; a declaration does not include the transport implementation or establish distribution rights for a vendor SDK.
- [PCAPdroid](https://github.com/emanuele-f/PCAPdroid) and [capture documentation](https://emanuele-f.github.io/PCAPdroid/quick_start): proposed authorized capture workflow and its limitations. No usable official-client session capture was supplied for completing P2P implementation.

## Dependencies and build documentation

| Source | Role |
| --- | --- |
| [Qt 6 documentation](https://doc.qt.io/qt-6/) | Native UI, network and test libraries |
| [Qt Windows deployment guide](https://doc.qt.io/qt-6/windows-deployment.html) | Qt runtime/plugin deployment and third-party dependency caveats |
| [FFmpeg documentation](https://ffmpeg.org/documentation.html) | Demuxing, decoding, remuxing and audio conversion libraries |
| [FFmpeg licensing information](https://ffmpeg.org/legal.html) | Build-dependent licensing context |
| [SDL2 documentation](https://wiki.libsdl.org/SDL2/FrontPage) | Default-speaker audio output |
| [OpenSSL documentation](https://docs.openssl.org/) | Cryptographic dependency for the device vault |
| [CMake command-line manual](https://cmake.org/cmake/help/latest/manual/cmake.1.html) | Configure/build command syntax |
| [CTest manual](https://cmake.org/cmake/help/latest/manual/ctest.1.html) | Automated test execution |
| [CPack manual](https://cmake.org/cmake/help/latest/manual/cpack.1.html) | Optional Linux package generation |
| [MSYS2](https://www.msys2.org/) and [environment guide](https://www.msys2.org/docs/environments/) | Native Windows UCRT64 toolchain setup |
| [MSYS2 Qt6 base package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-qt6-base) | Package and deployment-tool availability |
| [MSYS2 GitHub Actions setup](https://github.com/msys2/setup-msys2) | Existing experimental Windows CI environment |

## Project evidence and screenshots

[Validation history](validation.md), [protocol notes](protocol.md), [cloud investigation](xmeye-p2p.md) and the source/tests record what was implemented and checked. The project owner's manual feedback confirmed some local live viewing, PTZ and RTSP audio, and exposed recording-search failures. Those observations do not substitute for certification across devices.

The README uses six unmodified copies of owner-supplied images originally placed in the workspace's `outputs/images/` folder. They are distributed in the source repository's `images/` folder so relative Markdown links remain valid. Pre-existing red blocks in two screenshots were supplied with the images. These are project screenshots, not upstream vendor promotional images.
