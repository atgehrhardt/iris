# Experimental latency modes

In **Settings → Basic Settings**, Iris offers three opt-in options. Existing
settings and the default **Prefer lowest latency** behavior are preserved.

| Option | Behavior |
| --- | --- |
| Qualcomm ultra-low latency | Attempts extra Qualcomm decoder and software-fence flags before the normal decoder configuration sequence. |
| Warp Drive | Uses lowest-latency rendering and requests a stream rate of twice the selected FPS. |
| Warp 2 | Uses lowest-latency rendering and requests a stream rate of four times the selected FPS. |

Qualcomm tuning is independent of frame pacing and defaults off. Iris attempts
it only on Android 8+ with a `c2.qti.*` or `omx.qcom.*` decoder. This identifies
eligible decoders, not guaranteed hardware support. Firmware may silently ignore
individual flags even when configuration succeeds. Logs distinguish the
requested options, successful configuration, and fallback. If configuration or
startup fails, Iris releases the failed codec and retries the existing Moonlight
configurations using fresh formats without the added fence flags. Runtime codec
recovery continues to use the existing Moonlight recovery logic; disable the
option and reconnect if playback becomes unstable after successful setup.

Warp modes work independently of the decoder vendor. For selected 60 FPS, Warp
Drive requests 120 FPS and Warp 2 requests 240 FPS. The host launch request remains
60 FPS, and display refresh selection and configured bitrate stay unchanged.
The host may not deliver the requested rate. Higher requested rates can increase
host workload, reduce per-frame quality at a fixed bitrate, or introduce stutter.
If the host rejects the request, the normal connection error is shown; select
another pacing mode and reconnect. No host protocol changes are required.

## Validation

Build and run the automated checks with:

```shell
./gradlew lintNonRootDebug testNonRootDebugUnitTest assembleNonRootDebug \
  testNonRootReleaseUnitTest assembleNonRootRelease
```

For device validation, compare baseline, Qualcomm tuning alone, each Warp mode
alone, and both Warp modes with Qualcomm tuning enabled. Keep resolution,
bitrate, network, host workload, and display settings constant. Include H.264,
HEVC, supported AV1/HDR, reconnects, and background/resume. Confirm controller
navigation can reach and change all three settings.

Record the device, Android version, codec name, host OS, requested/received FPS,
frame drops, host load, thermal behavior, visible stutter, and input-to-display
response. Use repeated runs and high-speed camera measurements for latency
claims; overlay decode time alone does not establish end-to-end improvement.

The target matrix is Snapdragon 8 Gen 2 and Gen 3/Elite, an older Qualcomm
device, and a non-Qualcomm control, with Prism on Linux and Windows. Hardware
validation is pending; no measured latency improvement is claimed for Iris.

## Provenance

These features selectively adapt the GPL-3.0 Artemis implementation at
[`c5cf27f4dc822db0e863c4691e7a70c74bea977a`](https://github.com/ClassicOldSong/moonlight-android/tree/c5cf27f4dc822db0e863c4691e7a70c74bea977a).
Credit to ClassicOldSong and Artemis contributors, including **alonsojr1980**
for the Qualcomm decoder/software-fence work credited in the
[ultra-low-latency release](https://github.com/ClassicOldSong/moonlight-android/releases/tag/v12.1.250415).
The reference behavior is in `MediaCodecHelper`, `PreferenceConfiguration`, and
`Game`. Iris retains its GPL-3.0 license and selectively maintains these changes;
Moonlight remains the automatic upstream source.
