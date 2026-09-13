package com.limelight.binding.video;

import org.junit.Test;
import static org.junit.Assert.*;

public class AppTrafficRateTest {
    @Test
    public void firstSampleDoesNotReportLifetimeTrafficAsStreamBandwidth() {
        assertTrue(Double.isNaN(new AppTrafficRate().sample(10000000, 5000000, 1000)));
    }

    @Test
    public void combinesReceiveAndSendBytesAndUsesActualElapsedTime() {
        AppTrafficRate rate = new AppTrafficRate();
        rate.sample(100, 100, 1000);
        assertEquals(1.0, rate.sample(200100, 50100, 3000), 0.0001);
        assertEquals(0.0, rate.sample(200100, 50100, 4000), 0.0001);
    }

    @Test
    public void unsupportedCountersResetTheBaseline() {
        AppTrafficRate rate = new AppTrafficRate();
        rate.sample(100, 100, 1000);
        assertTrue(Double.isNaN(rate.sample(-1, 100, 2000)));
        assertTrue(Double.isNaN(rate.sample(1000000, 100, 3000)));
        assertTrue(Double.isNaN(rate.sample(1000000, -1, 4000)));
    }

    @Test
    public void resetsAndClockOrCounterDiscontinuitiesNeverProduceNegativeRates() {
        AppTrafficRate rate = new AppTrafficRate();
        rate.sample(1000, 1000, 1000);
        assertTrue(Double.isNaN(rate.sample(500, 500, 2000)));
        assertTrue(Double.isNaN(rate.sample(600, 600, 2000)));
        assertTrue(Double.isNaN(rate.sample(700, 700, 1500)));
        rate.reset();
        assertTrue(Double.isNaN(rate.sample(1000000, 1000000, 3000)));
    }
}
