package dev.doctrine.client;

import dev.doctrine.core.DropSeedSolver;
import dev.doctrine.core.PlayerSeedTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.network.protocol.game.ServerboundMovePlayerPacket;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.phys.Vec3;
import java.util.HashSet;
import java.util.OptionalLong;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;

public final class DropRecovery {
    private final ExecutorService worker;
    private final PlayerSeedTracker tracker;
    private final float[] samples = new float[DropSeedSolver.OBSERVATIONS];
    private final Set<Integer> entityIds = new HashSet<>();
    private Future<OptionalLong> solving;
    private Vec3 origin;
    private Vec3 expectedSpawn;
    private int received;
    private int sent;
    private int waitTicks;
    private int originalSlot;
    private float originalPitch;
    private float lockedYaw;
    private boolean collecting;
    private boolean issuing;
    private String status = "Hold 12 disposable items to recover your player seed.";

    public DropRecovery(ExecutorService worker, PlayerSeedTracker tracker) { this.worker = worker; this.tracker = tracker; }
    public boolean active() { return collecting || solving != null; }
    public int progress() { return received; }
    public String status() { return status; }
    public boolean start() {
        var client = Minecraft.getInstance();
        var player = client.player;
        if (player == null) { status = "Join a world before starting recovery."; return false; }
        if (player.containerMenu != player.inventoryMenu) { status = "Close the enchanting table first, then reopen Doctrine."; return false; }
        if (DropSlots.countEligible(player) < DropSeedSolver.OBSERVATIONS) {
            status = "Keep at least 12 stackable, unenchanted disposable items anywhere in your inventory.";
            return false;
        }
        if (!player.onGround() || player.isInWater() || player.isOnFire() || player.isUsingItem()) {
            status = "Stand still on dry ground and stop using items first.";
            return false;
        }
        cancel("Starting a new recovery.");
        tracker.invalidate("Recovering player RNG from item velocities.");
        received = sent = waitTicks = 0;
        entityIds.clear();
        origin = player.position();
        originalSlot = player.getInventory().getSelectedSlot();
        originalPitch = player.getXRot();
        lockedYaw = player.getYRot();
        player.setXRot(90);
        player.connection.send(new ServerboundMovePlayerPacket.Rot(lockedYaw, 90, player.onGround(), player.horizontalCollision));
        collecting = true;
        status = "Collecting 12 server-confirmed item velocities in one burst. Stay still.";
        return true;
    }
    public void packetDropped() {
        if (!active()) return;
        if (!issuing) cancel("An unexpected drop interrupted recovery. Start again.");
        else sent++;
    }
    public void cancel(String reason) {
        boolean wasActive = active();
        if (solving != null) solving.cancel(true);
        solving = null;
        if (collecting) restoreAim();
        collecting = issuing = false;
        status = reason;
        if (wasActive) tracker.invalidate(reason);
    }
    private void restoreAim() {
        var player = Minecraft.getInstance().player;
        if (player != null) DropSlots.select(player, originalSlot);
        if (player != null && player.getXRot() == 90 && player.getYRot() == lockedYaw) {
            player.setXRot(originalPitch);
            player.connection.send(new ServerboundMovePlayerPacket.Rot(lockedYaw, originalPitch, player.onGround(), player.horizontalCollision));
        }
    }
    public void tick() {
        if (solving != null) {
            if (!solving.isDone()) return;
            try {
                OptionalLong seed = solving.get();
                if (seed.isPresent()) {
                    tracker.recoveredFromDrops(seed.getAsLong());
                    status = "Player seed recovered. Both validation drops matched.";
                } else {
                    status = "The observations were ambiguous. Check for nearby drops and try again.";
                    tracker.invalidate(status);
                }
            } catch (Exception exception) { status = "Recovery was interrupted. Start a fresh set of drops."; tracker.invalidate(status); }
            solving = null;
            return;
        }
        if (!collecting) return;
        var client = Minecraft.getInstance();
        var player = client.player;
        if (player == null || client.gameMode == null || player.containerMenu != player.inventoryMenu || origin.distanceToSqr(player.position()) > 0.000001
                || player.getXRot() != 90 || player.getYRot() != lockedYaw) {
            cancel("Movement, camera motion, or a menu change interrupted recovery.");
            return;
        }
        if (received == samples.length) {
            collecting = false;
            restoreAim();
            float[] observations = samples.clone();
            solving = worker.submit(() -> DropSeedSolver.recover(observations));
            status = "Solving the player seed and checking two independent drops...";
            return;
        }
        if (sent > received && ++waitTicks > 100) {
            cancel("The server did not confirm the item drops within five seconds.");
            return;
        }
        expectedSpawn = player.getEyePosition().add(0, -0.3, 0);
        player.setXRot(90);
        player.connection.send(new ServerboundMovePlayerPacket.Rot(lockedYaw, 90, player.onGround(), player.horizontalCollision));
        while (sent < samples.length) {
            int slot = DropSlots.eligible(player.getMainHandItem())
                ? player.getInventory().getSelectedSlot()
                : DropSlots.nextEligibleSlot(player, 0);
            if (slot < 0) {
                cancel("Doctrine could not find 12 eligible items across your inventory.");
                return;
            }
            issuing = true;
            boolean dropped;
            try { dropped = DropSlots.dropOne(client, player, slot); }
            finally { issuing = false; }
            if (!dropped) {
                cancel("Doctrine could not stage the next inventory stack.");
                return;
            }
        }
    }
    public void spawned(ClientboundAddEntityPacket packet) {
        if (!collecting || received >= sent || packet.getType() != EntityTypes.ITEM || entityIds.contains(packet.getId())) return;
        double dx = packet.getX() - expectedSpawn.x;
        double dz = packet.getZ() - expectedSpawn.z;
        if (dx * dx + dz * dz > 0.01 || Math.abs(packet.getY() - expectedSpawn.y) > 0.15) return;
        Vec3 movement = packet.getMovement();
        float magnitude = (float) Math.sqrt(movement.x * movement.x + movement.z * movement.z) * 50f;
        if (magnitude > 1 + DropSeedSolver.MAX_ERROR || movement.y < -0.32 || movement.y > -0.08) return;
        entityIds.add(packet.getId());
        samples[received++] = magnitude;
        waitTicks = 0;
        status = received + " of 12 drops confirmed by the server.";
    }
}
