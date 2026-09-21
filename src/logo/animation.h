#pragma once

#include "fastfetch.h"

typedef struct FFBuiltinAnimation {
    const char* name;
    const char* data;
} FFBuiltinAnimation;

// Generated at build time from `examples/*.anim` (see CMakeLists.txt).
extern const FFBuiltinAnimation ffBuiltinAnimations[];

typedef struct FFAnimation {
    FFlist frames; // FFstrbuf: raw text of each frame, every line terminated with '\n'
    uint32_t width; // max visible width of a frame line, before logo padding
    uint32_t height; // number of lines per frame (all frames are padded to the same height)

    // Resolved playback options (config/CLI override file directives, file overrides defaults)
    uint32_t fps; // 1 - 60
    int32_t loop; // 0 = infinite, otherwise number of loops
    uint32_t timeout; // milliseconds, only used when loop == 0
    bool holdFirst; // freeze on the first frame instead of the last
} FFAnimation;

// animation.c

// Parses the `.anim` format (see examples/README.md) from a NUL-terminated string.
// On failure returns false and fills `error` with a human readable, line-numbered message.
bool ffAnimationParseData(const char* data, FFAnimation* animation, FFstrbuf* error);

// Resolves `instance.config.logo` (builtin animation name, detected OS or a file path),
// parses it and resolves playback options. Caller must call ffAnimationDestroy on success.
bool ffAnimationLoad(FFAnimation* animation, FFstrbuf* error);
void ffAnimationDestroy(FFAnimation* animation);

// Called by ffLogoPrint() when logo type is `animation`.
void ffAnimationPrint(void);

// Called by run() after all info has been printed. Returns true when the animation
// should play; in that case ffLogoPrintRemaining() must still be called before ffFinish().
bool ffAnimationBegin(void);

// Called by run() after ffFinish(). Redraws the logo region with the animation frames.
void ffAnimationRun(void);

// Prints builtin animation names (for --list-animations).
void ffAnimationListBuiltins(void);
