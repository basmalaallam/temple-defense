# Code structure

The whole game is now in `main.cpp`. Its numbered sections are:

1. Character types and data: player/enemy types, actions, and entity structs.
2. Wave data and game settings: lane layout, card costs, and combat timing.
3. Game states and drawing declarations: menu/play states and function signatures.
4. Cutscene timeline: scripted positions and dialogue over 18 seconds.
5. Text patterns and coordinate/layout helpers: letters, mouse conversion, health bars.
6. Gameplay: shared state, purchasing, evasion, movement, combat, pickups, and waves.
7. Input: keyboard, mouse clicks, resize, and menu actions.
8. Scenery: desert, pyramids, palms, and temple furnishings.
9. Drawing: shaders, primitives, character models, cards, menus, and complete frames.
10. Startup: window/context creation, vertex data, main loop, and cleanup.

Function declarations tell the compiler about functions used before their definitions
later in the same file. Custom headers are no longer needed. The external GLFW,
GLAD, and GLM headers are still needed because they belong to the supplied libraries.

Build the OpenGLStarter CMake target as before. CMake now compiles only main.cpp
for the game's own source. A copy of the previous divided source is stored in
`tmp/before_single_file` as a backup; it is not part of the build.

## How the provided template is used

The supplied template provides the graphics foundation: GLFW initialization and
window/context creation, GLAD loading, shader compilation/linking, VAO/VBO setup,
the render loop, and cleanup. The supplied examples also show GLM transformations,
uniforms, elapsed-time movement, callbacks, and triangle-fan circles.

The game extends these techniques with its own entity data, lane placement,
economy, collisions, combat, waves, cutscene, menus, and Egyptian shapes. It does
not depend on the original sample triangle appearing on screen. The rectangle,
triangle, and circle geometry now serve the actual game graphics.

This consolidation changes where the code is stored, rather than the game rules.

## Sound

Audio lives in `audio.h` / `audio.cpp` (miniaudio, `external/miniaudio/miniaudio.h`).
`main.cpp` only calls `audio::...` functions: effects on clicks, lane selection, card
selection and placement, and `updateAudioForState` in the main loop, which pauses the
music and plays the victory or game over sound once when that screen opens. Sound files
are in `assets/sounds/`; CMake copies them next to the executable after every build.
Keys: `+` / `-` change the volume by 10%, `N` mutes or unmutes. The main menu's
Sound Settings screen changes the same setting.
