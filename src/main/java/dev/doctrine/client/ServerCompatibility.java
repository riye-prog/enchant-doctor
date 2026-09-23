package dev.doctrine.client;

import net.minecraft.client.Minecraft;
import java.util.Locale;

public final class ServerCompatibility {
    private ServerCompatibility() {}
    public static String brand() {
        var connection = Minecraft.getInstance().getConnection();
        String brand = connection == null ? null : connection.serverBrand();
        return brand == null || brand.isBlank() ? "Unknown server" : brand;
    }
    public static boolean usesSharedEntityRandom() {
        return usesSharedEntityRandom(brand());
    }
    static boolean usesSharedEntityRandom(String value) {
        String brand = value.toLowerCase(Locale.ROOT);
        return brand.contains("paper") || brand.contains("purpur") || brand.contains("folia") || brand.contains("pufferfish");
    }
    public static String mode() {
        return usesSharedEntityRandom() ? brand() + ": current table offers only" : brand() + ": player routes available";
    }
    public static String limitation() {
        return "This server shares one RNG across entities. Player-seed recovery and future-seed routes are not reliable; current table offers remain available.";
    }
}
