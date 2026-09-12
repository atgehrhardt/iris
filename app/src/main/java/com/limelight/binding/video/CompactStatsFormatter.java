package com.limelight.binding.video;

import java.util.Locale;

/** Compact presentation of existing measurements; unavailable values are never shown as zero. */
final class CompactStatsFormatter {
    private CompactStatsFormatter() {}

    static String format(Locale locale, String statsFormat, String trafficFormat,
                         double fps, int rttMs, double decodeMs, double lossPercent, double trafficMbps) {
        String stats = String.format(locale, statsFormat,
                number(locale, fps, 0), number(locale, rttMs, 0),
                number(locale, decodeMs, 1), number(locale, lossPercent, 1));
        if (Double.isFinite(trafficMbps) && trafficMbps >= 0) {
            stats += String.format(locale, trafficFormat, number(locale, trafficMbps, 1));
        }
        return stats;
    }

    private static String number(Locale locale, double value, int decimals) {
        if (!Double.isFinite(value) || value < 0) {
            return "—";
        }
        return String.format(locale, "%." + decimals + "f", value);
    }
}
