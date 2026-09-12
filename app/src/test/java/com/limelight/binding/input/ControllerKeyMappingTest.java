package com.limelight.binding.input;

import android.view.InputDevice;
import android.view.KeyEvent;
import org.junit.Test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

public class ControllerKeyMappingTest {
    @Test
    public void hardwareBackSendsGuideForAllInputDeviceKinds() {
        for (int source : new int[] {InputDevice.SOURCE_GAMEPAD, InputDevice.SOURCE_JOYSTICK,
                InputDevice.SOURCE_KEYBOARD, InputDevice.SOURCE_DPAD, InputDevice.SOURCE_UNKNOWN}) {
            assertTrue(ControllerKeyMapping.isHardwareBack(KeyEvent.KEYCODE_BACK, source, 0));
        }
    }

    @Test
    public void selectStartAndEscapeRemainSeparate() {
        for (int key : new int[] {KeyEvent.KEYCODE_BUTTON_SELECT, KeyEvent.KEYCODE_BUTTON_START,
                KeyEvent.KEYCODE_ESCAPE, KeyEvent.KEYCODE_BUTTON_MODE}) {
            assertFalse(ControllerKeyMapping.isHardwareBack(key, InputDevice.SOURCE_GAMEPAD, 0));
        }
    }

    @Test
    public void virtualNavigationAndSoftKeyboardBackRemainNavigation() {
        assertFalse(ControllerKeyMapping.isHardwareBack(KeyEvent.KEYCODE_BACK,
                InputDevice.SOURCE_KEYBOARD, KeyEvent.FLAG_VIRTUAL_HARD_KEY));
        assertFalse(ControllerKeyMapping.isHardwareBack(KeyEvent.KEYCODE_BACK,
                InputDevice.SOURCE_KEYBOARD, KeyEvent.FLAG_SOFT_KEYBOARD));
    }

    @Test
    public void mouseBackDoesNotSendGuide() {
        for (int source : new int[] {InputDevice.SOURCE_MOUSE, InputDevice.SOURCE_MOUSE_RELATIVE,
                InputDevice.SOURCE_MOUSE | InputDevice.SOURCE_KEYBOARD}) {
            assertFalse(ControllerKeyMapping.isHardwareBack(KeyEvent.KEYCODE_BACK, source, 0));
        }
    }

    @Test
    public void canceledHardwareReleaseStillReachesGuideHandler() {
        assertTrue(ControllerKeyMapping.isHardwareBack(KeyEvent.KEYCODE_BACK,
                InputDevice.SOURCE_KEYBOARD, KeyEvent.FLAG_CANCELED));
    }
}
