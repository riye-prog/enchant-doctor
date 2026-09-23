package dev.doctrine.core;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

class DropSeedSolverTest {
    @Test void recoversPostDropStateFromQuantizedVelocityMeasurements() {
        for (long seed : new long[]{1, 0x123456789abcL, 0xffffffffffffL, 0x9876abcdef12L}) {
            Lcg48 random = new Lcg48(seed);
            float[] samples = new float[DropSeedSolver.OBSERVATIONS];
            for (int i = 0; i < samples.length; i++) {
                float angle = random.next(24) * 0x1.0p-24f * (float) (Math.PI * 2);
                float speed = random.next(24) * 0x1.0p-24f * 0.02f;
                double x = ((int) (Math.cos(angle) * speed * 8000)) / 8000.0;
                double z = ((int) (Math.sin(angle) * speed * 8000)) / 8000.0;
                samples[i] = (float) Math.sqrt(x * x + z * z) * 50f;
                random.advance(2);
            }
            assertEquals(random.state(), DropSeedSolver.recover(samples).orElseThrow());
        }
    }
    @Test void rejectsMissingAndNonfiniteObservations() {
        assertThrows(IllegalArgumentException.class, () -> DropSeedSolver.recover(new float[10]));
        float[] samples = new float[12];
        samples[5] = Float.NaN;
        assertThrows(IllegalArgumentException.class, () -> DropSeedSolver.recover(samples));
    }
}
