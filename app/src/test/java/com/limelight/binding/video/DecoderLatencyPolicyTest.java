package com.limelight.binding.video;

import org.junit.Test;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

import static com.limelight.binding.video.DecoderLatencyPolicy.Result.*;
import static org.junit.Assert.*;

public class DecoderLatencyPolicyTest {
    @Test
    public void tuningRequiresOptInAndAndroidVendorExtensionSupport() {
        assertFalse(DecoderLatencyPolicy.canTryEnhanced(false, 36, "c2.qti.hevc.decoder"));
        assertFalse(DecoderLatencyPolicy.canTryEnhanced(true, 25, "OMX.qcom.video.decoder.avc"));
        assertTrue(DecoderLatencyPolicy.canTryEnhanced(true, 26, "OMX.qcom.video.decoder.avc"));
        assertTrue(DecoderLatencyPolicy.canTryEnhanced(true, 36, "c2.qti.hevc.decoder.low_latency"));
        assertTrue(DecoderLatencyPolicy.canTryEnhanced(true, 36, "C2.QTI.AV1.DECODER"));
    }

    @Test
    public void otherVendorsAndSoftwareDecodersKeepNormalConfiguration() {
        for (String name : new String[] {null, "", "c2.android.avc.decoder", "c2.mtk.hevc.decoder",
                "OMX.Exynos.HEVC.Decoder", "OMX.Nvidia.h264.decode", "c2.qtiOther.decoder"}) {
            assertFalse(DecoderLatencyPolicy.canTryEnhanced(true, 36, name));
        }
    }

    @Test
    public void softwareFencingIncludesBothQualcommExtensionFamilies() {
        Map<String, Integer> options = DecoderLatencyPolicy.enhancedOptions();
        assertEquals(Integer.valueOf(1), options.get("vendor.qti-ext-dec-low-latency.enable"));
        assertEquals(Integer.valueOf(1), options.get("vendor.qti-ext-output-sw-fence-enable.value"));
        assertEquals(Integer.valueOf(1), options.get("vendor.qti-ext-output-fence.enable"));
        assertEquals(Integer.valueOf(1), options.get("vendor.qti-ext-output-fence.fence_type"));
    }

    @Test
    public void successfulEnhancedAttemptDoesNotReconfigureTheDecoder() {
        List<String> attempts = new ArrayList<>();
        assertTrue(DecoderLatencyPolicy.configure(true, (enhanced, number) -> {
            attempts.add(enhanced + ":" + number);
            return CONFIGURED;
        }));
        assertEquals(Arrays.asList("true:0"), attempts);
    }

    @Test
    public void rejectedEnhancedAttemptRestartsNormalOptionsAtZero() {
        List<String> attempts = new ArrayList<>();
        assertTrue(DecoderLatencyPolicy.configure(true, (enhanced, number) -> {
            attempts.add(enhanced + ":" + number);
            return !enhanced && number == 2 ? CONFIGURED : RETRY;
        }));
        assertEquals(Arrays.asList("true:0", "false:0", "false:1", "false:2"), attempts);
    }

    @Test
    public void disabledTuningRetainsTheNormalRetrySequence() {
        List<String> attempts = new ArrayList<>();
        assertTrue(DecoderLatencyPolicy.configure(false, (enhanced, number) -> {
            attempts.add(enhanced + ":" + number);
            return number == 1 ? CONFIGURED : RETRY;
        }));
        assertEquals(Arrays.asList("false:0", "false:1"), attempts);
    }

    @Test
    public void failureStopsAfterTheUnadornedConfigurationFails() {
        List<String> attempts = new ArrayList<>();
        assertFalse(DecoderLatencyPolicy.configure(true, (enhanced, number) -> {
            attempts.add(enhanced + ":" + number);
            return enhanced || number < 5 ? RETRY : EXHAUSTED;
        }));
        assertEquals(Arrays.asList("true:0", "false:0", "false:1", "false:2", "false:3", "false:4", "false:5"), attempts);
    }

    @Test(expected = IllegalStateException.class)
    public void terminalRecoveryExceptionsPropagate() {
        DecoderLatencyPolicy.configure(false, (enhanced, number) -> {
            throw new IllegalStateException("Recovery exhausted");
        });
    }
}
