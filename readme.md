# doctrine

a fabric enchantment workbench for minecraft 26.2 and 26.3, built from one source tree with stonecutter. press f8 while playing or while an enchanting table is open.

the interface is actual rmlui, loaded through a native library and drawn into minecraft's vulkan command buffer before presentation. the mod forces vulkan at startup, including when the saved graphics preference is opengl. it does not change the saved preference. a client without working vulkan cannot run this mod.

## what it does

- recovers the 32-bit enchantment seed from the server's 12 visible bits, costs, and enchantment clues.
- shows every enchantment in all three offers after the seed is recovered.
- recovers the 48-bit player rng from two confirmed, fresh enchantment seeds. the historical seed is deliberately excluded.
- searches the current seed first, then future seeds, for the fewest individual item drops needed for your target. every bookshelf count and all three offers are considered.
- drains eligible disposable stacks across the hotbar and main inventory, automatically staging the next stack when one empties, even when the starting slot is empty.
- rotates matching hotbar and main-inventory stacks through one fixed selected slot, drops exactly one item per action, and keeps the camera aimed straight down.
- supports multiple minimum-level requirements, a drop limit, a level budget, automatic bookshelf detection, and copying a route.

## using it

open an enchanting table and insert an unenchanted item. press f8 to see the workbench. swapping items narrows the seed candidates. the vanilla table stays underneath; use the back button or f8 to return to it.

press f7 while in the game or use the recovery action in the workbench to recover player rng from 12 drops. doctrine returns to the game, automatically stages eligible disposable items from anywhere in the inventory, looks straight down, sends the observations as one ordered burst, validates the final two, and reports progress above the hotbar. keep at least 12 stackable, unenchanted disposable items anywhere in your inventory. the two-enchantment recovery path remains available on servers with vanilla player rng.

enter an item identifier and requirements such as `efficiency:4,unbreaking:3`, then find a route. namespaced identifiers work too. requirements are minimum levels, so an extra compatible enchantment is allowed. doctrine searches only for a single enchanting-table result on the selected item, never for a sequence of books or anvil combinations. the drop limit can be set as high as 100,000. some requests cannot roll at the table at all: for example, efficiency v cannot roll directly on a book in vanilla. doctrine reports those impossible targets immediately rather than searching indefinitely.

for a route with drops, keep stackable, unenchanted disposable items anywhere in your inventory and press g or use the run drops action; the selected slot may be empty. doctrine closes the table, looks straight down, and sends bounded bursts of individual items. it drains the staged stack, then pulls the next eligible stack—regardless of item type—from either the hotbar or main inventory while keeping the selected hotbar index fixed. the in-flight item limit prevents an integrated server from being flooded; after the pickup delay doctrine reuses recovered items until the route is complete. f10 cancels either recovery or planned drops. once all drops are done, perform one dummy enchantment using the first offer, then set the recommended bookshelves and enchant the target item. enchanting and bookshelf changes remain manual.

normal walking and sprinting do not themselves clear a recovered seed. camera or position changes still cancel an active drop sequence because its item-spawn matching depends on a fixed origin. while recovering or tracking, doctrine blocks attacks, block breaking, item use, anvil button presses, untracked inventory drops and `/kill` commands before sending them. sneak while acting to override protection (this clears the seed). enchanting-table interactions and tracked drops remain allowed. damage, swimming, fire, equipment changes and experience pickups may advance player rng and invalidate tracking; ordinary advancement and entity-event packets do not. tracked drops advance rng by four calls only when doctrine issues them, and server spawn packets confirm progress. a mismatch or confirmation timeout clears the recovery state.

vanilla's `Entity` creates its own random source when constructed; server `PlayerList.respawn` constructs a new `ServerPlayer` after death. reconnection also creates a new player. dimension transitions send a respawn packet to recreate the *client* player, but the vanilla server may reuse its existing player; doctrine conservatively clears tracking there because mods/plugins can change that behavior. low health and portal entry trigger warnings. a client-only mod cannot prevent server-driven damage, portal travel, respawns, or RNG calls from plugins: after any such event, recover a new seed.

