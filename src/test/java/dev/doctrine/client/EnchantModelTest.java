package dev.doctrine.client;

import net.minecraft.SharedConstants;
import com.mojang.serialization.Lifecycle;
import dev.doctrine.core.Lcg48;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.MappedRegistry;
import net.minecraft.core.RegistrationInfo;
import net.minecraft.core.component.DataComponentMap;
import net.minecraft.core.component.DataComponents;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.enchantment.Enchantable;
import net.minecraft.tags.EnchantmentTags;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;
import java.util.List;
import java.util.Map;
import java.util.OptionalLong;
import static org.junit.jupiter.api.Assertions.*;

class EnchantModelTest {
    @BeforeAll static void bootstrap() {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();
        Items.BOOK.builtInRegistryHolder().bindComponents(DataComponentMap.builder()
            .set(DataComponents.ENCHANTABLE, new Enchantable(1)).build());
    }

    private HolderLookup.Provider vanillaLookup() throws Exception {
        HolderLookup.Provider lookup;
        try {
            lookup = (HolderLookup.Provider) VanillaRegistries.class.getMethod("createWorldLookup").invoke(null);
        } catch (NoSuchMethodException ignored) {
            lookup = (HolderLookup.Provider) VanillaRegistries.class.getMethod("createLookup").invoke(null);
        }
        return lookup;
    }

    @Test void efficiencyFiveCannotRollDirectlyOnBooks() throws Exception {
        var enchantments = vanillaLookup().lookupOrThrow(Registries.ENCHANTMENT);
        var efficiency = enchantments.getOrThrow(ResourceKey.create(Registries.ENCHANTMENT,
            Identifier.parse("minecraft:efficiency"))).value();
        int maxPower = EnchantModel.maximumTablePower(new ItemStack(Items.BOOK));
        assertTrue(efficiency.getMinCost(4) <= maxPower);
        assertTrue(efficiency.getMinCost(5) > maxPower);
    }

    @Test void onlySearchesForTheRequestedSingleTableResult() throws Exception {
        var lookup = vanillaLookup().lookupOrThrow(Registries.ENCHANTMENT);
        var key = ResourceKey.create(Registries.ENCHANTMENT, Identifier.parse("minecraft:efficiency"));
        var registry = new MappedRegistry<>(Registries.ENCHANTMENT, Lifecycle.stable());
        var efficiency = registry.register(key, lookup.getOrThrow(key).value(), RegistrationInfo.BUILT_IN);
        var unbreakingKey = ResourceKey.create(Registries.ENCHANTMENT, Identifier.parse("minecraft:unbreaking"));
        var unbreaking = registry.register(unbreakingKey, lookup.getOrThrow(unbreakingKey).value(), RegistrationInfo.BUILT_IN);
        registry.bindTags(Map.of(EnchantmentTags.IN_ENCHANTING_TABLE, List.of(efficiency, unbreaking)));
        registry.freeze();
        var model = new EnchantModel(registry, new ItemStack(Items.BOOK));
        assertTrue(model.unavailableReason(Map.of("minecraft:efficiency", 5)).contains("cannot roll directly"));
        assertNull(model.search(null, OptionalLong.of(123456L), Map.of("minecraft:efficiency", 5), 500, 60));
        var plan = model.search(null, OptionalLong.of(123456L), Map.of("minecraft:efficiency", 4), 500, 60);
        assertNotNull(plan);
        Lcg48 state = new Lcg48(123456L);
        state.advance(4L * plan.drops());
        int tableSeed = plan.dummy() ? state.nextInt() : 0;
        assertTrue(model.offer(tableSeed, plan.slot(), plan.level()).stream()
            .anyMatch(e -> e.enchantment().equals(efficiency) && e.level() >= 4));
    }
}
