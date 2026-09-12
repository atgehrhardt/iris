package com.limelight.binding.input;

import android.view.KeyEvent;
import com.limelight.nvstream.input.ControllerPacket;
import org.junit.Test;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

/** Verifies that controller calibration controls never become gamepad input or stuck keys. */
public class HdrCalibrationInputTest {
    @Test public void recognizesAppTileAndShortcutLaunches() {
        assertTrue(HdrCalibrationInput.isCalibrationApp("Headless HDR Configuration"));
        assertFalse(HdrCalibrationInput.isCalibrationApp("Steam"));
        assertFalse(HdrCalibrationInput.isCalibrationApp(null));
    }

    @Test public void gamepadPacketsNavigateWithoutRepeatingHeldButtons() {
        HdrCalibrationInput input = new HdrCalibrationInput();
        assertEquals(0x25, input.controllerAction(0, ControllerPacket.LEFT_FLAG, (short) 0));
        assertEquals(0, input.controllerAction(0, ControllerPacket.LEFT_FLAG, (short) 0));
        assertEquals(0x27, input.controllerAction(0, ControllerPacket.RIGHT_FLAG, (short) 0));
        assertEquals(0, input.controllerAction(0, 0, (short) 0));
        assertEquals(0x0d, input.controllerAction(0, ControllerPacket.A_FLAG, (short) 0));
        assertEquals(0, input.controllerAction(0, ControllerPacket.A_FLAG, (short) 0));
        assertEquals(0, input.controllerAction(0, 0, (short) 0));
        assertEquals(0x1b, input.controllerAction(0, ControllerPacket.B_FLAG, (short) 0));
        assertEquals(0x52, input.controllerAction(0, ControllerPacket.Y_FLAG, (short) 0));
    }

    @Test public void gamepadSticksAndDevicesHaveIndependentEdges() {
        HdrCalibrationInput input = new HdrCalibrationInput();
        assertEquals(0, input.controllerAction(-1, ControllerPacket.A_FLAG, (short) 0));
        assertEquals(0, input.controllerAction(16, ControllerPacket.A_FLAG, (short) 0));
        assertEquals(0, input.controllerAction(0, 0, (short) 16000));
        assertEquals(0x27, input.controllerAction(0, 0, (short) 20000));
        assertEquals(0, input.controllerAction(0, 0, (short) 20000));
        assertEquals(0x27, input.controllerAction(1, 0, (short) 20000));
        assertEquals(0x25, input.controllerAction(0, 0, (short) -20000));
        assertEquals(0, input.controllerAction(0,
                ControllerPacket.LEFT_FLAG | ControllerPacket.RIGHT_FLAG, (short) 20000));
        assertEquals(0x25, input.controllerAction(0, ControllerPacket.LEFT_FLAG, (short) 20000));
        assertEquals(0, input.controllerAction(0, 0, (short) 0));
    }

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
