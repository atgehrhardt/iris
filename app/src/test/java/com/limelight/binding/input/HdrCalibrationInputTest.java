package com.limelight.binding.input;

import android.view.KeyEvent;
import org.junit.Test;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

/** Verifies that controller calibration controls never become gamepad input or stuck keys. */
public class HdrCalibrationInputTest {
    @Test public void heldConfirmCannotSkipPagesOrSave() {
        assertTrue(HdrCalibrationInput.allowsRepeat(0x25));
        assertTrue(HdrCalibrationInput.allowsRepeat(0x27));
        assertFalse(HdrCalibrationInput.allowsRepeat(0x0d));
        assertFalse(HdrCalibrationInput.allowsRepeat(0x1b));
        assertFalse(HdrCalibrationInput.allowsRepeat(0x52));
    }
    @Test public void mapsControllerAndKeyboardActions() {
        assertEquals(0x25, HdrCalibrationInput.virtualKey(KeyEvent.KEYCODE_DPAD_LEFT));
        assertEquals(0x27, HdrCalibrationInput.virtualKey(KeyEvent.KEYCODE_DPAD_RIGHT));
        for (int key : new int[]{KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.KEYCODE_ENTER}) {
            assertEquals(0x0d, HdrCalibrationInput.virtualKey(key));
        }
        for (int key : new int[]{KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_ESCAPE, KeyEvent.KEYCODE_BACK}) {
            assertEquals(0x1b, HdrCalibrationInput.virtualKey(key));
        }
        assertEquals(0x52, HdrCalibrationInput.virtualKey(KeyEvent.KEYCODE_BUTTON_Y));
        assertEquals(0x52, HdrCalibrationInput.virtualKey(KeyEvent.KEYCODE_R));
        assertEquals(0, HdrCalibrationInput.virtualKey(KeyEvent.KEYCODE_VOLUME_UP));
    }

    @Test public void hatAndStickNavigateOnceUntilReleasedOrReversed() {
        HdrCalibrationInput input = new HdrCalibrationInput();
        assertEquals(0, input.horizontalAxis(0, 0.3f));
        assertEquals(0x25, input.horizontalAxis(-1, 0));
        assertEquals(0, input.horizontalAxis(-1, 0));
        assertEquals(0x27, input.horizontalAxis(1, 0));
        assertEquals(0, input.horizontalAxis(0, 0));
        assertEquals(0x27, input.horizontalAxis(0, 0.9f));
        assertEquals(0, input.horizontalAxis(0, 0.9f));
        assertEquals(0x25, input.horizontalAxis(-1, 0.9f));
        assertEquals(0, input.horizontalAxis(Float.NaN, Float.NaN));
    }
}
