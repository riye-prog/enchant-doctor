package dev.doctrine.core;

import org.junit.jupiter.api.Test;
import java.util.Random;
import static org.junit.jupiter.api.Assertions.*;

class CrackerTest {
    @Test void matchesJavaRandomIncludingRejectionSampling() {
        for (long seed : new long[]{0, -1, Long.MIN_VALUE, 0x123456789abL}) {
            Random expected = new Random(seed);
            Lcg48 actual = new Lcg48(0);
            actual.setSeed(seed);
            for (int i = 0; i < 10000; i++) {
                int bound = new int[]{1, 8, 15, 65537, 1073741825}[i % 5];
                assertEquals(expected.nextInt(bound), actual.nextInt(bound));
                assertEquals(expected.nextInt(), actual.nextInt());
            }
        }
    }
    @Test void jumpMatchesSequentialAndComposes() {
        Lcg48 stepped = new Lcg48(123456);
        Lcg48 jumped = new Lcg48(123456);
        for (int i = 0; i < 100000; i++) stepped.nextInt();
        jumped.advance(100000);
        assertEquals(stepped.state(), jumped.state());
        jumped.advance(1L << 40);
        Lcg48 composed = new Lcg48(123456);
        composed.advance((1L << 40) + 100000);
        assertEquals(composed.state(), jumped.state());
    }
    @Test void recoversSignedOutputPairs() {
        Lcg48 random = new Lcg48(0xabcdef123456L);
        for (int i = 0; i < 100000; i++) {
            int first = random.nextInt();
            int second = random.nextInt();
            assertEquals(random.state(), Lcg48.recover(first, second).orElseThrow());
        }
    }
    @Test void recoveryRejectsNonconsecutivePairs() {
        Random source = new Random(89382);
        for (int i = 0; i < 100; i++) {
            int first = source.nextInt();
            int second = source.nextInt();
            long high = Integer.toUnsignedLong(first) << 16;
            long reference = -1;
            for (int low = 0; low < 65536; low++) {
                long state = ((high | low) * Lcg48.MULTIPLIER + 11) & Lcg48.MASK;
                if ((int) (state >>> 16) == second) reference = state;
            }
            assertEquals(reference, Lcg48.recover(first, second).orElse(-1));
        }
    }
    @Test void candidateMaskIncludesAllUnknownBits() {
        int target = 0xfedcba98;
        SeedCandidates candidates = SeedCandidates.initial(target);
        assertEquals(1 << 20, candidates.size());
        SeedCandidates result = candidates.filter(seed -> seed == target);
        assertEquals(target, result.unique());
        assertEquals(0, candidates.filter(seed -> (seed & 0xfff0) != (target & 0xfff0)).size());
    }
    @Test void ignoresHistoricalSeedAndRequiresFreshPair() {
        PlayerSeedTracker tracker = new PlayerSeedTracker();
        Lcg48 random = new Lcg48(12345);
        tracker.cracked(random.nextInt());
        assertTrue(tracker.state().isEmpty());
        tracker.enchantRequested();
        tracker.cracked(random.nextInt());
        assertTrue(tracker.state().isEmpty());
        tracker.enchantRequested();
        tracker.cracked(random.nextInt());
        assertEquals(random.state(), tracker.state().orElseThrow());
        tracker.dropped();
        random.advance(4);
        assertEquals(random.state(), tracker.state().orElseThrow());
        tracker.invalidate("movement");
        assertTrue(tracker.state().isEmpty());
    }
}
