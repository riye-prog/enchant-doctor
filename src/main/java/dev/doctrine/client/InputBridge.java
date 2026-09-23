package dev.doctrine.client;

import com.mojang.blaze3d.platform.InputConstants;

public final class InputBridge {
    private InputBridge() {}
    public static int key(int code) {
        if (code >= InputConstants.KEY_A && code <= InputConstants.KEY_Z) return 4 + code - InputConstants.KEY_A;
        if (code >= InputConstants.KEY_1 && code <= InputConstants.KEY_9) return 30 + code - InputConstants.KEY_1;
        if (code == InputConstants.KEY_0) return 39;
        if (code == InputConstants.KEY_BACKSPACE) return 42;
        if (code == InputConstants.KEY_TAB) return 43;
        if (code == InputConstants.KEY_RETURN) return 40;
        if (code == InputConstants.KEY_ESCAPE) return 41;
        if (code == InputConstants.KEY_SPACE) return 44;
        if (code == InputConstants.KEY_DELETE) return 76;
        if (code == InputConstants.KEY_LEFT) return 80;
        if (code == InputConstants.KEY_RIGHT) return 79;
        if (code == InputConstants.KEY_UP) return 82;
        if (code == InputConstants.KEY_DOWN) return 81;
        if (code == InputConstants.KEY_HOME) return 74;
        if (code == InputConstants.KEY_END) return 77;
        return -1;
    }
    public static int button(int code) {
        if (code == InputConstants.MOUSE_BUTTON_LEFT) return 1;
        if (code == InputConstants.MOUSE_BUTTON_RIGHT) return 3;
        if (code == InputConstants.MOUSE_BUTTON_MIDDLE) return 2;
        return code + 1;
    }
    public static int modifiers(int modifiers) {
        int result = 0;
        if ((modifiers & InputConstants.MOD_SHIFT) != 0) result |= 3;
        if ((modifiers & InputConstants.MOD_CONTROL) != 0) result |= 192;
        if ((modifiers & InputConstants.MOD_ALT) != 0) result |= 768;
        if ((modifiers & InputConstants.MOD_SUPER) != 0) result |= 3072;
        return result;
    }
}
