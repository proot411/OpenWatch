# User manual

For an entire XMEye NVR/DVR, click **XMEye recorder** in the sidebar. Enter its IP, port, username and password, select Substream or Main stream, and press **Scan and play all**. A successful scan replaces the current grid, automatically chooses its size and starts every advertised channel, up to 64. Failed scans leave the current grid untouched. Cancel stops the scan. Serial-number cloud access is disabled for this local-first release. Empty recorder inputs may show errors; channel enumeration does not detect whether a camera is physically present.

1. Choose **Add device**. Give the camera a descriptive name and enter its stream URL. Supply credentials in the separate fields. Use Save devices to retain it in a passphrase-encrypted file.
2. For XM, use `dvrip://192.168.1.10:34567?channel=0&subtype=0`. Channels are zero-based; subtype 0 is Main, 1 is Extra1. The default username when omitted is admin, with an empty password. Supply your actual device credentials.
3. For RTSP, use the camera's documented RTSP path, for example `rtsp://192.168.1.10:554/stream1`. For an ONVIF device, use Discover devices to resolve its stream paths.
4. Select a grid cell, then double-click a device, or drag a device into a cell. The selected cell has a green border. Change the layout using the channel-count menu. Reducing the grid disconnects hidden cells, ending their recordings.
5. Mouse-wheel zoom enlarges around the pointer inside the video. Aspect ratio is preserved. F11 toggles fullscreen for the workspace, and Escape exits it; select one channel for a single-view workspace.
6. **Snapshot** saves the selected channel's current frame as PNG. **Record** selects an MKV path and waits for the next keyframe with usable timestamps. Check the cell's status for the Recording indicator. **Stop recording** finalizes the file when the next packet arrives. Disconnecting or closing also finalizes it. Recording errors stop that stream and appear in the cell status.

Recordings contain video only. No automatic retention exists: monitor available disk space yourself. An interrupted/crashed application can leave an incomplete recording. Interrupted live connections retry automatically after 1, 2, 4, 8, 16 and then 30 seconds. A stable connection resets the delay. Authentication rejection and unsupported protocols/codecs require correcting the settings; these are not repeatedly retried. Click Disconnect to stop a channel and its retries. Local footage never loops or reconnects automatically. Connection shutdown may briefly wait for network deadlines. Recording the same camera in multiple cells uses separate connections.

Credentials and URLs reside in process memory while used. Save devices stores them only in an encrypted file. FFmpeg logging is disabled in the GUI to avoid printing credential-bearing URLs. Unencrypted network protocols remain unencrypted; the Sofia password digest is not a secure transport. Cloud serial numbers cannot be used as addresses.

## Channel navigation (v0.3)

Double-click a video cell to enlarge that specific channel. Double-click again or press Escape to restore the grid. Left/Right selects the previous/next assigned channel, skipping empty cells and wrapping at the ends; in focused view it changes the enlarged channel. Other channels continue playing and recording in the background. Explicitly reducing the layout with the channel-count menu still disconnects cells outside that layout. F11 remains fullscreen for the whole workspace.

After a dropped connection, recording resumes at a keyframe in a sibling file named `original-reconnect-<time>-<unique ID>.mkv`. The previous segment is finalized and preserved. There is a recording gap during the outage. Stop recording clears the recording request even while a channel is retrying. Disconnect stops both streaming and retrying. Network error detection can take several seconds before the retry countdown appears.

## Local device discovery (v0.4)

1. Click **Discover devices** in the sidebar, choose a network interface (or leave All active IPv4 interfaces), and click **Scan LAN**.
2. Select an XM or ONVIF result. One physical device may advertise both protocols and appear twice; choose either service.
3. Enter credentials and click **Load channels / profiles**. Some cameras require enabling ONVIF and creating an ONVIF-specific account in their own settings.
4. Check the desired streams, then choose **Open selected streams**. At most 64 can be opened together. For XM, substreams are selected by default. For ONVIF, the first advertised profile per reported video source is initially selected; adjust the checkboxes for main/substream preference. If a source token is absent, only the first profile is preselected.
5. If discovery finds nothing, verify the selected network, camera settings and firewall. You can manually enter `dvrip://192.168.1.10:34567` or the camera's actual ONVIF device-service URL, often `http://192.168.1.10/onvif/device_service`.

Discovery is limited to responding devices on the local IPv4 network. It does not scan arbitrary RTSP paths or route across VLANs automatically. XML/SOAP redirects, invalid TLS certificates and returned service/stream hosts different from the selected device are rejected; enter the actual target manually when needed. Cancel stops discovery or an active lookup. Multi-language support is intentionally skipped.

## Remove or edit a device (v0.5)

