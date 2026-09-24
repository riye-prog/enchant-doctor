package dev.doctrine.client;

import net.minecraft.client.Minecraft;
import net.minecraft.client.player.LocalPlayer;
import net.minecraft.network.protocol.game.ServerboundSetCarriedItemPacket;
import net.minecraft.world.inventory.ContainerInput;
import net.minecraft.world.item.ItemStack;

public final class DropSlots {
    private DropSlots() {}

    public static boolean eligible(ItemStack stack) {
        return !stack.isEmpty() && !stack.isEnchanted() && stack.getMaxStackSize() > 1;
    }

    public static int countEligible(LocalPlayer player) {
        int total = 0;
        for (ItemStack stack : player.getInventory().getNonEquipmentItems())
            if (eligible(stack)) total += stack.getCount();
        return total;
    }

    public static int nextEligibleSlot(LocalPlayer player, int start) {
        var items = player.getInventory().getNonEquipmentItems();
        if (items.isEmpty()) return -1;
        for (int offset = 0; offset < items.size(); offset++) {
            int slot = Math.floorMod(start + offset, items.size());
            if (eligible(items.get(slot))) return slot;
        }
        return -1;
    }

    public static boolean dropOne(Minecraft client, LocalPlayer player, int slot) {
        var inventory = player.getInventory();
        var items = inventory.getNonEquipmentItems();
        if (client.gameMode == null || player.containerMenu != player.inventoryMenu
                || slot < 0 || slot >= items.size() || !eligible(items.get(slot))) return false;
        int selected = inventory.getSelectedSlot();
        if (slot != selected) {
            int menuSlot = slot < 9 ? 36 + slot : slot;
            client.gameMode.handleContainerInput(player.inventoryMenu.containerId, menuSlot, selected, ContainerInput.SWAP, player);
        }
        if (!eligible(player.getMainHandItem())) return false;
        client.gameMode.dropItem(player, false);
        return true;
    }

    public static void select(LocalPlayer player, int slot) {
        player.getInventory().setSelectedSlot(slot);
        sync(player);
    }

    private static void sync(LocalPlayer player) {
        player.connection.send(new ServerboundSetCarriedItemPacket(player.getInventory().getSelectedSlot()));
    }
}
