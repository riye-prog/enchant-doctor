package dev.doctrine.client;

import dev.doctrine.core.PlayerSeedTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.network.protocol.game.ClientboundTakeItemEntityPacket;
import net.minecraft.network.protocol.game.ServerboundMovePlayerPacket;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.phys.Vec3;
import java.util.HashSet;
import java.util.OptionalLong;
import java.util.Set;

public final class FastDropTask {
    private static final int MAX_PENDING = 128;
    // A normal dropped item cannot be picked up for 40 ticks. Keeping room for
    // 40 full bursts avoids collapsing to pickup-speed after the first burst,
    // while retaining a hard ceiling if pickups stop entirely.
    private static final int MAX_DROPS_PER_TICK = 16;
    private static final int MAX_IN_FLIGHT = MAX_DROPS_PER_TICK * 40;
    private final PlayerSeedTracker tracker;
    private final Set<Integer> droppedEntities = new HashSet<>();
    private OptionalLong expectedState = OptionalLong.empty();
    private Object level;
    private Vec3 origin;
    private Vec3 expectedSpawn;
    private int target;
    private int remaining;
    private int sent;
    private int confirmed;
    private int pending;
    private int recycled;
    private int waitTicks;
    private int originalSlot;
    private float originalPitch;
    private float lockedYaw;
    private boolean active;
    private boolean issuing;
    private boolean completed;
    private String status = "No drop sequence is running.";

