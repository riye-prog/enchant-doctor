package dev.doctrine.client;

import dev.doctrine.core.PlayerSeedTracker;
import dev.doctrine.core.SeedCandidates;
import net.fabricmc.api.ClientModInitializer;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.inventory.EnchantmentScreen;
import net.minecraft.core.BlockPos;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.inventory.EnchantmentMenu;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.ExperienceOrb;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.NetherPortalBlock;
import net.minecraft.world.level.block.EndPortalBlock;
import net.minecraft.world.level.block.EndGatewayBlock;
import net.minecraft.world.level.block.EnchantingTableBlock;
import net.minecraft.world.phys.Vec3;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.OptionalLong;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.network.protocol.game.ClientboundTakeItemEntityPacket;
import net.minecraft.network.protocol.game.ClientboundRespawnPacket;
import net.minecraft.network.chat.Component;

public final class DoctrineClient implements ClientModInitializer {
    public static final DoctrineClient INSTANCE = new DoctrineClient();
    private static final Logger LOGGER = LoggerFactory.getLogger("doctrine");
    private final ExecutorService worker = Executors.newSingleThreadExecutor(task -> {
        Thread thread = new Thread(task, "Doctrine seed search");
        thread.setDaemon(true);
        thread.setPriority(Thread.MIN_PRIORITY);
        return thread;
    });
    private final PlayerSeedTracker tracker = new PlayerSeedTracker();
    private final DropRecovery recovery = new DropRecovery(worker, tracker);
    private final FastDropTask fastDrops = new FastDropTask(tracker);
    private Future<?> job;
    private boolean searching;
    private SeedCandidates candidates;
    private Observation observed;
    private Observation pending;
    private BlockPos table;
    private Object world;
    private Vec3 position;
    private int stableTicks;
    private int menuId = -1;
    private long started;
    private long elapsedMillis;
    private ItemStack beforeEnchant;
    private boolean awaitingEnchant;
    private String status = "Open an enchanting table and insert an unenchanted item.";
    private String route = "Crack the table seed to search its offers. Recover player RNG to search future seeds.";
    private String activity = "Ready to observe";
    private EnchantModel.Plan selectedPlan;
    private OptionalLong planState = OptionalLong.empty();
    private int remainingDrops;
    private int overlayTicks;
    private String lastOverlay = "";
    private boolean sharedRandomServer;
    private ItemStack[] equipment;
    private static final EquipmentSlot[] TRACKED_EQUIPMENT = {EquipmentSlot.HEAD, EquipmentSlot.CHEST, EquipmentSlot.LEGS, EquipmentSlot.FEET, EquipmentSlot.OFFHAND};
    private boolean publishingDirty = true;
    private int hazardWarningTicks;
    private String hazardWarning = "";

    private record Observation(int reported, int shelves, ItemStack item, int[] costs, int[] clues, int[] levels) {
        boolean same(Observation other) {
            return other != null && reported == other.reported && shelves == other.shelves && ItemStack.matches(item, other.item)
                && Arrays.equals(costs, other.costs) && Arrays.equals(clues, other.clues) && Arrays.equals(levels, other.levels);
        }
    }

