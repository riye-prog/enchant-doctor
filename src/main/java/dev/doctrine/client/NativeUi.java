package dev.doctrine.client;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.HashMap;
import java.util.Map;

public final class NativeUi {
    private static final Map<String, String> texts = new HashMap<>();
    private static Path directory;
    private static boolean ready;
    private static int pixelWidth;
    private static int pixelHeight;

    private NativeUi() {}
    public static boolean ready() { return ready; }
    public static int pixelWidth() { return pixelWidth; }
    public static int pixelHeight() { return pixelHeight; }
    public static void ensure(long instance, long physical, long device, long loader, int format, int width, int height, float scale) {
        if (ready) return;
        try {
            if (directory == null) {
                String osName = System.getProperty("os.name").toLowerCase();
                String os = osName.contains("mac") ? "macos" : osName.contains("win") ? "windows" : "linux";
                String arch = System.getProperty("os.arch").matches("aarch64|arm64") ? "arm64" : "x86_64";
                String library = os.equals("macos") ? "libdoctrine.dylib" : os.equals("windows") ? "doctrine.dll" : "libdoctrine.so";
                directory = Files.createTempDirectory("doctrine-native-");
                directory.toFile().deleteOnExit();
                for (String file : new String[]{library, "ui.vert.spv", "ui.frag.spv"}) extract("/natives/" + os + "-" + arch + "/" + file, file);
                for (String file : new String[]{"workbench.rml", "workbench.rcss", "PlexSans.ttf"}) extract("/ui/" + file, file);
                System.load(directory.resolve(library).toAbsolutePath().toString());
            }
            initialize(instance, physical, device, loader, format, directory.toString(), width, height, scale);
            ready = true;
        } catch (IOException exception) {
            throw new IllegalStateException("Doctrine native library is missing. Build native/CMakeLists.txt for this platform before packaging the mod.", exception);
        }
    }
    private static void extract(String resource, String name) throws IOException {
        try (var stream = NativeUi.class.getResourceAsStream(resource)) {
            if (stream == null) throw new IOException("Missing resource " + resource);
            Path target = directory.resolve(name);
            Files.copy(stream, target, StandardCopyOption.REPLACE_EXISTING);
            target.toFile().deleteOnExit();
        }
    }
    public static void update(String id, String value) {
        if (ready && !value.equals(texts.get(id))) {
            text(id, value);
            texts.put(id, value);
        }
    }
    public static void close() {
        if (ready) shutdown();
        ready = false;
        texts.clear();
    }
    private static native void initialize(long instance, long physical, long device, long loader, int format, String directory, int width, int height, float scale);
    public static void frame(long command, long image, int width, int height, boolean show, float scale) {
        pixelWidth = width;
        pixelHeight = height;
        renderFrame(command, image, width, height, show, scale);
    }
    private static native void renderFrame(long command, long image, int width, int height, boolean show, float scale);
    public static native void selectTab(String tab);
    public static native void progress(int completed);
    public static native void setClipboard(String text);
    public static native String takeClipboard();
    public static native void resize();
    private static native void shutdown();
    private static native void text(String id, String value);
    public static native String value(String id);
    public static native String pollAction();
    public static void input(int type, double x, double y, int code, int modifiers) {
        int normalized = type == 4 || type == 5 ? InputBridge.key(code) : type == 1 || type == 2 ? InputBridge.button(code) : code;
        dispatchInput(type, x, y, normalized, InputBridge.modifiers(modifiers));
    }
    private static native void dispatchInput(int type, double x, double y, int code, int modifiers);
}
