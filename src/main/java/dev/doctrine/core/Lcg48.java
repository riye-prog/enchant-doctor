package dev.doctrine.core;

import java.util.OptionalLong;

public final class Lcg48 {
    public static final long MULTIPLIER = 0x5deece66dL;
    public static final long MASK = (1L << 48) - 1;
    private long state;

    public Lcg48(long state) { this.state = state & MASK; }
    public long state() { return state; }
    public void setSeed(long seed) { state = (seed ^ MULTIPLIER) & MASK; }
    public int next(int bits) {
        state = (state * MULTIPLIER + 11) & MASK;
        return (int) (state >>> (48 - bits));
    }
    public int nextInt() { return next(32); }
    public int nextInt(int bound) {
        if (bound <= 0) throw new IllegalArgumentException("bound must be positive");
        if ((bound & -bound) == bound) return (int) ((bound * (long) next(31)) >> 31);
        int bits;
        int value;
        do {
            bits = next(31);
            value = bits % bound;
        } while (bits - value + (bound - 1) < 0);
        return value;
    }
    public void advance(long steps) {
        if (steps < 0) throw new IllegalArgumentException("steps must be nonnegative");
        long multiplier = MULTIPLIER;
        long addend = 11;
        long accumulatedMultiplier = 1;
        long accumulatedAddend = 0;
        while (steps != 0) {
            if ((steps & 1) != 0) {
                accumulatedMultiplier = (accumulatedMultiplier * multiplier) & MASK;
                accumulatedAddend = (accumulatedAddend * multiplier + addend) & MASK;
            }
            addend = ((multiplier + 1) * addend) & MASK;
            multiplier = (multiplier * multiplier) & MASK;
            steps >>>= 1;
        }
        state = (accumulatedMultiplier * state + accumulatedAddend) & MASK;
    }
    public static OptionalLong recover(int first, int second) {
        long firstUnsigned = Integer.toUnsignedLong(first);
        long secondUpper = Integer.toUnsignedLong(second) + 1;
        long latticeA = (24667315L * (firstUnsigned + 1) + 18218081L * secondUpper) >> 32;
        long latticeB = (-4824621L * firstUnsigned + 7847617L * secondUpper) >> 32;
        long firstState = (7847617L * latticeA - 18218081L * latticeB) & MASK;
        if ((int) (firstState >>> 16) != first) return OptionalLong.empty();
        long secondState = (firstState * MULTIPLIER + 11) & MASK;
        return (int) (secondState >>> 16) == second ? OptionalLong.of(secondState) : OptionalLong.empty();
    }
}
