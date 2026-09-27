package com.limelight.nvstream.av.video;

public abstract class VideoDecoderRenderer {
    /**
     * @brief Set the Android presentation surface before stream setup.
     */
    public abstract void setRenderTarget(android.view.SurfaceHolder target);
    /**
     * @brief Stop accepting frames before connection teardown.
     */
    public abstract void prepareForStop();
    /**
     * @brief Resume presentation after foregrounding.
     */
    public abstract void notifyVideoForeground();
    /**
     * @brief Suspend presentation while the activity is backgrounded.
     */
    public abstract void notifyVideoBackground();
    /**
     * @brief Return the negotiated codec identifier.
     */
    public abstract int getActiveVideoFormat();
    /**
     * @brief Return the renderer's preferred SDR matrix.
     */
    public abstract int getPreferredColorSpace();
    /**
     * @brief Return the renderer's preferred signal range.
     */
    public abstract int getPreferredColorRange();
    /**
     * @brief Return average client latency, or zero when unavailable.
     */
    public abstract int getAverageEndToEndLatency();
    /**
     * @brief Return measured decoder latency, or zero when unavailable.
     */
    public abstract int getAverageDecoderLatency();
    /**
     * @brief Report support for H.264.
     */
    public boolean isAvcSupported() { return false; }
    /**
     * @brief Report support for HEVC SDR.
     */
    public boolean isHevcSupported() { return false; }
    /**
     * @brief Report support for HEVC HDR10.
     */
    public boolean isHevcMain10Hdr10Supported() { return false; }
    /**
     * @brief Report support for AV1 SDR.
     */
    public boolean isAv1Supported() { return false; }
    /**
     * @brief Report support for AV1 HDR10.
     */
    public boolean isAv1Main10Supported() { return false; }

    public abstract int setup(int format, int width, int height, int redrawRate);

    public abstract void start();

    public abstract void stop();

    // This is called once for each frame-start NALU. This means it will be called several times
    // for an IDR frame which contains several parameter sets and the I-frame data.
    public abstract int submitDecodeUnit(byte[] decodeUnitData, int decodeUnitLength, int decodeUnitType,
                                         int frameNumber, int frameType, char frameHostProcessingLatency,
                                         long receiveTimeUs, long enqueueTimeUs);
    
    public abstract void cleanup();

    public abstract int getCapabilities();

    public abstract void setHdrMode(boolean enabled, byte[] hdrMetadata);
}
