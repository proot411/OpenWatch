# WFS disk investigation

Objective: understand the owner's NVR disk and evaluate whether its indexes and media can inform OpenWatch playback. Disk extraction and DVRIP network playback are separate backends; disk metadata does not establish the network query format.

Observed from the owner's read-only command output: HGST HTS541010A9E680, 1,000,204,886,016 bytes, WFS0.4 at offset 0 and XM at offsets 510–511. A GPT signature at the last sector may be residual; it has not been validated as an active partition table. No full disk health or recording integrity conclusion follows.

Reference: Galileu Batista de Sousa and Unaldo de Oliveira Brito, *Analysis and recovery of videos from the WFS file system* (2022), DOI 10.22533/at.ed.216282205059, https://educapes.capes.gov.br/bitstream/capes/704325/1/ANALYSISAND.pdf . The paper describes a superblock at 0x3000, linked recording fragments and channel/time metadata. Compatibility with this recorder remains to be tested. No upstream code is copied.

`tools/inspect_wfs.py` opens a disk or sample read-only, reads exactly 64 KiB and optionally creates a new sample file. It refuses to overwrite existing output paths and reports candidate superblock values without following unvalidated pointers. This is a metadata probe, not a filesystem driver or playback implementation. Header samples may contain private metadata; keep them local and out of GitHub.

Next steps after collecting the header:

1. Check superblock signature, geometry, bounds and offset units against actual disk size.
2. Read bounded index samples and validate descriptor chains, channel identifiers, dates, and overwritten/stale entries.
3. Reconstruct one short recording from validated fragments, then check codec and timestamps against a matching NVR-exported clip.
4. Only after real-sample validation, consider a separate read-only disk/image playback backend. Use findings to investigate DVRIP searches without assuming disk channel encodings equal network channel numbers.

The collector passed synthetic checks for field reading, malformed/truncated input rejection, exact copying, existing-output rejection and source preservation. It has not yet read the real disk: the agent session lacks raw-device permission and cannot elevate through sudo. The user must run the collector locally.

## Real header findings (2026-09-21)

Analyzed 65,536 bytes, SHA256 `8ce4b851403d9ce68e35555ce89149cbe021dbd1ba2e65003c8ccebe1f183cf7`. No disk writes performed. The agent cannot open /dev/sdb in its sandbox, so analysis is limited to the copied sample.

- Packed date fields at 0x3010 and 0x3014 decode to 2026-09-11 11:38:09 and 2026-09-21 18:04:26. Exact semantics remain unverified.
- 0x302c and 0x3030 give 512 bytes and 4096 blocks: a candidate 2 MiB fragment.
- The published count field at 0x3020 is zero. The value at 0x304c is 476,053, a plausible alternate count.
- Interpreting 0x3044 and 0x3048 as sector addresses gives candidate index/data starts of 5,242,880 and 1,848,508,416 bytes. Data start plus 476,053 fragments ends 876,544 bytes before physical disk end. This supports, but does not prove, that interpretation.
- Signature bytes DE BC 9A 78 appear at 0x282c, outside the initially assumed superblock base.
- A primary EFI PART header exists at 0x200 with a valid header CRC. Its referenced partition-entry array fails its stored CRC. Along with WFS structures overlapping that array, this is consistent with residual GPT metadata; no repair should be attempted.

`tools/collect_wfs_samples.py` is specific to this snapshot and disk size. It verifies both before sampling seven bounded ranges (4,456,448 bytes total), including candidate index and fragment areas. It does not follow descriptor pointers or scan the entire disk. Outputs are new private files in a fresh directory. Samples may contain private footage and must not be published. No recovered recording, channel mapping, or playback support has been validated yet.

Validation: the actual header passed the collection plan; altered headers and wrong sizes were rejected; read bounds and short reads were checked. Physical reads still require the user's local administrator execution. The script gives only newly created sample files back to the invoking sudo user, without changing disk permissions.

## Index and media samples validated (2026-09-21)

All seven collected files matched their manifest hashes. The candidate index at disk byte 0x500000 contains 32-byte descriptors: 128 reserved, 746 primary, 56,662 continuation, and 8,000 zero-attribute entries in the captured 2 MiB. This supports the sector-based index location. Fragment links in this sample are fragment numbers, not byte offsets. The first byte of active records is 0x04, unlike the 0x00 described in the paper; do not reject records solely on that byte.

The three observed channel codes 0x02/0x06/0x0A map provisionally to channels 1/2/3. Their primary counts are 252/251/243. The earliest starts are September 11 at 11:38:09, 11:38:30, and 20:21:53 respectively. Latest indexed ends reach September 21 18:09:21. All 746 chains passed primary ownership, channel consistency, continuation ordinal, previous-link, count, bounds and cycle checks within the captured index. This validates chain structure, not footage integrity or active-versus-stale status. Header timestamps are earlier than some index entries; these fields must not yet be treated as global coverage limits.

The data mapping `data_start + fragment_number * fragment_size` produces Sofia-framed media at fragments 128/129. Bounded 1 MiB samples end within a frame; only complete frames were extracted. FFmpeg decoded 760 H.264 frames at 1280x720 and 105 HEVC frames at 640x720 without reporting errors. These samples are not encrypted at the media-payload level. This does not establish encryption status of all footage. Fragment 128 also includes 294 FA blocks; audio was not decoded.

`tools/analyze_wfs_index.py` reproduces the index analysis on copied files. It passed synthetic cycle, out-of-capture, owner/previous-link and misaligned-input tests. Private JSON/CSV inventories and extracted elementary-stream samples remain in the workspace under work/wfs-analysis, outside the source repository. No real footage is included in tests or published.

Implications: an independent read-only disk/image playback backend is now plausible. Remaining work includes full-fragment reconstruction, final-length validation, multi-fragment decode, seeking/keyframe indexing, audio interpretation, and stale-entry handling. The existing Sofia reader understands the tested frame subset. This does not fix OPFileQuery network searches: disk channel encoding must not be substituted into DVRIP requests without wire evidence. The user-reported channel-2 recording period is supported by the disk index, so an empty network reply cannot be taken as evidence of absent disk recordings.
