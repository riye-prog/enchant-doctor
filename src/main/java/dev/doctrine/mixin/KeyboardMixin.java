package dev.doctrine.mixin;

import dev.doctrine.client.DoctrineClient;
import dev.doctrine.client.DoctrineKeys;
import net.minecraft.client.KeyboardHandler;
import net.minecraft.client.input.KeyEvent;
import com.mojang.blaze3d.platform.InputConstants;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

@Mixin(KeyboardHandler.class)
public abstract class KeyboardMixin {
    @Inject(method = "keyPress", at = @At("HEAD"), cancellable = true)
    private void doctrine$key(long window, int action, KeyEvent event, CallbackInfo callback) {
        if (action == InputConstants.PRESS && DoctrineKeys.handle(event)) {
            callback.cancel();
        }
    }
}
