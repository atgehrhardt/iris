package com.limelight.nvstream.av.video;

import org.junit.Test;
import static org.junit.Assert.*;

/**

 * @brief Regression coverage for strict codec selection and version compatibility.

 */
public class PyroWaveFormatTest {
    /**
     * @brief Every requested color/chroma combination maps to exactly one wire mode.
     */
    @Test public void allModesRoundTrip() {
        for (int mode = 0; mode < 4; mode++) {
            int format = PyroWaveFormat.requested((mode & 1) != 0, (mode & 2) != 0);
            assertEquals(0x10000 << mode, format);
            assertEquals(mode, PyroWaveFormat.mode(format));
            assertTrue(PyroWaveFormat.supported(format, format << 8, true));
            assertFalse(PyroWaveFormat.supported(format, format << 8, false));
            assertFalse(PyroWaveFormat.supported(format, 0x0F000000 ^ (format << 8), true));
        }
    }
    /**
     * @brief Old peers, unsupported profiles, and fallback combinations cannot select PyroWave.
     */
    @Test public void rejectsAmbiguousOrForeignFormats() {
        for (int format : new int[] {0, 1, 0x100, 0x1000, 0x10001, 0x30000, 0xF0000, -1}) {
            assertFalse(PyroWaveFormat.supported(format, -1, true));
            assertThrows(IllegalArgumentException.class, () -> PyroWaveFormat.mode(format));
        }
    }
}
