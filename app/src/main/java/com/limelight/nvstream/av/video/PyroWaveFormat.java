package com.limelight.nvstream.av.video;

/**
 * @brief Pure policy for strict version-one PyroWave negotiation.
 */
public final class PyroWaveFormat {
    private static final int MASK = 0xF0000; ///< Native format bits reserved by the protocol.

    /**
     * @brief Prevent construction of this static policy class.
     */
    private PyroWaveFormat() {}

    /**
     * @brief Choose exactly one codec mode without advertising lower-quality alternatives.
     * @param hdr Request HDR10.
     * @param fullChroma Request full-resolution chroma.
     * @return Native client format bit.
     */
    public static int requested(boolean hdr, boolean fullChroma) {
        return 0x10000 << ((hdr ? 1 : 0) + (fullChroma ? 2 : 0));
    }

    /**
     * @brief Check both the independent protocol version and the exact requested server mode.
     * @param requested Native requested mode, without other codecs or fallback modes.
     * @param serverModes ServerCodecModeSupport bitset.
     * @param versionOne Whether discovery advertised version one.
     * @return True when this exact mode can be negotiated.
     */
    public static boolean supported(int requested, int serverModes, boolean versionOne) {
        return versionOne && requested != 0 && (requested & MASK) == requested &&
                (requested & (requested - 1)) == 0 && (serverModes & (requested << 8)) != 0;
    }

    /**
     * @brief Convert a negotiated format to envelope flags.
     * @param format Exactly one PyroWave codec mode.
     * @return Bit zero HDR and bit one 4:4:4.
     * @throws IllegalArgumentException When the format is ambiguous or unsupported.
     */
    public static int mode(int format) {
        if (!supported(format, 0x0F000000, true)) throw new IllegalArgumentException("Invalid PyroWave codec mode");
        return Integer.numberOfTrailingZeros(format) - 16;
    }
}
