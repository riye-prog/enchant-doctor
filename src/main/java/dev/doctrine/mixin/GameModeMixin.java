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
    @Inject(method = "useItemOn", at = @At("HEAD"), cancellable = true)
    private void doctrine$table(LocalPlayer player, InteractionHand hand, BlockHitResult hit, CallbackInfoReturnable<InteractionResult> callback) {
        var client = net.minecraft.client.Minecraft.getInstance();
        if (client.level != null && client.level.getBlockState(hit.getBlockPos()).is(net.minecraft.world.level.block.Blocks.ENCHANTING_TABLE)) {
            DoctrineClient.INSTANCE.tableUsed(hit.getBlockPos());
        } else if (!player.getItemInHand(hand).isEmpty() && DoctrineClient.INSTANCE.guard("using an item on a block")) {
            callback.setReturnValue(InteractionResult.PASS);
        }
    }
    @Inject(method = "handleInventoryButtonClick", at = @At("HEAD"), cancellable = true)
    private void doctrine$enchant(int containerId, int button, CallbackInfo callback) {
        var player = net.minecraft.client.Minecraft.getInstance().player;
        if (player != null && player.containerMenu instanceof net.minecraft.world.inventory.AnvilMenu
                && DoctrineClient.INSTANCE.guard("using an anvil")) { callback.cancel(); return; }
        DoctrineClient.INSTANCE.enchantRequested(containerId, button);
    }
    @Inject(method = "destroyBlock", at = @At("HEAD"), cancellable = true)
    private void doctrine$blockBreak(BlockPos pos, CallbackInfoReturnable<Boolean> callback) {
        if (DoctrineClient.INSTANCE.guard("block breaking (tool durability)")) callback.setReturnValue(false);
    }
    @Inject(method = "attack", at = @At("HEAD"), cancellable = true)
    private void doctrine$attack(Player player, net.minecraft.world.entity.Entity target, CallbackInfo callback) {
        if (DoctrineClient.INSTANCE.guard("attacking")) callback.cancel();
    }
    @Inject(method = "useItem", at = @At("HEAD"), cancellable = true)
    private void doctrine$use(CallbackInfoReturnable<InteractionResult> callback) {
        if (DoctrineClient.INSTANCE.guard("using an item")) callback.setReturnValue(InteractionResult.PASS);
    }
    @Inject(method = "handleContainerInput", at = @At("HEAD"), cancellable = true)
    private void doctrine$inventory(int container, int slot, int button, ContainerInput input, Player player, CallbackInfo callback) {
        if ((input == ContainerInput.THROW || slot == -999)
                && DoctrineClient.INSTANCE.guard("an untracked inventory drop (use the drop key instead)")) callback.cancel();
    }
    @Inject(method = "handleCreativeModeItemDrop", at = @At("HEAD"), cancellable = true)
    private void doctrine$creativeDrop(CallbackInfo callback) {
        if (DoctrineClient.INSTANCE.guard("a creative inventory drop")) callback.cancel();
    }
}
