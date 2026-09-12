package com.limelight.preferences;

/** Keeps the host launch rate separate from the stream rate requested by Warp. */
public enum FramePacingMode {
    LATENCY("latency", PreferenceConfiguration.FRAME_PACING_MIN_LATENCY, 1),
    BALANCED("balanced", PreferenceConfiguration.FRAME_PACING_BALANCED, 1),
    CAP_FPS("cap-fps", PreferenceConfiguration.FRAME_PACING_CAP_FPS, 1),
    SMOOTHNESS("smoothness", PreferenceConfiguration.FRAME_PACING_MAX_SMOOTHNESS, 1),
    WARP("warp", PreferenceConfiguration.FRAME_PACING_MIN_LATENCY, 2),
    WARP_2("warp2", PreferenceConfiguration.FRAME_PACING_MIN_LATENCY, 4);

    public final String preferenceValue;
    public final int renderingMode;
    private final int streamRateMultiplier;

    FramePacingMode(String preferenceValue, int renderingMode, int streamRateMultiplier) {
        this.preferenceValue = preferenceValue;
        this.renderingMode = renderingMode;
        this.streamRateMultiplier = streamRateMultiplier;
    }

    public static FramePacingMode fromPreference(String value) {
        for (FramePacingMode mode : values()) {
            if (mode.preferenceValue.equals(value)) {
                return mode;
            }
        }
        return LATENCY;
    }

    public FrameRates resolveRates(int selectedFps, int adjustedFps) {
        return new FrameRates(selectedFps, Math.multiplyExact(adjustedFps, streamRateMultiplier));
    }

    public static final class FrameRates {
        public final int launchFps;
        public final int streamFps;

        private FrameRates(int launchFps, int streamFps) {
            this.launchFps = launchFps;
            this.streamFps = streamFps;
        }
    }
}