## paper servers

stock paper 26.2 does not preserve a private random stream for each player. its entity patch replaces every entity's random source with one synchronized shared random source, so mobs and other entities insert unknown calls between player actions. there is no stable player seed that a client-only mod can recover from item drops, enchantment seeds, projectiles, or another observable player action.

doctrine detects paper, purpur, folia, and pufferfish server brands and switches to current-offer mode. table costs and clues can still recover the stored enchantment seed and reveal all three current offers, but future-seed routes and automatic rng steering are disabled. a server-side plugin could read or set paper's enchantment seed api, but that requires cooperation from the server and is outside a client-only mod.

the behavior was checked against [paper's shared entity random patch](https://github.com/papermc/paper/blob/e5fe71723e2ffde7cc9fafc085ac3bb73e63175e/paper-server/patches/sources/net/minecraft/world/entity/Entity.java.patch) and the [paper 26.2 human entity api](https://jd.papermc.io/paper/26.2/org/bukkit/entity/HumanEntity.html).

## build

use a java 25 jdk and cmake. native compilation needs vulkan development headers and the glslang shader compiler. on macos, the homebrew packages are `cmake`, `vulkan-headers`, `vulkan-loader`, and `glslang`.

on macos or linux, run `sh scripts/build-native.sh`, then `./gradlew :26.2:build :26.3:build`. the native build fetches pinned revisions of rmlui, volk, and freetype and links them statically into the bridge.

on windows, install the visual studio 2022 c++ build tools and the lunarg vulkan sdk, set `JAVA_HOME` to a java 25 jdk, and open powershell in the project directory. run `powershell -ExecutionPolicy Bypass -File scripts/build-native.ps1`, then `.\gradlew.bat :26.2:build :26.3:build`. the script creates `native/build/package/windows-x86_64/doctrine.dll` (or `windows-arm64` when requested), verifies its shaders, and the gradle build packages it into each mod jar. the windows library uses the static visual c++ runtime, so players do not need to install a matching runtime separately.

the mod jars are in `versions/26.2/build/libs/` and `versions/26.3/build/libs/`. install the matching jar in a fabric client's mods folder. fabric api is not required. each locally built jar contains the native packages present in `native/build/package/`; copy additional platform directories there before running gradle to produce a multi-platform jar. windows support is implemented but still requires gameplay validation on windows gpu drivers.

stonecutter handles the render package move and item-drop api change between versions through string replacements; the shared java source needs no version comments.

## verification

`./gradlew :26.2:test :26.3:test` checks java random equivalence, rejection sampling, rng jump-ahead, 12-drop lattice recovery, candidate masks, fresh-seed tracking, and 75,000 enchantment costs against each version's actual vanilla implementation.

the optional `doctrine_smoke` cmake target renders the real workbench to an offscreen vulkan image. enable its build option through cmake and run ctest from the native build directory. the desktop and compact layouts were rendered on an apple m3, with no rmlui warnings or vulkan validation errors. retained rmlui update and draw-list construction measured about 28 microseconds median in the isolated desktop validation test. that excludes gpu execution, jni, minecraft rendering, and first-open work; it is not an in-game fps guarantee.

both development clients started with vulkan and prewarmed the native workbench. full end-to-end enchantment manipulation and long multiplayer sessions still need gameplay testing.

## source and license

the enchantment algorithm and constant-time lattice recovery were studied and adapted from [earthcomputer's clientcommands](https://github.com/earthcomputer/clientcommands), revision `554a519c5e8d491abb3a7accca9b485cfeb69e4d`. doctrine is distributed under lgpl-3.0-or-later. attribution and third-party license files are included in the source tree and packaged jar.

the candidate scan uses primitive arrays and a lightweight lcg cost filter. searches share one low-priority worker, cancellation checks, and immutable observations. rmlui retains compiled geometry, updates on the game frame loop, and defers resource destruction until gpu events confirm completion. no frame-by-frame framebuffer readback is used by the mod.
