# Headless HDR configuration

Select a paired, online Prism host, press **Y**, and choose **Headless HDR
Configuration**. The host must have the updated `prism-hdr-calibration` app and
private HDR compositor installed, and it must be idle.

The wizard forces HDR for this session only. It refuses to launch if the display,
decoder or host cannot negotiate HDR; it never substitutes an SDR calibration
stream. A saved H.264-only preference is temporarily overridden for the wizard.

Use D-pad left/right or the left stick to adjust, **A** to advance/save, **B** to
go back/cancel, and **Y** to reset. Saving closes the stream. The profile lives on
that host and is keyed by this installation's paired certificate, so other
devices keep their own calibration. Clearing Iris's app data replaces that
identity. The wizard does not change the device's global brightness setting or
the user's saved HDR/codec preferences.

SDR white adjusts SDR games inside an HDR stream. Peak luminance sets the host's
HDR mastering metadata. Native HDR games still require their own HDR settings;
the wizard does not add native HDR to SDR-only games. Tone mapping can prevent a
clipping cross from disappearing; use the display's rated peak in that case.
