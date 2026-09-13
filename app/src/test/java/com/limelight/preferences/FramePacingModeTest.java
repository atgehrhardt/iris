package com.limelight.preferences;

import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;

public class FramePacingModeTest {
    @Test
    public void existingPreferencesKeepTheirRenderingBehaviorAndRate() {
        String[] values = {"latency", "balanced", "cap-fps", "smoothness"};
        int[] renderingModes = {PreferenceConfiguration.FRAME_PACING_MIN_LATENCY,
                PreferenceConfiguration.FRAME_PACING_BALANCED,
                PreferenceConfiguration.FRAME_PACING_CAP_FPS,
                PreferenceConfiguration.FRAME_PACING_MAX_SMOOTHNESS};
        for (int i = 0; i < values.length; i++) {
            FramePacingMode mode = FramePacingMode.fromPreference(values[i]);
            assertEquals(renderingModes[i], mode.renderingMode);
            assertEquals(60, mode.resolveRates(60, 60).launchFps);
            assertEquals(60, mode.resolveRates(60, 60).streamFps);
        }
    }

    @Test
    public void warpRequestsHigherStreamRatesWithoutChangingLaunchRate() {
        for (int fps : new int[] {30, 60, 90, 120, 144, 165, 240}) {
            FramePacingMode.FrameRates warp = FramePacingMode.fromPreference("warp").resolveRates(fps, fps);
            FramePacingMode.FrameRates warp2 = FramePacingMode.fromPreference("warp2").resolveRates(fps, fps);
            assertEquals(fps, warp.launchFps);
            assertEquals(fps * 2, warp.streamFps);
            assertEquals(fps, warp2.launchFps);
            assertEquals(fps * 4, warp2.streamFps);
        }
        assertEquals(PreferenceConfiguration.FRAME_PACING_MIN_LATENCY, FramePacingMode.WARP.renderingMode);
        assertEquals(PreferenceConfiguration.FRAME_PACING_MIN_LATENCY, FramePacingMode.WARP_2.renderingMode);
    }

    @Test
    public void cappedFpsAdjustmentKeepsTheOriginalLaunchRate() {
        FramePacingMode.FrameRates rates = FramePacingMode.CAP_FPS.resolveRates(60, 59);
        assertEquals(60, rates.launchFps);
        assertEquals(59, rates.streamFps);
    }

    @Test
    public void missingOrUnknownPreferencesUseTheExistingDefault() {
        assertSame(FramePacingMode.LATENCY, FramePacingMode.fromPreference(null));
        assertSame(FramePacingMode.LATENCY, FramePacingMode.fromPreference(""));
        assertSame(FramePacingMode.LATENCY, FramePacingMode.fromPreference("future-mode"));
    }

    @Test(expected = ArithmeticException.class)
    public void oversizedRatesCannotWrapToANegativeStreamRate() {
        FramePacingMode.WARP_2.resolveRates(Integer.MAX_VALUE, Integer.MAX_VALUE);
    }
}
