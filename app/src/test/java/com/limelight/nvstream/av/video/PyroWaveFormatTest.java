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
    /**
     * @brief Bitrate follows pixels, frame rate, and chroma resolution, capped at the input range.
     */
    @Test public void bitrateScalesWithStreamShape() {
        assertEquals(233280, PyroWaveFormat.bitrateKbps(1920, 1080, 60, false));
        assertEquals(466560, PyroWaveFormat.bitrateKbps(1920, 1080, 120, false));
        assertEquals(699840, PyroWaveFormat.bitrateKbps(1920, 1080, 120, true));
        assertEquals(829440, PyroWaveFormat.bitrateKbps(2560, 1440, 120, false));
        assertEquals(1_000_000, PyroWaveFormat.bitrateKbps(3840, 2160, 120, false));
        assertEquals(1_000_000, PyroWaveFormat.bitrateKbps(16384, 16384, 1000, true));
    }
    /**
     * @brief Invalid stream shapes are rejected instead of requesting a zero or negative bitrate.
     */
    @Test public void bitrateRejectsInvalidShapes() {
        for (int[] shape : new int[][] {{0, 1080, 60}, {1920, 0, 60}, {1920, 1080, 0}, {-1, 1080, 60}}) {
            assertThrows(IllegalArgumentException.class, () -> PyroWaveFormat.bitrateKbps(shape[0], shape[1], shape[2], false));
        }
    }
}
