package dev.doctrine.core;

import net.minecraft.SharedConstants;
import net.minecraft.server.Bootstrap;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.core.component.DataComponentMap;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.item.enchantment.Enchantable;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;
import java.util.Random;
import static org.junit.jupiter.api.Assertions.assertEquals;

class VanillaCostTest {
    @BeforeAll static void bootstrap() {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();
        Items.DIAMOND_PICKAXE.builtInRegistryHolder().bindComponents(DataComponentMap.builder().set(DataComponents.ENCHANTABLE, new Enchantable(10)).build());
    }
    @Test void optimizedCostsMatchEachTargetVersionsVanillaImplementation() {
        Random seeds = new Random(9384938);
        Lcg48 optimized = new Lcg48(0);
        RandomSource vanilla = RandomSource.create();
        ItemStack item = new ItemStack(Items.DIAMOND_PICKAXE);
        for (int shelves = 0; shelves <= 24; shelves++) {
            for (int i = 0; i < 1000; i++) {
                int seed = seeds.nextInt();
                optimized.setSeed(seed);
                vanilla.setSeed(seed);
                for (int slot = 0; slot < 3; slot++) {
                    int expected = EnchantmentHelper.getEnchantmentCost(vanilla, slot, shelves, item);
                    if (expected < slot + 1) expected = 0;
                    assertEquals(expected, SeedCandidates.cost(optimized, slot, shelves));
                }
            }
        }
    }
}
