package dev.doctrine.mixin;

import com.mojang.renderpearl.api.device.GpuBackend;
import com.mojang.renderpearl.backend.vulkan.VulkanBackend;
import net.minecraft.client.PreferredGraphicsApi;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

@Mixin(PreferredGraphicsApi.class)
public abstract class GraphicsApiMixin {
    @Inject(method = "getBackendsToTry", at = @At("HEAD"), cancellable = true)
    private void doctrine$vulkan(CallbackInfoReturnable<GpuBackend[]> callback) {
        callback.setReturnValue(new GpuBackend[]{new VulkanBackend()});
    }
}
