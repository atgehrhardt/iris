package com.limelight.binding.input;

import android.view.InputDevice;
import android.view.KeyEvent;

/** Distinguishes hardware Back from Select, virtual navigation, and mouse Back. */
final class ControllerKeyMapping {
    private ControllerKeyMapping() {}

    /** Whether a Back event should press or release the controller Guide button. */
    static boolean isHardwareBack(int keyCode, int source, int flags) {
        return keyCode == KeyEvent.KEYCODE_BACK
                && (flags & (KeyEvent.FLAG_VIRTUAL_HARD_KEY | KeyEvent.FLAG_SOFT_KEYBOARD)) == 0
                && (source & InputDevice.SOURCE_MOUSE) != InputDevice.SOURCE_MOUSE
                && (source & InputDevice.SOURCE_MOUSE_RELATIVE) != InputDevice.SOURCE_MOUSE_RELATIVE;
    }
}
