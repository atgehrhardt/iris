package com.limelight.binding.input;

import android.view.KeyEvent;
import org.junit.Test;

import static org.junit.Assert.assertEquals;

public class ControllerKeyMappingTest {
    @Test
    public void odinHardwareBackSendsGuide() {
        assertEquals(KeyEvent.KEYCODE_BUTTON_MODE,
                ControllerKeyMapping.remap(0x2020, 0x0112, KeyEvent.KEYCODE_BACK));
    }

    @Test
    public void odinSelectAndStartRemainDistinct() {
        for (int key : new int[] {KeyEvent.KEYCODE_BUTTON_SELECT,
                KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_BUTTON_MODE}) {
            assertEquals(key, ControllerKeyMapping.remap(0x2020, 0x0112, key));
        }
    }

    @Test
    public void otherControllersKeepTheirBackMapping() {
        assertEquals(KeyEvent.KEYCODE_BACK,
                ControllerKeyMapping.remap(0x045e, 0x0112, KeyEvent.KEYCODE_BACK));
        assertEquals(KeyEvent.KEYCODE_BACK,
                ControllerKeyMapping.remap(0x2020, 0x0001, KeyEvent.KEYCODE_BACK));
    }
}
