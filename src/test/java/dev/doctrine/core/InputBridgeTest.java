package dev.doctrine.core;

import com.mojang.blaze3d.platform.InputConstants;
import dev.doctrine.client.InputBridge;
import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.assertEquals;

class InputBridgeTest {
    @Test void normalizesEachGamesInputCodesToTheNativeContract() {
        assertEquals(1, InputBridge.button(InputConstants.MOUSE_BUTTON_LEFT));
        assertEquals(3, InputBridge.button(InputConstants.MOUSE_BUTTON_RIGHT));
        assertEquals(2, InputBridge.button(InputConstants.MOUSE_BUTTON_MIDDLE));
        assertEquals(4, InputBridge.key(InputConstants.KEY_A));
        assertEquals(29, InputBridge.key(InputConstants.KEY_Z));
        assertEquals(39, InputBridge.key(InputConstants.KEY_0));
        assertEquals(30, InputBridge.key(InputConstants.KEY_1));
        assertEquals(38, InputBridge.key(InputConstants.KEY_9));
        assertEquals(40, InputBridge.key(InputConstants.KEY_RETURN));
        assertEquals(41, InputBridge.key(InputConstants.KEY_ESCAPE));
        assertEquals(80, InputBridge.key(InputConstants.KEY_LEFT));
        assertEquals(195, InputBridge.modifiers(InputConstants.MOD_CONTROL | InputConstants.MOD_SHIFT));
        assertEquals(3072, InputBridge.modifiers(InputConstants.MOD_SUPER));
    }
}