    @Override public void onInitializeClient() { DoctrineKeys.register(); LOGGER.info("Doctrine ready. Press F8 to open the enchantment workbench."); }
    public void open() {
        Minecraft client = Minecraft.getInstance();
        if (client.gui.screen() instanceof WorkbenchScreen screen) screen.onClose();
        else if (client.player != null && (client.gui.screen() == null || client.gui.screen() instanceof EnchantmentScreen)) client.gui.setScreen(new WorkbenchScreen(client.gui.screen()));
    }
    public void tableUsed(BlockPos pos) {
        Minecraft client = Minecraft.getInstance();
        if (client.level != null && client.level.getBlockState(pos).is(Blocks.ENCHANTING_TABLE)) table = pos.immutable();
    }
    public boolean guard(String action) {
        if (!trackingPlayerRng()) return false;
        var player = Minecraft.getInstance().player;
        if (player != null && player.isShiftKeyDown()) {
            invalidatePlayer(action + " allowed while sneaking. Recover a fresh seed.");
            warn("Doctrine: RNG protection bypassed; recover your seed.");
            return false;
        }
        warn("Doctrine blocked " + action + " to protect your player RNG. Sneak to override.");
        return true;
    }
    public void warn(String message) {
        hazardWarning = message;
        hazardWarningTicks = 80;
        showOverlay(message);
    }
    public boolean guardCommand(String command) {
        return RngHazards.killsPlayer(command) && guard("/kill (death recreates the server player)");
    }
    public void enchantRequested(int containerId, int button) {
        var player = Minecraft.getInstance().player;
        if (player == null) return;
        if (!(player.containerMenu instanceof EnchantmentMenu menu) || menu.containerId != containerId || button < 0 || button > 2) return;
        if (menu.costs[button] == 0 || menu.getSlot(0).getItem().isEmpty()) return;
        beforeEnchant = menu.getSlot(0).getItem().copy();
        awaitingEnchant = true;
        cancel();
    }
    public void dropped() {
        if (recovery.active()) { recovery.packetDropped(); return; }
        if (fastDrops.packetDropped()) return;
        tracker.dropped();
        cancelSearch();
    }
    public void invalidatePlayer(String reason) {
        if (recovery.active()) recovery.cancel(reason);
        if (fastDrops.active()) fastDrops.cancel(reason);
        tracker.invalidate(reason);
        cancelSearch();
    }
    private void cancelSearch() {
        if (searching) cancel();
        selectedPlan = null;
        remainingDrops = 0;
        route = "Player RNG changed. Find a new route before enchanting.";
    }
    public void shutdown() { recovery.cancel("Client closed."); fastDrops.cancel("Client closed."); cancel(); worker.shutdownNow(); }
    public void itemSpawned(ClientboundAddEntityPacket packet) { recovery.spawned(packet); fastDrops.spawned(packet); }
    public void itemTaken(ClientboundTakeItemEntityPacket packet) {
        var client = Minecraft.getInstance();
        if (trackingPlayerRng() && client.player != null && client.level != null && packet.getPlayerId() == client.player.getId()
                && client.level.getEntity(packet.getItemId()) instanceof ExperienceOrb)
            invalidatePlayer("Experience pickup can consume player RNG through mending.");
        fastDrops.pickedUp(packet);
    }
    public void respawned(ClientboundRespawnPacket packet) {
        invalidatePlayer("Respawn or dimension transition: verify the server-side player RNG again.");
    }
    private boolean trackingPlayerRng() { return tracker.state().isPresent() || awaitingEnchant || recovery.active() || fastDrops.active(); }
    private void cancel() {
        if (job != null && !searching) observed = null;
        if (job != null) job.cancel(true);
        job = null;
        searching = false;
        activity = "Idle";
    }
    private void reset() {
        recovery.cancel("Hold 12 disposable items to recover your player seed.");
        cancel();
        candidates = null;
        observed = pending = null;
        awaitingEnchant = false;
        beforeEnchant = null;
        stableTicks = 0;
        selectedPlan = null;
        remainingDrops = 0;
        fastDrops.cancel("No drop sequence is running.");
        publishingDirty = true;
        tracker.invalidate("Enchant once to start tracking fresh seeds.");
        status = "Insert an unenchanted item to read the table.";
        route = "Observe the table, then choose a target enchantment.";
    }
    public void tick() {
        Minecraft client = Minecraft.getInstance();
        if (world != client.level) {
            boolean changedWorld = world != null && client.level != null;
            reset();
            if (changedWorld) tracker.invalidate("World transition: verify the server player RNG again.");
            world = client.level;
            table = null;
            position = null;
            menuId = -1;
            sharedRandomServer = false;
            equipment = null;
        }
        if (client.player == null) return;
        boolean nextSharedRandomServer = ServerCompatibility.usesSharedEntityRandom();
        if (nextSharedRandomServer && !sharedRandomServer) {
            if (recovery.active()) recovery.cancel(ServerCompatibility.limitation());
            if (fastDrops.active()) fastDrops.cancel(ServerCompatibility.limitation());
            tracker.invalidate(ServerCompatibility.limitation());
            route = "Paper-compatible mode uses table observations to reveal current offers. Future-seed routing is unavailable.";
            publishingDirty = true;
        }
        sharedRandomServer = nextSharedRandomServer;
        var player = client.player;
        ItemStack[] nextEquipment = new ItemStack[TRACKED_EQUIPMENT.length];
        boolean equipmentChanged = false;
        for (int i = 0; i < TRACKED_EQUIPMENT.length; i++) {
            nextEquipment[i] = player.getItemBySlot(TRACKED_EQUIPMENT[i]).copy();
            if (equipment != null && !ItemStack.matches(equipment[i], nextEquipment[i])) equipmentChanged = true;
        }
        equipment = nextEquipment;
        if (equipmentChanged && (tracker.state().isPresent() || awaitingEnchant || recovery.active() || fastDrops.active()))
            invalidatePlayer("Equipping or damaging worn items invalidated player RNG tracking.");
        Vec3 nextPosition = player.position();
        if (position != null && (player.isUsingItem() || player.isInWater() || player.isOnFire())) {
            if (tracker.state().isPresent() || awaitingEnchant || recovery.active()) invalidatePlayer("RNG-consuming player activity invalidated tracking. Recover a fresh seed.");
            else tracker.invalidate("Avoid RNG-consuming activity while recovering consecutive player seeds.");
        }
        position = nextPosition;
        if (hazardWarningTicks > 0) --hazardWarningTicks;
        if (trackingPlayerRng() && player.isDeadOrDying()) {
            invalidatePlayer("Death recreates the server player and resets its RNG. Recover again after respawning.");
            warn("Doctrine: death reset the player RNG. Recover again after respawning.");
        }
        if (trackingPlayerRng() && client.level != null) {
            BlockPos feet = player.blockPosition();
            boolean nearPortal = false;
            for (BlockPos nearby : BlockPos.betweenClosed(feet.offset(-1, -1, -1), feet.offset(1, 1, 1))) {
                var block = client.level.getBlockState(nearby).getBlock();
                if (block instanceof NetherPortalBlock || block instanceof EndPortalBlock || block instanceof EndGatewayBlock) {
                    nearPortal = true;
                    break;
                }
            }
            if (nearPortal && hazardWarningTicks == 0)
                warn("Doctrine: portal nearby! Dimension travel may interrupt RNG tracking.");
            if (player.getHealth() <= 6 && hazardWarningTicks == 0)
                warn("Doctrine: low health! Death recreates the player RNG; get safe.");
        }
        boolean recoveryWasActive = recovery.active();
        recovery.tick();
        if (recoveryWasActive && !recovery.active()) showOverlay(recovery.status());
        boolean dropsWereActive = fastDrops.active();
        fastDrops.tick();
        if (fastDrops.active()) remainingDrops = fastDrops.remaining();
        boolean dropsCompleted = fastDrops.consumeCompleted();
        if (dropsCompleted && selectedPlan != null) {
            remainingDrops = 0;
            activity = "Drop sequence completed";
            route = "Drops completed. Perform one level-one dummy enchantment, then insert your target item.\n"
                + selectedPlan.targetDescription();
            showOverlay(fastDrops.status());
        } else if (dropsWereActive && !fastDrops.active()) {
            remainingDrops = 0;
            selectedPlan = null;
            activity = "Drop sequence stopped";
            route = fastDrops.status();
            showOverlay(fastDrops.status());
        }
        publishOverlay(player);
        if (player.containerMenu instanceof EnchantmentMenu menu) {
            if (menu.containerId != menuId) {
                cancel();
                candidates = null;
                observed = pending = null;
                publishingDirty = true;
                menuId = menu.containerId;
            }
            if (awaitingEnchant && !ItemStack.matches(beforeEnchant, menu.getSlot(0).getItem()) && !menu.getSlot(0).getItem().isEmpty() && !menu.getSlot(0).getItem().isEnchantable()) {
                awaitingEnchant = false;
                if (sharedRandomServer) tracker.invalidate(ServerCompatibility.limitation());
                else tracker.enchantRequested();
                candidates = null;
                observed = pending = null;
                publishingDirty = true;
                status = "Enchantment confirmed. Insert a fresh item to recover the new seed.";
            }
            if (!awaitingEnchant) observe(menu);
        } else if (menuId != -1) {
            cancel();
            menuId = -1;
            observed = pending = null;
            candidates = null;
            publishingDirty = true;
            awaitingEnchant = false;
            status = "Open an enchanting table to observe its offers.";
        }
        finishJob();
        if (NativeUi.ready()) {
            for (int i = 0; i < 16; i++) {
                String action = NativeUi.pollAction();
                if (action == null) break;
                action(action);
            }
            publish();
        }
    }
    private void observe(EnchantmentMenu menu) {
        Minecraft client = Minecraft.getInstance();
        if (table == null || client.level == null || !client.level.getBlockState(table).is(Blocks.ENCHANTING_TABLE)) return;
        ItemStack item = menu.getSlot(0).getItem();
        if (item.isEmpty() || !item.isEnchantable() || Arrays.stream(menu.costs).allMatch(cost -> cost == 0)) return;
        int shelves = 0;
        for (var offset : EnchantingTableBlock.BOOKSHELF_OFFSETS)
            if (EnchantingTableBlock.isValidBookShelf(client.level, table, offset)) shelves++;
        Observation next = new Observation(menu.getEnchantmentSeed() & 0xfff0, Math.min(15, shelves), item.copy(), menu.costs.clone(), menu.enchantClue.clone(), menu.levelClue.clone());
        if (!next.same(pending)) {
            pending = next;
            stableTicks = 0;
            if (job != null) cancel();
            return;
        }
        if (++stableTicks < 3 || next.same(observed) || searching) return;
        if (observed == null || observed.reported != next.reported) candidates = null;
        observed = next;
        publishingDirty = true;
        var model = new EnchantModel(client.level.registryAccess().lookupOrThrow(Registries.ENCHANTMENT), next.item);
        SeedCandidates source = candidates;
        started = System.nanoTime();
        activity = "Checking table seed candidates";
        status = "Reading the table. The search runs in the background.";
        job = worker.submit(() -> (source == null ? SeedCandidates.initial(next.reported) : source).filter(model.matcher(next.shelves, next.costs, next.clues, next.levels)));
    }
    private void finishJob() {
        if (job == null || !job.isDone()) return;
        try {
            Object result = job.get();
            elapsedMillis = (System.nanoTime() - started) / 1_000_000;
            if (searching) {
                selectedPlan = result instanceof EnchantModel.Plan plan ? plan : null;
                remainingDrops = selectedPlan == null ? 0 : selectedPlan.drops();
                planState = tracker.state();
                route = selectedPlan != null ? selectedPlan.describe() : "No single-table offer within these limits. Increase the drop limit or choose a different target.";
                activity = "Search completed in " + elapsedMillis + " ms";
            } else {
                candidates = (SeedCandidates) result;
                publishingDirty = true;
                if (candidates.size() == 0) {
                    status = "No matching seed. Check the bookshelves and server rules, then reset.";
                    tracker.invalidate("Table observations do not match vanilla enchanting.");
                } else if (candidates.size() == 1) {
                    status = "Table seed recovered. All three offers are now predictable.";
                    tracker.cracked(candidates.unique());
                } else status = "Swap to another unenchanted item to narrow the remaining seeds.";
                activity = "Observation completed in " + elapsedMillis + " ms";
            }
        } catch (Exception exception) {
            Throwable cause = exception.getCause();
            if (searching && cause instanceof IllegalArgumentException) {
                route = cause.getMessage();
                activity = "Invalid target";
            } else {
                status = "Search interrupted or failed. Read the table again.";
                LOGGER.error("Doctrine search failed", exception);
                observed = null;
            }
        }
        job = null;
        searching = false;
    }
    public void action(String action) {
        Minecraft client = Minecraft.getInstance();
        switch (action) {
            case "close" -> { if (client.gui.screen() instanceof WorkbenchScreen screen) screen.onClose(); }
            case "reset" -> reset();
            case "observe" -> { cancel(); observed = null; }
            case "cancel" -> { recovery.cancel("Recovery cancelled. No more items will be dropped."); fastDrops.cancel("Drop sequence cancelled. Player RNG must be recovered again."); cancel(); selectedPlan = null; remainingDrops = 0; observed = null; route = "Search or item dropping cancelled. Find a new route to continue."; }
            case "copy" -> client.keyboardHandler.setClipboard(route);
            case "search" -> search();
            case "drop" -> beginDrops();
            case "recover" -> beginRecovery();
            case "controls" -> client.gui.setScreen(new net.minecraft.client.gui.screens.options.controls.KeyBindsScreen(client.gui.screen(), client.options));
            default -> { }
        }
    }
    private void search() {
        Minecraft client = Minecraft.getInstance();
        if (client.level == null || recovery.active() || ((candidates == null || candidates.size() != 1) && tracker.state().isEmpty())) {
            route = "Recover the player seed or the table seed before searching.";
            return;
        }
        try {
            Identifier itemId = Identifier.parse(NativeUi.value("item").trim());
            if (!BuiltInRegistries.ITEM.containsKey(itemId)) throw new IllegalArgumentException("Unknown item identifier.");
            ItemStack item = new ItemStack(BuiltInRegistries.ITEM.getValue(itemId));
            if (!item.isEnchantable()) throw new IllegalArgumentException("Choose an enchantable item.");
            var registry = client.level.registryAccess().lookupOrThrow(Registries.ENCHANTMENT);
            Map<String, Integer> wanted = new LinkedHashMap<>();
            for (String entry : NativeUi.value("wanted").split(",")) {
                String text = entry.trim();
                int separator = text.lastIndexOf(':');
                if (separator < 1) throw new IllegalArgumentException("Use enchantment:level, separated by commas.");
                Identifier id = Identifier.parse(text.substring(0, separator));
                int level = Integer.parseInt(text.substring(separator + 1));
                Enchantment enchantment = registry.getValue(id);
                if (enchantment == null || level < 1 || level > enchantment.getMaxLevel()) throw new IllegalArgumentException("Unknown enchantment or invalid level: " + id);
                wanted.put(id.toString(), level);
            }
            if (wanted.isEmpty()) throw new IllegalArgumentException("Enter at least one enchantment.");
            int maxDrops = Integer.parseInt(NativeUi.value("max-drops").trim());
            int levels = Integer.parseInt(NativeUi.value("levels").trim());
            if (maxDrops < 0 || maxDrops > 100000 || levels < 1 || levels > 999) throw new IllegalArgumentException("Use 0..100000 drops and 1..999 levels.");
            cancel();
            selectedPlan = null;
            var model = new EnchantModel(registry, item);
            String impossible = model.unavailableReason(wanted);
            if (impossible != null) { route = impossible; return; }
            var playerSeed = tracker.state();
            Integer seed = candidates != null && candidates.size() == 1 ? candidates.unique() : null;
            searching = true;
            started = System.nanoTime();
            activity = "Searching target enchantments";
            route = playerSeed.isPresent() ? "Searching current offers and future seeds..." : "Searching current offers. Recover player RNG to include future seeds.";
            job = worker.submit(() -> model.search(seed, playerSeed, Map.copyOf(wanted), maxDrops, levels));
        } catch (IllegalArgumentException exception) { route = exception.getMessage(); }
    }
    private void publish() {
        NativeUi.update("status", status);
        NativeUi.update("activity", recovery.active() ? "Recovering Player Seed" : activity);
        NativeUi.update("tracking", tracker.reason());
        NativeUi.update("server-mode", ServerCompatibility.mode());
        NativeUi.update("recovery-status", recovery.status());
        NativeUi.update("recovery-count", recovery.progress() + " / 12");
        NativeUi.update("player-seed", tracker.state().isPresent() ? String.format("%012X", tracker.state().getAsLong()) : "Not recovered");
        NativeUi.update("player-state", tracker.state().isPresent() ? "Verified" : recovery.active() ? "Recovering" : "Awaiting recovery");
        NativeUi.progress(recovery.progress());
        NativeUi.update("key-toggle", DoctrineKeys.label("toggle"));
        NativeUi.update("key-recover", DoctrineKeys.label("recover"));
        NativeUi.update("key-planner", DoctrineKeys.label("planner"));
        NativeUi.update("key-cancel", DoctrineKeys.label("cancel"));
        NativeUi.update("key-observe", DoctrineKeys.label("observe"));
        NativeUi.update("key-drop", DoctrineKeys.label("drop"));
        NativeUi.update("key-drop-sidebar", DoctrineKeys.label("drop"));
        NativeUi.update("route", route);
        NativeUi.update("drop-label", fastDrops.active() ? "Dropping " + fastDrops.remaining() + " remaining" : remainingDrops > 0 ? "Run all " + remainingDrops + " drops" : "Run planned drops");
        NativeUi.update("drop-status", fastDrops.active() ? fastDrops.status() : "Items are recovered from the ground and reused automatically.");
        NativeUi.update("candidates", candidates == null ? "No observation" : String.format("%,d possible seed%s", candidates.size(), candidates.size() == 1 ? "" : "s"));
        NativeUi.update("seed", candidates != null && candidates.size() == 1 ? String.format("Seed %08X", candidates.unique()) : "Seed unknown");
        NativeUi.update("shelves", observed == null ? "Bookshelves: unknown" : observed.shelves + " active bookshelves");
        NativeUi.update("timing", elapsedMillis == 0 ? "Waiting for a table" : "Last search: " + elapsedMillis + " ms");
        if (!publishingDirty) return;
        publishingDirty = false;
        if (observed == null) {
            for (int i = 0; i < 3; i++) {
                NativeUi.update("cost" + i, "No offer");
                NativeUi.update("offer" + i, "Insert an unenchanted item.");
            }
            return;
        }
        Minecraft client = Minecraft.getInstance();
        var registry = client.level.registryAccess().lookupOrThrow(Registries.ENCHANTMENT);
        var model = new EnchantModel(registry, observed.item);
        for (int i = 0; i < 3; i++) {
            NativeUi.update("cost" + i, "Level " + observed.costs[i] + " / " + (i + 1) + " lapis");
            String text;
            if (candidates != null && candidates.size() == 1) text = EnchantModel.describe(model.offer(candidates.unique(), i, observed.costs[i]));
            else {
                var clue = registry.asHolderIdMap().byId(observed.clues[i]);
                text = clue == null ? "No visible clue" : Enchantment.getFullname(clue, observed.levels[i]).getString() + " / server clue";
            }
            NativeUi.update("offer" + i, text);
        }
    }

