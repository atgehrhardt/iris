package com.limelight.binding.video;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Locale;
import java.util.Map;

/** Experimental Qualcomm options, isolated from Moonlight's normal retry sequence. */
final class DecoderLatencyPolicy {
    private DecoderLatencyPolicy() {}

    static boolean canTryEnhanced(boolean enabled, int sdkInt, String decoderName) {
        if (!enabled || sdkInt < 26 || decoderName == null) {
            return false;
        }
        String name = decoderName.toLowerCase(Locale.ROOT);
        return name.startsWith("c2.qti.") || name.startsWith("omx.qcom.");
    }

    static Map<String, Integer> enhancedOptions() {
        // Adapted from Artemis (ClassicOldSong/moonlight-android), c5cf27f4.
        // Qualcomm fence tuning contributed by alonsojr1980. See docs/latency-modes.md.
        Map<String, Integer> options = new LinkedHashMap<>();
        options.put("vendor.qti-ext-dec-low-latency.enable", 1);
        options.put("vendor.qti-ext-output-sw-fence-enable.value", 1);
        options.put("vendor.qti-ext-output-fence.enable", 1);
        options.put("vendor.qti-ext-output-fence.fence_type", 1);
        return Collections.unmodifiableMap(options);
    }

    enum Result { CONFIGURED, RETRY, EXHAUSTED }

    interface Attempt {
        Result configure(boolean enhanced, int tryNumber);
    }

    static boolean configure(boolean tryEnhanced, Attempt attempt) {
        if (tryEnhanced && attempt.configure(true, 0) == Result.CONFIGURED) {
            return true;
        }
        for (int tryNumber = 0; ; tryNumber++) {
            Result result = attempt.configure(false, tryNumber);
            if (result == Result.CONFIGURED) {
                return true;
            }
            if (result == Result.EXHAUSTED) {
                return false;
            }
        }
    }
}
