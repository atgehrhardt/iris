package com.limelight.nvstream.av.video;

/**
 * @brief Pure policy for strict version-one PyroWave negotiation.
 */
public final class PyroWaveFormat {
    private static final int MASK = 0xF0000; ///< Native format bits reserved by the protocol.
    private static final long MAXIMUM_BITRATE_KBPS = 1_000_000; ///< Largest bitrate Iris can request.

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
     * @brief Choose PyroWave's bitrate from the stream shape instead of the inter-frame codec setting.
     *
     * Every PyroWave frame is coded independently, so quality depends on bits per pixel rather than
     * on how much the image changes. Targets 1.5 bits per pixel per frame for 4:2:0 and 2.25 for
     * 4:4:4, plus 25% for the FEC parity, audio, and packet overhead the host deducts before encoding.
     * @param width Stream width in pixels.
     * @param height Stream height in pixels.
     * @param fps Selected stream frame rate.
     * @param fullChroma Whether full-resolution chroma is requested.
     * @return Requested bitrate in Kbps, capped at 1000 Mbps.
     * @throws IllegalArgumentException When a dimension or the frame rate is not positive.
     */
    public static int bitrateKbps(int width, int height, int fps, boolean fullChroma) {
        if (width <= 0 || height <= 0 || fps <= 0) throw new IllegalArgumentException("Invalid PyroWave stream shape");
        long bitsPerFrame = (long) width * height * (fullChroma ? 9 : 6) / 4;
        return (int) Math.min(bitsPerFrame * fps * 5 / 4 / 1000, MAXIMUM_BITRATE_KBPS);
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
