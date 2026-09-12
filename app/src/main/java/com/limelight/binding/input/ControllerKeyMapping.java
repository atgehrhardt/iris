package com.limelight.binding.input;

import android.view.KeyEvent;

/** Device-specific mappings for distinct controller and Android navigation buttons. */
final class ControllerKeyMapping {
    private ControllerKeyMapping() {}

    /** Preserve Select while mapping the Odin virtual controller's hardware Back to Guide. */
    static int remap(int vendorId, int productId, int keyCode) {
        // Odin3 exposes its built-in controls as "Xbox Wireless Controller" with
        // this VID/PID. Its hardware Back and Select emit distinct Android keys.
        if (vendorId == 0x2020 && productId == 0x0112 && keyCode == KeyEvent.KEYCODE_BACK) {
            return KeyEvent.KEYCODE_BUTTON_MODE;
        }
        return keyCode;
    }
}
