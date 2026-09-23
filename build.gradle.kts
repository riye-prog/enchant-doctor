plugins {
    id("net.fabricmc.fabric-loom") version "1.18.2"
    `maven-publish`
}
val minecraftVersion = stonecutter.current.version
version = "${property("mod_version")}+$minecraftVersion"
group = "dev.doctrine"
base.archivesName = "doctrine"
repositories {
    mavenCentral()
    maven("https://maven.latticg.com")
}
dependencies {
    minecraft("com.mojang:minecraft:$minecraftVersion")
    implementation("net.fabricmc:fabric-loader:0.19.5")
    include(implementation("com.seedfinding:latticg:1.07:rt")!!)
    for (module in listOf("fabric-api-base", "fabric-key-mapping-api-v1")) {
        include(implementation(fabricApi.module(module, "0.161.0+$minecraftVersion"))!!)
    }
    testImplementation(platform("org.junit:junit-bom:5.13.4"))
    testImplementation("org.junit.jupiter:junit-jupiter")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}
java {
    toolchain.languageVersion = JavaLanguageVersion.of(25)
    withSourcesJar()
}
tasks.test { useJUnitPlatform() }
val verifyNative by tasks.registering {
    doLast {
        val osName = System.getProperty("os.name").lowercase()
        val os = when {
            osName.contains("mac") -> "macos"
            osName.contains("win") -> "windows"
            else -> "linux"
        }
        val architecture = if (System.getProperty("os.arch").lowercase() in setOf("aarch64", "arm64")) "arm64" else "x86_64"
        val library = when (os) {
            "macos" -> "libdoctrine.dylib"
            "windows" -> "doctrine.dll"
            else -> "libdoctrine.so"
        }
        val packageDirectory = rootProject.file("native/build/package/$os-$architecture")
        val requiredFiles = listOf(library, "ui.vert.spv", "ui.frag.spv")
        check(requiredFiles.all { packageDirectory.resolve(it).isFile }) {
            "Native renderer for $os-$architecture is missing. Run the platform native build script before packaging the mod."
        }
    }
}
tasks.processResources {
    dependsOn(verifyNative)
    inputs.property("version", project.version)
    inputs.property("minecraft", minecraftVersion)
    filesMatching("fabric.mod.json") {
        expand("version" to project.version, "minecraft" to minecraftVersion)
    }
    from(rootProject.file("native/build/package")) { into("natives") }
}
tasks.jar { from(rootProject.file("LICENSE"), rootProject.file("NOTICE")) }
loom {
    runs.named("client") {
        runDir("../../run/$minecraftVersion")
        vmArg("-Dmixin.debug.countInjections=true")
    }
}
