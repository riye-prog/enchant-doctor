package dev.doctrine.core;

import java.util.OptionalLong;

public final class PlayerSeedTracker {
    private Integer first;
    private OptionalLong state = OptionalLong.empty();
    private boolean awaitingFreshSeed;
    private String reason = "Enchant once to start tracking fresh seeds.";

    public OptionalLong state() { return state; }
    public String reason() { return reason; }
    public void recoveredFromDrops(long seed) {
        first = null;
        awaitingFreshSeed = false;
        state = OptionalLong.of(seed & Lcg48.MASK);
        reason = "Player RNG recovered and validated from item drops.";
    }
    public void invalidate(String reason) {
        first = null;
        state = OptionalLong.empty();
        awaitingFreshSeed = false;
        this.reason = reason;
    }
    public void enchantRequested() { awaitingFreshSeed = true; }
    public void cracked(int seed) {
        if (!awaitingFreshSeed) return;
        awaitingFreshSeed = false;
        if (state.isPresent()) {
            Lcg48 random = new Lcg48(state.getAsLong());
            if (random.nextInt() != seed) {
                invalidate("The server seed changed unexpectedly. Start a fresh pair.");
                return;
            }
            state = OptionalLong.of(random.state());
        } else if (first == null) {
            first = seed;
            reason = "First fresh seed found. Enchant again, then observe the next seed.";
        } else {
            state = Lcg48.recover(first, seed);
            first = null;
            reason = state.isPresent() ? "Player RNG recovered. Stay still while planning." : "Seeds were not consecutive. Start a fresh pair.";
        }
    }
    public void dropped() {
        if (awaitingFreshSeed || state.isEmpty()) {
            invalidate("An item was dropped before player RNG recovery finished.");
            return;
        }
        Lcg48 random = new Lcg48(state.getAsLong());
        random.advance(4);
        state = OptionalLong.of(random.state());
    }
}
