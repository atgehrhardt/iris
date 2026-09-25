package com.limelight.binding.video;

import android.content.Context;
import android.os.Build;
import android.os.SystemClock;
import android.view.SurfaceHolder;

import com.limelight.R;
import com.limelight.LimeLog;
import com.limelight.nvstream.av.video.VideoDecoderRenderer;
import com.limelight.nvstream.av.video.PyroWaveFormat;
import com.limelight.nvstream.jni.MoonBridge;

import java.util.function.Consumer;

/**
 * @brief Strict PyroWave renderer with serialized native ownership and bounded frame submission.
 */
public final class PyroWaveDecoderRenderer extends VideoDecoderRenderer {
    private SurfaceHolder target; ///< Surface owned by the activity.
    private long handle; ///< Owned native renderer, or zero after cleanup.
    private int format; ///< Negotiated native codec identifier.
    private boolean stopped; ///< Prevent submissions during connection shutdown.
    private boolean background; ///< Drop frames while presentation is suspended.
    private boolean failed; ///< Deliver one terminal failure per session.
    private boolean hdr; ///< Explicitly requested HDR mode.
    private byte[] hdrMetadata; ///< Latest host static HDR metadata.
    private long decodeTimeUs; ///< Accumulated measured GPU time.
    private long decodeSamples; ///< Number of completed GPU measurements.
    private long windowDecodeTimeUs; ///< GPU time in the current overlay window.
    private long windowDecodeSamples; ///< Completed GPU measurements in the current window.
    private final AppTrafficRate trafficRate = new AppTrafficRate(); ///< App traffic sampled for the overlay.
    private final Context context; ///< Application context for localized overlay text.
    private final boolean liteOverlay; ///< Whether to use the existing compact overlay.
    private long received; ///< Received complete frames in the current stats window.
    private long lost; ///< Missing frame numbers in the current stats window.
    private int previousFrame; ///< Most recent received frame number.
    private long submitted; ///< Frames submitted to the GPU.
    private long dropped; ///< Frames dropped because presentation was busy.
    private long lastUpdate; ///< Last performance overlay update time.
    private final Consumer<String> failure; ///< Activity-safe terminal error callback.
    private final boolean lowLatency; ///< Presentation policy inherited from frame-pacing settings.
    private final PerfOverlayListener overlay; ///< Existing performance overlay callback.

