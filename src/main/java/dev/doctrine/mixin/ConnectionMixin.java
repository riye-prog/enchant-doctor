package dev.doctrine.mixin;

import dev.doctrine.client.DoctrineClient;
import net.minecraft.network.Connection;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ServerboundPlayerActionPacket;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

@Mixin(Connection.class)
public abstract class ConnectionMixin {
    @Inject(method = "send(Lnet/minecraft/network/protocol/Packet;)V", at = @At("HEAD"))
    private void doctrine$drop(Packet<?> packet, CallbackInfo callback) {
        if (packet instanceof ServerboundPlayerActionPacket action
                && (action.getAction() == ServerboundPlayerActionPacket.Action.DROP_ITEM || action.getAction() == ServerboundPlayerActionPacket.Action.DROP_ALL_ITEMS))
            DoctrineClient.INSTANCE.dropped();
    }
}
