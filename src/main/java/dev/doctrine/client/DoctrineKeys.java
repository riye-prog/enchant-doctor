package dev.doctrine.client;

import com.mojang.blaze3d.platform.InputConstants;
import net.fabricmc.fabric.api.client.keymapping.v1.KeyMappingHelper;
import net.minecraft.client.KeyMapping;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.inventory.EnchantmentScreen;
import net.minecraft.client.input.KeyEvent;
import net.minecraft.resources.Identifier;

public final class DoctrineKeys {
    private static KeyMapping toggle;
    private static KeyMapping recover;
    private static KeyMapping planner;
    private static KeyMapping cancel;
    private static KeyMapping observe;
    private static KeyMapping drop;
    private DoctrineKeys() {}
    public static void register() {
        var category = KeyMapping.Category.register(Identifier.fromNamespaceAndPath("doctrine", "workbench"));
        toggle = mapping("toggle", InputConstants.KEY_F8, category);
        recover = mapping("recover", InputConstants.KEY_F7, category);
        planner = mapping("planner", InputConstants.KEY_F9, category);
        cancel = mapping("cancel", InputConstants.KEY_F10, category);
        observe = mapping("observe", InputConstants.KEY_F6, category);
        drop = mapping("drop", InputConstants.KEY_G, category);
    }
    private static KeyMapping mapping(String name, int code, KeyMapping.Category category) {
        return KeyMappingHelper.registerKeyMapping(new KeyMapping("key.doctrine." + name, InputConstants.Type.KEYBOARD, code, category));
    }
    public static boolean handle(KeyEvent event) {
        if (toggle == null) return false;
        var client = Minecraft.getInstance();
        if (client.player == null) return false;
        var screen = client.gui.screen();
        if (screen != null && !(screen instanceof WorkbenchScreen) && !(screen instanceof EnchantmentScreen)) return false;
        if (toggle.matches(event)) { DoctrineClient.INSTANCE.open(); return true; }
        if (cancel.matches(event)) { DoctrineClient.INSTANCE.action("cancel"); return true; }
        if (recover.matches(event)) { DoctrineClient.INSTANCE.action("recover"); return true; }
        if (drop.matches(event)) { DoctrineClient.INSTANCE.action("drop"); return true; }
        if (planner.matches(event)) {
            if (!(screen instanceof WorkbenchScreen)) DoctrineClient.INSTANCE.open();
            if (NativeUi.ready()) NativeUi.selectTab("planner");
            return true;
        }
        if (observe.matches(event)) { DoctrineClient.INSTANCE.action("observe"); return true; }
        return false;
    }
    public static String label(String action) {
        KeyMapping key = switch(action) {
            case "recover" -> recover;
            case "planner" -> planner;
            case "cancel" -> cancel;
            case "observe" -> observe;
            case "drop" -> drop;
            default -> toggle;
        };
        return key == null ? "" : key.getTranslatedKeyMessage().getString();
    }
}
