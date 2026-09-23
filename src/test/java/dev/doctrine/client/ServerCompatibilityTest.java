package dev.doctrine.client;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

class ServerCompatibilityTest {
    @Test void recognizesPaperFamilyBrands() {
        assertTrue(ServerCompatibility.usesSharedEntityRandom("Paper"));
        assertTrue(ServerCompatibility.usesSharedEntityRandom("Purpur 26.2"));
        assertTrue(ServerCompatibility.usesSharedEntityRandom("Folia"));
        assertTrue(ServerCompatibility.usesSharedEntityRandom("Pufferfish"));
    }
    @Test void preservesVanillaCompatibleBrands() {
        assertFalse(ServerCompatibility.usesSharedEntityRandom("vanilla"));
        assertFalse(ServerCompatibility.usesSharedEntityRandom("Spigot"));
        assertFalse(ServerCompatibility.usesSharedEntityRandom("fabric"));
    }
}
