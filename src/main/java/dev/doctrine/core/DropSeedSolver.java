package dev.doctrine.core;

import java.util.OptionalLong;
import java.util.concurrent.CancellationException;

public final class DropSeedSolver {
    public static final int OBSERVATIONS = 12;
    public static final float MAX_ERROR = 0.00883889f;
    private DropSeedSolver() {}
    public static OptionalLong recover(float[] samples) {
        if (samples.length != OBSERVATIONS) throw new IllegalArgumentException("Twelve observations are required");
        float[] low = new float[10];
        float[] high = new float[10];
        for (int i = 0; i < samples.length; i++) {
            if (!Float.isFinite(samples[i]) || samples[i] < 0 || samples[i] > 1 + MAX_ERROR) throw new IllegalArgumentException("Invalid drop speed");
            if (i < 10) {
                low[i] = Math.max(0, samples[i] - MAX_ERROR);
                high[i] = Math.min(1, samples[i] + MAX_ERROR);
            }
        }
        long[] candidates = DropLattice.getSeeds(low[0], high[0], low[1], high[1], low[2], high[2], low[3], high[3], low[4], high[4],
            low[5], high[5], low[6], high[6], low[7], high[7], low[8], high[8], low[9], high[9])
            .sequential().filter(seed -> {
                if (Thread.currentThread().isInterrupted()) throw new CancellationException();
                Lcg48 random = new Lcg48(seed);
                for (int i = 10; i < OBSERVATIONS; i++) {
                    random.next(24);
                    float predicted = random.next(24) * 0x1.0p-24f;
                    random.advance(2);
                    if (Math.abs(predicted - samples[i]) > MAX_ERROR) return false;
                }
                return true;
            }).limit(2).toArray();
        if (candidates.length != 1) return OptionalLong.empty();
        Lcg48 result = new Lcg48(candidates[0]);
        result.advance(8);
        return OptionalLong.of(result.state());
    }
}
