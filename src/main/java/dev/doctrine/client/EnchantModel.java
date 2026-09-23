package dev.doctrine.client;

import dev.doctrine.core.Lcg48;
import dev.doctrine.core.SeedCandidates;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.tags.EnchantmentTags;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.world.item.enchantment.EnchantmentInstance;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.OptionalLong;
import java.util.concurrent.CancellationException;
import java.util.function.IntPredicate;
import java.util.stream.Collectors;

public final class EnchantModel {
    private final Registry<Enchantment> registry;
    private final List<Holder<Enchantment>> enchantments;
    private final ItemStack item;
    private final RandomSource random = RandomSource.create();
    private final Lcg48 costsRandom = new Lcg48(0);

    public record Plan(int drops, int shelves, int slot, int level, boolean dummy, String enchantments) {
        public String describe() {
            String preparation = dummy ? "Drop " + drops + " individual items, then perform one cheap dummy enchantment.\n" : "Use the current table seed. No dummy enchantment needed.\n";
            return preparation + "Set " + shelves + " active bookshelves. Choose offer " + (slot + 1) + ".\nRequires level " + level + " and " + (slot + 1) + " lapis.\n" + enchantments;
        }
    }

    public EnchantModel(Registry<Enchantment> registry, ItemStack item) {
        this.registry = registry;
        this.item = item.copy();
        this.enchantments = registry.get(EnchantmentTags.IN_ENCHANTING_TABLE).map(tag -> tag.stream().toList()).orElse(List.of());
    }
    public List<EnchantmentInstance> offer(int seed, int slot, int cost) {
        if (cost <= 0) return List.of();
        random.setSeed(seed + slot);
        var result = new ArrayList<>(EnchantmentHelper.selectEnchantment(random, item, cost, enchantments.stream()));
        if (item.is(Items.BOOK) && result.size() > 1) result.remove(random.nextInt(result.size()));
        return result;
    }
    public IntPredicate matcher(int shelves, int[] costs, int[] clues, int[] levels) {
        return seed -> {
            costsRandom.setSeed(seed);
            for (int slot = 0; slot < 3; slot++)
                if (SeedCandidates.cost(costsRandom, slot, shelves) != costs[slot]) return false;
            for (int slot = 0; slot < 3; slot++) {
                if (costs[slot] == 0) continue;
                var list = offer(seed, slot, costs[slot]);
                if (list.isEmpty()) {
                    if (clues[slot] != -1 || levels[slot] != -1) return false;
                } else {
                    var clue = list.get(random.nextInt(list.size()));
                    if (registry.asHolderIdMap().getId(clue.enchantment()) != clues[slot] || clue.level() != levels[slot]) return false;
                }
            }
            return true;
        };
    }
    public Plan search(Integer currentSeed, OptionalLong playerSeed, Map<String, Integer> wanted, int maxDrops, int availableLevels) {
        if (currentSeed != null) {
            Plan direct = find(currentSeed, -1, wanted, availableLevels);
            if (direct != null) return direct;
        }
        if (playerSeed.isEmpty()) return null;
        Lcg48 throwRandom = new Lcg48(playerSeed.getAsLong());
        for (int drops = 0; drops <= maxDrops; drops++) {
            if (Thread.currentThread().isInterrupted()) throw new CancellationException();
            Lcg48 dummyRandom = new Lcg48(throwRandom.state());
            Plan plan = find(dummyRandom.nextInt(), drops, wanted, availableLevels - 1);
            if (plan != null) return plan;
            throwRandom.advance(4);
        }
        return null;
    }
    private Plan find(int seed, int drops, Map<String, Integer> wanted, int availableLevels) {
        for (int shelves = 0; shelves <= 15; shelves++) {
            costsRandom.setSeed(seed);
            for (int slot = 0; slot < 3; slot++) {
                int cost = SeedCandidates.cost(costsRandom, slot, shelves);
                if (cost == 0 || cost > availableLevels) continue;
                var result = offer(seed, slot, cost);
                boolean matches = wanted.entrySet().stream().allMatch(want -> result.stream().anyMatch(enchantment ->
                    registry.getKey(enchantment.enchantment().value()).toString().equals(want.getKey()) && enchantment.level() >= want.getValue()));
                if (matches) return new Plan(Math.max(0, drops), shelves, slot, cost, drops >= 0, describe(result));
            }
        }
        return null;
    }
    public static String describe(List<EnchantmentInstance> enchantments) {
        return enchantments.isEmpty() ? "No enchantments" : enchantments.stream()
            .map(enchantment -> Enchantment.getFullname(enchantment.enchantment(), enchantment.level()).getString())
            .collect(Collectors.joining(", "));
    }
}
