package dev.doctrine.mixin;

import dev.doctrine.client.DoctrineClient;
import net.minecraft.client.Minecraft;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

@Mixin(Minecraft.class)
public abstract class MinecraftMixin {
    @Inject(method = "tick", at = @At("TAIL"))
    private void doctrine$tick(CallbackInfo callback) { DoctrineClient.INSTANCE.tick(); }
    @Inject(method = "close", at = @At("HEAD"))
    private void doctrine$close(CallbackInfo callback) { DoctrineClient.INSTANCE.shutdown(); }
}
