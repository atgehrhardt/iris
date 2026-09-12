package com.limelight.binding.video;

import org.junit.Test;
import java.util.Locale;
import static org.junit.Assert.*;

public class CompactStatsFormatterTest {
    private static final String STATS = "%1$s FPS · Net %2$s ms · Dec %3$s ms · Loss %4$s%%";
    private static final String TRAFFIC = " · %1$s Mbps";

    @Test
    public void normalMeasurementsFitOnOneLine() {
        String text = CompactStatsFormatter.format(Locale.US, STATS, TRAFFIC, 59.96, 4, 1.24, 0, 25.48);
        assertEquals("60 FPS · Net 4 ms · Dec 1.2 ms · Loss 0.0% · 25.5 Mbps", text);
        assertFalse(text.contains("\n"));
    }

    @Test
    public void missingTrafficIsOmittedAndUnknownMeasurementsAreNotZero() {
        String text = CompactStatsFormatter.format(Locale.US, STATS, TRAFFIC,
                Double.NaN, -1, Double.POSITIVE_INFINITY, Double.NaN, Double.NaN);
        assertEquals("— FPS · Net — ms · Dec — ms · Loss —%", text);
    }

    @Test
    public void zeroIsAValidMeasurement() {
        assertEquals("0 FPS · Net 0 ms · Dec 0.0 ms · Loss 0.0% · 0.0 Mbps",
                CompactStatsFormatter.format(Locale.US, STATS, TRAFFIC, 0, 0, 0, 0, 0));
    }

    @Test
    public void decimalFormattingUsesTheAppLocale() {
        String text = CompactStatsFormatter.format(Locale.GERMANY, STATS, TRAFFIC, 120, 3, 1.25, 0.5, 20.5);
        assertTrue(text.contains("1,3 ms"));
        assertTrue(text.contains("0,5%"));
        assertTrue(text.contains("20,5 Mbps"));
    }
}
