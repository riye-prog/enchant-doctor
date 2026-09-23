plugins {
    id("dev.kikugie.stonecutter")
}
stonecutter active "26.3"
stonecutter parameters {
    replacements {
        string(current.parsed < "26.3") {
            replace("com.mojang.renderpearl.backend.vulkan", "com.mojang.blaze3d.vulkan")
            replace("com.mojang.renderpearl.api.device.GpuBackend", "com.mojang.blaze3d.systems.GpuBackend")
            replace("com/mojang/renderpearl/backend/vulkan", "com/mojang/blaze3d/vulkan")
            replace("client.gameMode.dropItem(player, false)", "player.drop(false)")
            replace("startTextInput(this)", "startTextInput()")
            replace("InputConstants.Type.KEYBOARD", "InputConstants.Type.KEYSYM")
        }
    }
}