Select the camera/stream entry in the sidebar, then click **Remove**. You can also right-click it and choose Remove, or press Delete with the device list focused. All cells using that entry are disconnected and cleared, including hidden or reconnecting views. If the removed entry was enlarged, the grid returns. Existing saved recordings remain on disk. Other entries, including other NVR channels, remain in the list.

**Edit** lets you rename an entry or change its URL, username and password. Passwords are masked and the URL field omits embedded credentials. For ONVIF, you edit the resolved media stream; use Discover devices to reload the device's profiles. Renaming keeps playback and recording running. Changing connection details restarts matching views and finalizes their current recordings. Save devices after edits if you want to retain the changes.

## Device groups and saved lists (v0.7)

Select an entry, click **Assign group**, and enter a name. An empty name removes its group. The group menu and name/address search filter the sidebar without disconnecting cameras. Group membership is per stream, including individual NVR channels.

**Save devices** writes an encrypted `.owv` file. Enter and confirm a passphrase of at least 12 characters. Keep it somewhere safe: OpenWatch does not store it and cannot recover it. **Load devices** asks for the passphrase and replaces the current list only after successful decryption and validation. Existing views stop; saved footage remains. Double-click or drag the loaded entries to reconnect. Changes are not automatically saved.

## Camera controls (v0.7)

Select a video cell, then **Camera controls**. The dialog title identifies the target. With an empty cell selected, the current sidebar entry is used. For XM, its URL's channel number is the PTZ target. Click an arrow once for a short move; click again for another. Optical zoom, focus and iris need hardware support. The speed selector applies to XM. Select a preset number, then Go to, Save position (overwrites that numbered position), or Delete preset. Stop sends an explicit stop command. A firmware rejection appears in the dialog; it is not retried automatically.

For ONVIF, check the service URL, click **Load ONVIF controls**, and select the camera profile. The discovered service URL is retained; a manually added RTSP camera may use a different ONVIF port or path. The stream credentials must also have ONVIF permissions. Pan/tilt and zoom are enabled independently when the profile advertises compatible standard velocity spaces. Each move requests a 0.2-second device timeout and a subsequent Stop. The preset list comes from the camera; Save position creates a named preset. Devices without PTZ remain usable for live video.

**Device information** reads model/firmware information for XM or ONVIF. **Set recorder time from this computer** is XM-only and changes the recorder's clock for all channels; its confirmation explains the time-zone requirement. Network settings, motion zones, recording schedules, user roles, PTZ tours and two-way audio remain future work.

**Reset zoom** returns the selected video to its fitted view. Digital zoom works on fixed cameras and does not move hardware. **Open footage** is removed for now; no local-file playback action is available in this release. Existing manual recording and snapshot actions are retained.

## Cameras reporting no PTZ configuration (v0.7.1)

Use the camera's ONVIF port in the service address. The tested V380 Pro cameras respond at `http://CAMERA_IP:8899/onvif/device_service`. Click Load ONVIF controls after changing the address. This update requests full Media2 configurations, avoiding false reports of missing PTZ configuration. Try normal mode first. If a known movable camera still returns incomplete configuration data, enable Compatibility mode and try one direction click. It does not configure presets or attach camera configurations automatically.

The Stop request still follows a 200 ms pulse, while the requested device timeout is adjusted to its advertised range. Missing preset support does not establish that pan/tilt is unavailable. Advertised zoom or speed support may differ from physical firmware behavior.

## Workspace PTZ shortcuts and live audio (v0.8)

1. Select a channel and open Camera controls. For ONVIF, load controls and choose the desired profile.
2. Close the controls dialog. The loaded profile stays available for that device during this app session.
3. With the workspace active, use Alt + Up/Down/Left/Right to move, Alt + Page Up/Page Down to zoom, and Alt + End to send Stop. Each press produces one pulse. Repeated key events are ignored; wait for the current command before pressing again. Plain Left/Right still change selected channels.
4. Select another channel to direct shortcuts to that channel's loaded controls. An unloaded channel displays a message instead of controlling the previous camera.

Use Audio: on to hear the selected RTSP camera or NVR RTSP channel. Audio begins muted, follows the selected cell, and the adjacent slider sets volume. The audio status appears below these controls. A stream with no audio track reports that explicitly. The default system speaker output is used; change your system output if needed.

This release does not capture microphone audio, implement two-way talk, add audio to recordings or decode audio from the direct DVRIP transport. Add the NVR channel's RTSP stream when you need sound. PTZ tours are skipped at the user's request.

## NVR playback removed (v0.11.3)

The experimental NVR playback workspace has been removed from the application at the user's request because recording searches remained unreliable on their NVR. Use the NVR's own playback interface or phone app to review stored footage. Live viewing, camera controls, live RTSP audio, snapshots and manual recording remain available.

The experimental archive source and tests are retained for reference, but the playback dialog is not built into the application or accessible through its interface.
