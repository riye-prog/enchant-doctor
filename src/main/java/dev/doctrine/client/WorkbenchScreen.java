package dev.doctrine.client;

import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.input.CharacterEvent;
import net.minecraft.client.input.KeyEvent;
import net.minecraft.client.input.MouseButtonEvent;
import net.minecraft.network.chat.Component;
import com.mojang.blaze3d.platform.InputConstants;

public final class WorkbenchScreen extends Screen {
    private final Screen parent;
    public WorkbenchScreen(Screen parent) {
        super(Component.literal("Doctrine enchantment workbench"));
        this.parent = parent;
    }
    @Override public boolean isPauseScreen() { return false; }
    @Override protected void init() { minecraft.textInputManager().startTextInput(this); }
    @Override public void removed() { minecraft.textInputManager().stopTextInput(); }
    @Override public void onClose() { minecraft.gui.setScreen(parent); }
    @Override public void mouseMoved(double x, double y) {
        if (NativeUi.ready()) NativeUi.input(0, x * NativeUi.pixelWidth() / width, y * NativeUi.pixelHeight() / height, 0, 0);
    }
    @Override public boolean mouseClicked(MouseButtonEvent event, boolean doubleClick) {
        mouseMoved(event.x(), event.y());
        if (NativeUi.ready()) NativeUi.input(1, 0, 0, event.button(), event.modifiers());
        return true;
    }
    @Override public boolean mouseReleased(MouseButtonEvent event) {
        if (NativeUi.ready()) NativeUi.input(2, 0, 0, event.button(), event.modifiers());
        return true;
    }
    @Override public boolean mouseDragged(MouseButtonEvent event, double x, double y) { mouseMoved(event.x(), event.y()); return true; }
    @Override public boolean mouseScrolled(double x, double y, double horizontal, double vertical) {
        if (NativeUi.ready()) NativeUi.input(3, horizontal, vertical, 0, 0);
        return true;
    }
    @Override public boolean keyPressed(KeyEvent event) {
        if (event.key() == InputConstants.KEY_ESCAPE) { onClose(); return true; }
        if (NativeUi.ready()) {
            if (event.isPaste()) NativeUi.setClipboard(minecraft.keyboardHandler.getClipboard());
            NativeUi.input(4, 0, 0, event.key(), event.modifiers());
            String copied = NativeUi.takeClipboard();
            if (copied != null) minecraft.keyboardHandler.setClipboard(copied);
        }
        return true;
    }
    @Override public boolean keyReleased(KeyEvent event) {
        if (NativeUi.ready()) NativeUi.input(5, 0, 0, event.key(), event.modifiers());
        return true;
    }
    @Override public boolean charTyped(CharacterEvent event) {
        if (NativeUi.ready()) NativeUi.input(6, 0, 0, event.codepoint(), 0);
        return true;
    }
}
