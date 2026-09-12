package com.limelight.binding.video;

/** Converts cumulative app RX/TX counters to Mbps using the actual sampling interval. */
final class AppTrafficRate {
    private long previousBytes = -1;
    private long previousTimeMs;

    double sample(long receivedBytes, long sentBytes, long nowMs) {
        if (receivedBytes < 0 || sentBytes < 0) {
            reset();
            return Double.NaN;
        }
        long bytes = receivedBytes + sentBytes;
        double rate = Double.NaN;
        if (previousBytes >= 0 && bytes >= previousBytes && nowMs > previousTimeMs) {
            rate = (bytes - previousBytes) * 8.0 / ((nowMs - previousTimeMs) * 1000.0);
        }
        previousBytes = bytes;
        previousTimeMs = nowMs;
        return rate;
    }

    void reset() {
        previousBytes = -1;
        previousTimeMs = 0;
    }
}