    /**
     * @brief Load the optional Vulkan renderer only on supported Android ABIs and API levels.
     * @return True when native dependencies can be loaded; GPU and surface checks run during setup.
     */
    public static boolean isAvailable() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return false;
        try {
            System.loadLibrary("iris-pyrowave");
            return true;
        } catch (UnsatisfiedLinkError | SecurityException error) {
            LimeLog.warning("Unable to load PyroWave native renderer: " + error);
            return false;
        }
    }

    /**
     * @brief Construct a renderer without creating a GPU device before a surface exists.
     * @param context Context supplying localized performance labels.
     * @param liteOverlay Use the existing compact performance bar.
     * @param failure Terminal error callback; must dispatch activity operations to the UI thread.
     * @param overlay Optional performance overlay listener.
     * @param lowLatency Prefer mailbox presentation for latency-oriented pacing.
     */
    public PyroWaveDecoderRenderer(Context context, boolean liteOverlay, Consumer<String> failure, PerfOverlayListener overlay, boolean lowLatency) {
        this.context = context.getApplicationContext();
        this.liteOverlay = liteOverlay;
        this.lowLatency = lowLatency;
        this.failure = failure;
        this.overlay = overlay;
    }

    /**
     * @brief Retain the surface for native setup.
     */
    @Override public synchronized void setRenderTarget(SurfaceHolder target) { this.target = target; }

    /**
     * @brief Create the requested native renderer or fail the connection without codec substitution.
     * @param format Exactly one negotiated PyroWave format.
     * @param width Stream luma width.
     * @param height Stream luma height.
     * @param redrawRate Actual stream rate, including any Warp multiplier.
     * @return Zero on success; negative on failure.
     */
    @Override public synchronized int setup(int format, int width, int height, int redrawRate) {
        this.format = format;
        hdr = (format & (MoonBridge.VIDEO_FORMAT_PYROWAVE_HDR420 | MoonBridge.VIDEO_FORMAT_PYROWAVE_HDR444)) != 0;
        try {
            if (stopped) throw new IllegalStateException("Presentation surface was destroyed before PyroWave setup");
            if (target == null || !target.getSurface().isValid()) {
                throw new IllegalStateException("PyroWave presentation surface is unavailable");
            }
            if (!PyroWaveFormat.supported(format, 0x0F000000, true)) {
                throw new IllegalStateException("Expected PyroWave but negotiated codec 0x" + Integer.toHexString(format));
            }
            if (!isAvailable()) throw new IllegalStateException("PyroWave native library could not be loaded");
            handle = nativeCreate(target.getSurface(), width, height, PyroWaveFormat.mode(format), lowLatency);
            if (hdrMetadata != null) nativeHdr(handle, hdrMetadata);
            return 0;
        } catch (RuntimeException | LinkageError error) {
            reportFailure(error);
            cleanup();
            return -1;
        }
    }

    /**
     * @brief Native setup already creates the resources required for streaming.
     */
    @Override public void start() {}
    /**
     * @brief Prevent further frame submissions before teardown.
     */
    @Override public synchronized void prepareForStop() { stopped = true; }
    /**
     * @brief Release the native renderer once submissions have stopped.
     */
    @Override public synchronized void stop() { stopped = true; cleanup(); }
    /**
     * @brief Release resources exactly once, including after partial setup failure.
     */
    @Override public synchronized void cleanup() {
        if (handle != 0) { nativeDestroy(handle); handle = 0; }
    }
    /**
     * @brief Resume presentation, preserving the negotiated codec.
     */
    @Override public synchronized void notifyVideoForeground() { background = false; }
    /**
     * @brief Drop frames without building a decode queue while in the background.
     */
    @Override public synchronized void notifyVideoBackground() {
        background = true;
        previousFrame = 0;
        lastUpdate = received = lost = submitted = dropped = 0;
        windowDecodeTimeUs = windowDecodeSamples = 0;
        trafficRate.reset();
    }
    /**
     * @brief Return the codec selected by RTSP negotiation.
     */
    @Override public int getActiveVideoFormat() { return format; }
    /**
     * @brief PyroWave SDR uses the BT.709 matrix.
     */
    @Override public int getPreferredColorSpace() { return MoonBridge.COLORSPACE_REC_709; }
    /**
     * @brief PyroWave GPU conversion produces full-range components.
     */
    @Override public int getPreferredColorRange() { return MoonBridge.COLOR_RANGE_FULL; }
    /**
     * @brief Presentation completion timing is not currently measured.
     */
    @Override public int getAverageEndToEndLatency() { return 0; }
    /**
     * @brief Return the average measured GPU decode duration, excluding presentation.
     */
    @Override public synchronized int getAverageDecoderLatency() { return decodeSamples == 0 ? 0 : (int)(decodeTimeUs / decodeSamples / 1000); }
    /**
     * @brief Use the bounded native video decode queue without reference-frame recovery capabilities.
     */
    @Override public int getCapabilities() { return 0; }

    /**
     * @brief Preserve strict HDR selection and apply updated host metadata.
     * @param enabled Host HDR state.
     * @param metadata Moonlight SS_HDR_METADATA bytes.
     */
    @Override public synchronized void setHdrMode(boolean enabled, byte[] metadata) {
        if (stopped || failed) return;
        hdrMetadata = metadata == null ? null : metadata.clone();
        if (handle == 0) return;
        try {
            if (enabled != hdr || (hdr && metadata == null)) throw new IllegalStateException("Host HDR mode does not match the requested PyroWave mode");
            if (hdr) nativeHdr(handle, metadata);
        } catch (RuntimeException error) { reportFailure(error); }
    }

    /**
     * @brief Submit one complete frame; discard independent frames when the GPU/display is busy.
     * @return DR_OK; terminal native errors are delivered separately to the activity.
     */
    @Override public synchronized int submitDecodeUnit(byte[] data, int length, int type,
            int frameNumber, int frameType, char hostLatency, long receiveTimeUs, long enqueueTimeUs) {
        if (stopped || failed || background || handle == 0) return MoonBridge.DR_OK;
        try {
            if (previousFrame != 0 && frameNumber > previousFrame) lost += Math.max(0, frameNumber - previousFrame - 1);
            previousFrame = frameNumber;
            received++;
            if (nativeSubmit(handle, data, length)) submitted++;
            else dropped++;
            long sample = nativeTakeDecodeTimeUs(handle);
            if (sample >= 0) {
                decodeTimeUs += sample;
                decodeSamples++;
                windowDecodeTimeUs += sample;
                windowDecodeSamples++;
            }
            long now = SystemClock.elapsedRealtime();
            if (lastUpdate == 0) lastUpdate = now;
            if (overlay != null && now - lastUpdate >= 1000) {
                long rtt = MoonBridge.getEstimatedRttInfo();
                double gpuMs = windowDecodeSamples == 0 ? Double.NaN : windowDecodeTimeUs / (windowDecodeSamples * 1000.0);
                int uid = android.os.Process.myUid();
                double trafficMbps = trafficRate.sample(android.net.TrafficStats.getUidRxBytes(uid),
                        android.net.TrafficStats.getUidTxBytes(uid), now);
                String stats = CompactStatsFormatter.format(context.getResources().getConfiguration().locale,
                        context.getString(R.string.perf_overlay_lite_stats),
                        context.getString(R.string.perf_overlay_lite_traffic),
                        received * 1000.0 / (now - lastUpdate), (int)(rtt >> 32), gpuMs,
                        lost * 100.0 / Math.max(1, received + lost), trafficMbps);
                if (!liteOverlay) {
                    String mode = (hdr ? "HDR10" : "SDR") + ((PyroWaveFormat.mode(format) & 2) != 0 ? " 4:4:4" : " 4:2:0");
                    stats = context.getString(R.string.perf_overlay_decoder, "PyroWave " + mode) + "\n" + stats + "\n" +
                            context.getString(R.string.pyrowave_perf_presentation,
                                    submitted * 1000.0 / (now - lastUpdate), dropped);
                }
                overlay.onPerfUpdate(stats);
                received = lost = submitted = dropped = 0;
                windowDecodeTimeUs = windowDecodeSamples = 0;
                lastUpdate = now;
            }
        } catch (RuntimeException error) { reportFailure(error); }
        return MoonBridge.DR_OK;
    }

    /**
     * @brief Report one terminal error without stopping the network from its own callback thread.
     * @param error Native initialization, decoding, or presentation failure.
     */
    private void reportFailure(Throwable error) {
        if (!failed) {
            failed = true;
            LimeLog.warning("PyroWave renderer failed: " + error);
            failure.accept("PyroWave: " + error.getMessage());
        }
    }

    /**
     * @brief Create and validate native Vulkan resources.
     */
    private static native long nativeCreate(android.view.Surface surface, int width, int height, int mode, boolean lowLatency);
    /**
     * @brief Submit a complete bounded envelope to the GPU.
     */
    private static native boolean nativeSubmit(long handle, byte[] data, int length);
    /**
     * @brief Update HDR10 presentation metadata.
     */
    private static native void nativeHdr(long handle, byte[] metadata);
    /**
     * @brief Consume one completed GPU decode timing sample.
     */
    private static native long nativeTakeDecodeTimeUs(long handle);
    /**
     * @brief Destroy the native renderer.
     */
    private static native void nativeDestroy(long handle);
}