    public FastDropTask(PlayerSeedTracker tracker) { this.tracker = tracker; }
    public boolean active() { return active; }
    public int remaining() { return remaining; }
    public int recycled() { return recycled; }
    public String status() { return status; }
    public boolean consumeCompleted() {
        boolean value = completed;
        completed = false;
        return value;
    }
    public boolean start(int count, OptionalLong state) {
        var client = Minecraft.getInstance();
        var player = client.player;
        if (player == null || client.gameMode == null || state.isEmpty()) {
            status = "Recover the player seed before starting a drop sequence.";
            return false;
        }
        if (player.containerMenu != player.inventoryMenu) {
            status = "Close the current container before starting the drop sequence.";
            return false;
        }
        if (DropSlots.countEligible(player) == 0) {
            status = "Keep stackable, unenchanted disposable items anywhere in your inventory.";
            return false;
        }
        if (!player.onGround() || player.isInWater() || player.isOnFire() || player.isUsingItem()) {
            status = "Stand still on dry ground and stop using items first.";
            return false;
        }
        target = remaining = count;
        sent = confirmed = pending = recycled = waitTicks = 0;
        droppedEntities.clear();
        expectedState = state;
        origin = player.position();
        level = player.level();
        expectedSpawn = player.getEyePosition().add(0, -0.3, 0);
        originalSlot = player.getInventory().getSelectedSlot();
        originalPitch = player.getXRot();
        lockedYaw = player.getYRot();
        player.setXRot(90);
        player.connection.send(new ServerboundMovePlayerPacket.Rot(lockedYaw, 90, player.onGround(), player.horizontalCollision));
        active = true;
        completed = false;
        status = "Dropping at high speed. Picked-up items will be reused automatically.";
        return true;
    }
    public boolean packetDropped() {
        if (!active) return false;
        if (!issuing) {
            cancel("An untracked item drop interrupted the sequence.");
            return true;
        }
        tracker.dropped();
        expectedState = tracker.state();
        sent++;
        pending++;
        return true;
    }
    public void spawned(ClientboundAddEntityPacket packet) {
        if (!active || pending == 0 || packet.getType() != EntityTypes.ITEM || droppedEntities.contains(packet.getId())) return;
        double dx = packet.getX() - expectedSpawn.x;
        double dz = packet.getZ() - expectedSpawn.z;
        if (dx * dx + dz * dz > 0.04 || Math.abs(packet.getY() - expectedSpawn.y) > 0.2) return;
        droppedEntities.add(packet.getId());
        pending--;
        remaining--;
        confirmed++;
        waitTicks = 0;
        status = confirmed + " of " + target + " drops confirmed; " + recycled + " items recycled.";
    }
    public void pickedUp(ClientboundTakeItemEntityPacket packet) {
        var player = Minecraft.getInstance().player;
        if (!active || player == null || packet.getPlayerId() != player.getId() || !droppedEntities.remove(packet.getItemId())) return;
        recycled += packet.getAmount();
        status = confirmed + " of " + target + " drops confirmed; " + recycled + " items recycled.";
    }
    public void tick() {
        if (!active) return;
        var client = Minecraft.getInstance();
        var player = client.player;
        if (player == null || client.level != level || client.gameMode == null || player.containerMenu != player.inventoryMenu || origin.distanceToSqr(player.position()) > 0.000001
                || player.getXRot() != 90 || player.getYRot() != lockedYaw || !tracker.state().equals(expectedState)) {
            cancel("Movement, camera motion, a menu change, or RNG drift interrupted the drop sequence.");
            return;
        }
        // Item entities can merge or be removed without a take-item packet.
        // Keeping those stale IDs made the in-flight limiter permanently full,
        // reducing long runs to roughly one drop whenever an old ID happened
        // to clear. The add-entity callback runs at TAIL, so confirmed entities
        // are already present in the client level and can be pruned safely.
        droppedEntities.removeIf(id -> client.level.getEntity(id) == null);
        if (remaining == 0) {
            active = false;
            completed = true;
            restoreView();
            status = "All " + target + " drops were confirmed. " + recycled + " picked-up items were reused.";
            return;
        }
        if (pending > 0 && ++waitTicks > 200) {
            cancel("The server stopped confirming item drops. The player seed was cleared.");
            return;
        }
        if (remaining == pending || pending >= MAX_PENDING) return;
        if (pending + droppedEntities.size() >= MAX_IN_FLIGHT) {
            status = "Waiting for dropped items to be picked up before sending the next burst.";
            return;
        }
        int issued = 0;
        Set<Integer> usedSlots = new HashSet<>();
        // Drain the staged stack, then pull the next matching stack from any
        // hotbar or main-inventory slot. The initially selected slot may be
        // empty or unrelated; it is only used as the fixed staging position.
        player.setXRot(90);
        player.connection.send(new ServerboundMovePlayerPacket.Rot(lockedYaw, 90, player.onGround(), player.horizontalCollision));
        while (issued < MAX_DROPS_PER_TICK && pending < MAX_PENDING
                && pending + droppedEntities.size() < MAX_IN_FLIGHT && remaining > pending) {
            int slot = DropSlots.eligible(player.getMainHandItem())
                ? player.getInventory().getSelectedSlot()
                : DropSlots.nextEligibleSlot(player, 0);
            if (slot < 0) break;
            issuing = true;
            boolean dropped;
            try { dropped = DropSlots.dropOne(client, player, slot); }
            finally { issuing = false; }
            if (!dropped) break;
            issued++;
            usedSlots.add(slot);
        }
        if (issued == 0) {
            status = "Waiting for dropped items to be picked up and returned to the inventory.";
            return;
        }
        status = confirmed + " of " + target + " drops confirmed; " + pending + " awaiting server confirmation"
            + " from " + usedSlots.size() + " inventory slot" + (usedSlots.size() == 1 ? "." : "s.");
    }
    public void cancel(String reason) {
        if (!active) {
            status = reason;
            return;
        }
        active = false;
        completed = false;
        restoreView();
        tracker.invalidate(reason);
        status = reason;
    }
    private void restoreView() {
        var player = Minecraft.getInstance().player;
        if (player == null || player.level() != level) return;
        DropSlots.select(player, originalSlot);
        if (player.getXRot() == 90 && player.getYRot() == lockedYaw) {
            player.setXRot(originalPitch);
            player.connection.send(new ServerboundMovePlayerPacket.Rot(lockedYaw, originalPitch, player.onGround(), player.horizontalCollision));
        }
    }
}
