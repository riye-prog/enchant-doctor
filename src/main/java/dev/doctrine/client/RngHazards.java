package dev.doctrine.client;

import java.util.Locale;
import java.util.regex.Pattern;

public final class RngHazards {
    private static final Pattern KILL = Pattern.compile("(?:kill|execute\\s+.+\\s+run\\s+kill)(?:\\s+.*)?");

    private RngHazards() { }

    public static boolean killsPlayer(String command) {
        String normalized = command.strip().toLowerCase(Locale.ROOT).replace("minecraft:", "");
        return KILL.matcher(normalized).matches();
    }
}
