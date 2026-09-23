package dev.doctrine.mixin;

import com.mojang.renderpearl.backend.vulkan.VulkanDevice;
import com.mojang.renderpearl.backend.vulkan.VulkanGpuSurface;
import dev.doctrine.client.NativeUi;
import dev.doctrine.client.WorkbenchScreen;
import it.unimi.dsi.fastutil.longs.LongList;
import net.minecraft.client.Minecraft;
import org.lwjgl.vulkan.VK;
import org.lwjgl.vulkan.VK12;
import org.lwjgl.vulkan.VkCommandBuffer;
import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.ModifyArg;
import org.spongepowered.asm.mixin.injection.Redirect;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

@Mixin(value = VulkanGpuSurface.class, remap = false)
public abstract class VulkanSurfaceMixin {
    @Shadow @Final private VulkanDevice device;
    @Shadow @Final private int swapchainImageFormat;
    @Shadow @Final private LongList swapchainImages;
    @Shadow private int currentImageIndex;
    @Shadow private int swapchainWidth;
    @Shadow private int swapchainHeight;

    @ModifyArg(method = "configure", at = @At(value = "INVOKE", target = "Lorg/lwjgl/vulkan/VkSwapchainCreateInfoKHR;imageUsage(I)Lorg/lwjgl/vulkan/VkSwapchainCreateInfoKHR;"), index = 0)
    private int doctrine$colorAttachment(int usage) { return usage | VK12.VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; }

    @ModifyArg(method = "blitFromTexture", at = @At(value = "INVOKE", target = "Lcom/mojang/renderpearl/backend/vulkan/VulkanCommandEncoder;signalSemaphore(JJJ)V"), index = 2)
    private long doctrine$signalAfterOverlay(long mask) { return VK12.VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; }

    @Redirect(method = "blitFromTexture", at = @At(value = "INVOKE", target = "Lorg/lwjgl/vulkan/VK12;vkEndCommandBuffer(Lorg/lwjgl/vulkan/VkCommandBuffer;)I"))
    private int doctrine$draw(VkCommandBuffer command) {
        Minecraft client = Minecraft.getInstance();
        boolean show = client.gui.screen() instanceof WorkbenchScreen;
        float scale = Math.max(1f, Math.min(swapchainWidth / 1180f, swapchainHeight / 780f));
        if (!NativeUi.ready()) {
            var physical = device.vkDevice().getPhysicalDevice();
            NativeUi.ensure(physical.getInstance().address(), physical.address(), device.vkDevice().address(),
                VK.getFunctionProvider().getFunctionAddress("vkGetInstanceProcAddr"), swapchainImageFormat, swapchainWidth, swapchainHeight, scale);
        }
        if (NativeUi.ready()) {
            NativeUi.frame(command.address(), swapchainImages.getLong(currentImageIndex), swapchainWidth, swapchainHeight, show, scale);
        }
        return VK12.vkEndCommandBuffer(command);
    }

    @Inject(method = "destroySwapchain", at = @At("HEAD"))
    private void doctrine$resize(CallbackInfo callback) { if (NativeUi.ready()) NativeUi.resize(); }

    @Inject(method = "close", at = @At("HEAD"))
    private void doctrine$close(CallbackInfo callback) { NativeUi.close(); }
}