    private void beginDrops() {
        if (fastDrops.active()) {
            route = fastDrops.status();
            return;
        }
        var player = Minecraft.getInstance().player;
        if (selectedPlan == null || !selectedPlan.dummy() || remainingDrops == 0 || player == null) {
            route = "Find a route with item drops first.";
            return;
        }
        if (tracker.state().isEmpty() || !tracker.state().equals(planState)) {
            cancelSearch();
            return;
        }
        returnToGame();
        if (fastDrops.start(remainingDrops, planState)) {
            activity = "Fast drop sequence active";
            route = fastDrops.status();
        } else route = fastDrops.status() + "\n" + selectedPlan.describe();
    }

    private void beginRecovery() {
        Minecraft client = Minecraft.getInstance();
        var player = client.player;
        if (player == null) return;
        if (ServerCompatibility.usesSharedEntityRandom()) {
            route = ServerCompatibility.limitation();
            status = "Observe an enchanting table to recover its current seed on Paper.";
            showOverlay("Doctrine: Paper shared RNG detected. Current table offers remain available.");
            return;
        }
        cancel();
        selectedPlan = null;
        remainingDrops = 0;
        returnToGame();
        if (recovery.start()) {
            activity = "Fast player-seed recovery active";
            route = "Recovery is running in-game. Stay still; progress appears above the hotbar.";
        }
    }

    private void returnToGame() {
        Minecraft client = Minecraft.getInstance();
        if (client.player != null && client.player.containerMenu != client.player.inventoryMenu) client.player.closeContainer();
        if (client.gui.screen() != null) client.gui.setScreen(null);
    }

    private void publishOverlay(net.minecraft.client.player.LocalPlayer player) {
        String message = hazardWarningTicks > 0 ? hazardWarning
            : recovery.active() ? recovery.status() + "  |  " + DoctrineKeys.label("cancel") + " cancels"
            : fastDrops.active() ? fastDrops.status() + "  |  " + DoctrineKeys.label("cancel") + " cancels" : "";
        if (message.isEmpty()) {
            lastOverlay = "";
            overlayTicks = 0;
            return;
        }
        if (!message.equals(lastOverlay) || overlayTicks++ >= 10) {
            showOverlay(message);
            lastOverlay = message;
            overlayTicks = 0;
        }
    }

    private void showOverlay(String message) {
        Minecraft.getInstance().gui.chatListener().handleOverlay(Component.literal(message));
    }
}
