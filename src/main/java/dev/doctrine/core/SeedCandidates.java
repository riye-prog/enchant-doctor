package dev.doctrine.core;

import java.util.Arrays;
import java.util.concurrent.CancellationException;
import java.util.function.IntPredicate;

public final class SeedCandidates {
    private final int[] seeds;

    private SeedCandidates(int[] seeds) { this.seeds = seeds; }
    public static SeedCandidates initial(int reported) {
        int[] values = new int[1 << 20];
        int known = reported & 0xfff0;
        for (int i = 0; i < values.length; i++) values[i] = ((i >>> 4) << 16) | known | (i & 15);
        return new SeedCandidates(values);
    }
    public SeedCandidates filter(IntPredicate matches) {
        int[] accepted = new int[seeds.length];
        int count = 0;
        for (int i = 0; i < seeds.length; i++) {
            if ((i & 4095) == 0 && Thread.currentThread().isInterrupted()) throw new CancellationException();
            if (matches.test(seeds[i])) accepted[count++] = seeds[i];
        }
        return new SeedCandidates(Arrays.copyOf(accepted, count));
    }
    public int size() { return seeds.length; }
    public int unique() {
        if (seeds.length != 1) throw new IllegalStateException("seed is not unique");
        return seeds[0];
    }
    public static int cost(Lcg48 random, int slot, int shelves) {
        int power = Math.clamp(shelves, 0, 15);
        int selected = random.nextInt(8) + 1 + (power >> 1) + random.nextInt(power + 1);
        int cost = switch (slot) {
            case 0 -> Math.max(selected / 3, 1);
            case 1 -> selected * 2 / 3 + 1;
            case 2 -> Math.max(selected, power * 2);
            default -> throw new IllegalArgumentException("slot must be 0..2");
        };
        return cost < slot + 1 ? 0 : cost;
    }
}
