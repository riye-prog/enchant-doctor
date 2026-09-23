package dev.doctrine.mixin;

import dev.doctrine.client.DoctrineClient;
import net.minecraft.client.Minecraft;
import net.minecraft.client.multiplayer.ClientPacketListener;
import net.minecraft.network.protocol.game.ClientboundDamageEventPacket;
import net.minecraft.network.protocol.game.ClientboundEntityEventPacket;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.network.protocol.game.ClientboundTakeItemEntityPacket;
import net.minecraft.network.protocol.game.ClientboundRespawnPacket;
import net.minecraft.network.protocol.game.ClientboundUpdateAdvancementsPacket;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

@Mixin(ClientPacketListener.class)
public abstract class PlayerEventsMixin {
    @Inject(method = "handleAddEntity", at = @At("TAIL"))
    private void doctrine$itemSpawn(ClientboundAddEntityPacket packet, CallbackInfo callback) { DoctrineClient.INSTANCE.itemSpawned(packet); }
    @Inject(method = "handleTakeItemEntity", at = @At("HEAD"))
    private void doctrine$itemTaken(ClientboundTakeItemEntityPacket packet, CallbackInfo callback) { DoctrineClient.INSTANCE.itemTaken(packet); }
    @Inject(method = "handleRespawn", at = @At("HEAD"))
    private void doctrine$respawn(ClientboundRespawnPacket packet, CallbackInfo callback) { DoctrineClient.INSTANCE.respawned(packet); }
    @Inject(method = "handleUpdateAdvancementsPacket", at = @At("HEAD"))
    private void doctrine$advancement(ClientboundUpdateAdvancementsPacket packet, CallbackInfo callback) { DoctrineClient.INSTANCE.advancementUpdated(packet); }
    @Inject(method = "handleDamageEvent", at = @At("TAIL"))
    private void doctrine$damage(ClientboundDamageEventPacket packet, CallbackInfo callback) {
        var player = Minecraft.getInstance().player;
        if (player != null && packet.entityId() == player.getId()) DoctrineClient.INSTANCE.invalidatePlayer("A damage event invalidated player RNG tracking.");
    }
    @Inject(method = "handleEntityEvent", at = @At("TAIL"))
    private void doctrine$event(ClientboundEntityEventPacket packet, CallbackInfo callback) {
        var client = Minecraft.getInstance();
        if (client.level != null && packet.getEntity(client.level) == client.player) DoctrineClient.INSTANCE.invalidatePlayer("A server player event invalidated RNG tracking.");
    }
    @Inject(method = "sendCommand", at = @At("HEAD"))
    private void doctrine$command(String command, CallbackInfo callback) {
        if (command.startsWith("give ") || command.startsWith("minecraft:give ")) DoctrineClient.INSTANCE.invalidatePlayer("A give command can consume player RNG. Recover a fresh pair.");
    }
}
