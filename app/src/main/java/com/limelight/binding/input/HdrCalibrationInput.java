package com.limelight.binding.input;

import android.view.KeyEvent;
import com.limelight.nvstream.input.ControllerPacket;

/** Controller-to-keyboard navigation used only by the host HDR calibration stream. */
public final class HdrCalibrationInput {
    public static final String APP_NAME = "Headless HDR Configuration";
    public static final String EXTRA_CALIBRATION = "HeadlessHdrCalibration";
    private int previousHat;
    private final int[] previousButtons = new int[16];
    private final int[] previousDirection = new int[16];

    /** Identifies calibration launches, including app tiles and old shortcuts. */
    public static boolean isCalibrationApp(String appName) {
        return APP_NAME.equals(appName);
    }

    /** Converts normalized gamepad packets from USB and on-screen controllers to keys. */
    public synchronized int controllerAction(int controller, int buttons, short stickX) {
        if (controller < 0 || controller >= previousButtons.length) return 0;
        int pressed = buttons & ~previousButtons[controller];
        previousButtons[controller] = buttons;
        boolean left = (buttons & ControllerPacket.LEFT_FLAG) != 0;
        boolean right = (buttons & ControllerPacket.RIGHT_FLAG) != 0;
        int direction = left || right ? (left == right ? 0 : left ? -1 : 1)
                : stickX < -16384 ? -1 : stickX > 16384 ? 1 : 0;
        boolean moved = direction != previousDirection[controller];
        previousDirection[controller] = direction;
        if ((pressed & ControllerPacket.B_FLAG) != 0) return 0x1b;
        if ((pressed & ControllerPacket.Y_FLAG) != 0) return 0x52;
        if ((pressed & ControllerPacket.A_FLAG) != 0) return 0x0d;
        return moved ? (direction < 0 ? 0x25 : direction > 0 ? 0x27 : 0) : 0;
    }

    /** Allows held directional adjustment without letting held A skip pages and save. */
    public static boolean allowsRepeat(int virtualKey) {
        return virtualKey == 0x25 || virtualKey == 0x27;
    }

    /** Returns a Windows virtual key for a wizard action, or zero for unrelated input. */
    public static int virtualKey(int androidKey) {
        switch (androidKey) {
            case KeyEvent.KEYCODE_DPAD_LEFT: return 0x25;
            case KeyEvent.KEYCODE_DPAD_RIGHT: return 0x27;
            case KeyEvent.KEYCODE_BUTTON_A:
            case KeyEvent.KEYCODE_DPAD_CENTER:
            case KeyEvent.KEYCODE_ENTER: return 0x0d;
            case KeyEvent.KEYCODE_BUTTON_B:
            case KeyEvent.KEYCODE_ESCAPE:
            case KeyEvent.KEYCODE_BACK: return 0x1b;
            case KeyEvent.KEYCODE_BUTTON_Y:
            case KeyEvent.KEYCODE_R: return 0x52;
            default: return 0;
        }
    }

    /** Converts hat or stick movement to one navigation action until released or reversed. */
    public int horizontalAxis(float hat, float stick) {
        float value = Math.abs(hat) > 0.5f ? hat : stick;
        int direction = value < -0.5f ? -1 : value > 0.5f ? 1 : 0;
        if (direction == previousHat) return 0;
        previousHat = direction;
        return direction < 0 ? 0x25 : direction > 0 ? 0x27 : 0;
    }
}
