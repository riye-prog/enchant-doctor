package dev.doctrine.mixin;

import dev.doctrine.client.DoctrineClient;
import net.minecraft.client.multiplayer.MultiPlayerGameMode;
import net.minecraft.client.player.LocalPlayer;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.inventory.ContainerInput;
import net.minecraft.core.BlockPos;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

@Mixin(MultiPlayerGameMode.class)
public abstract class GameModeMixin {
    @Inject(method = "useItemOn", at = @At("HEAD"))
    private void doctrine$table(LocalPlayer player, InteractionHand hand, BlockHitResult hit, CallbackInfoReturnable<InteractionResult> callback) {
        DoctrineClient.INSTANCE.tableUsed(hit.getBlockPos());
    }
    @Inject(method = "handleInventoryButtonClick", at = @At("HEAD"))
    private void doctrine$enchant(int containerId, int button, CallbackInfo callback) { DoctrineClient.INSTANCE.enchantRequested(containerId, button); }
    @Inject(method = "destroyBlock", at = @At("HEAD"))
    private void doctrine$blockBreak(BlockPos pos, CallbackInfoReturnable<Boolean> callback) { DoctrineClient.INSTANCE.blockBroken(); }
    @Inject(method = {"attack", "handleCreativeModeItemAdd", "handleCreativeModeItemDrop"}, at = @At("HEAD"))
    private void doctrine$otherActivity(CallbackInfo callback) { DoctrineClient.INSTANCE.invalidatePlayer("Inventory or item activity changed player RNG assumptions. Recover a fresh pair."); }
    @Inject(method = "useItem", at = @At("HEAD"))
    private void doctrine$use(CallbackInfoReturnable<InteractionResult> callback) { DoctrineClient.INSTANCE.invalidatePlayer("An item was used. Recover a fresh seed pair."); }
    @Inject(method = "handleContainerInput", at = @At("HEAD"))
    private void doctrine$inventory(int container, int slot, int button, ContainerInput input, Player player, CallbackInfo callback) {
        if (input == ContainerInput.THROW || slot == -999)
            DoctrineClient.INSTANCE.invalidatePlayer("An inventory drop changed player RNG. Use the drop key for tracked single-item drops.");
    }
}
