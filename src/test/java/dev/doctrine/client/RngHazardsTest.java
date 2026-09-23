package dev.doctrine.client;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

class RngHazardsTest {
    @Test void killCommandsAreBlocked() {
        assertTrue(RngHazards.killsPlayer("kill"));
        assertTrue(RngHazards.killsPlayer("minecraft:kill @s"));
        assertTrue(RngHazards.killsPlayer("execute as @s run kill @s"));
        assertTrue(RngHazards.killsPlayer("execute in minecraft:the_nether run minecraft:kill"));
    }

    @Test void otherCommandsAreNotBlocked() {
        assertFalse(RngHazards.killsPlayer("give @s minecraft:kill"));
        assertFalse(RngHazards.killsPlayer("killers"));
        assertFalse(RngHazards.killsPlayer("tp @s 0 64 0"));
    }
}
