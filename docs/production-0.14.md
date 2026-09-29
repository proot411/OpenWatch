# OpenWatch 0.14.0

Promotes the working local/cloud playback, read-only WFS disk playback, grouped device sidebar, camera controls, audio and deferred-save recording UI to production source. Network playback remains at 1× because accelerated requests were unreliable on the owner's recorder.

Network timeline: Find recordings reads OPFileQuery for the selected camera/day over the selected local or cloud connection. Teal ranges come from returned BeginTime/EndTime metadata, clipped to the day. Overlapping pagination entries are deduplicated; incomplete searches are labelled. Changing selection clears stale highlights. Manual seeking remains available even when firmware cannot return a file list. Search does not read or modify recorder disk contents.

Validation uses synthetic recording lists (gaps, duplicates, channel mismatch, midnight crossings), simulated recorder protocol tests and offscreen UI checks. Real-device availability reporting still requires owner verification. These changes do not claim compatibility with all XM firmware or Windows runtime verification.

Code authored by OpenAI Codex under the owner's direction. Protocol and third-party sources: [sources](sources.md), [playback research](XMEye-remote-playback-research.md), [CloudID notes](XMEye-CloudID-P2P.md). No proprietary binaries or packet captures are distributed.
