#include "logo/animation.h"
#include "logo/logo.h"
#include "common/io.h"
#include "common/strutil.h"
#include "common/time.h"
#include "detection/os/os.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <poll.h>
    #include <signal.h>
    #include <sys/ioctl.h>
    #include <unistd.h>
#endif

#define FF_ANIMATION_MAX_FRAMES 256
#define FF_ANIMATION_MAX_LINES 512
#define FF_ANIMATION_MAX_WIDTH 512
#define FF_ANIMATION_MAX_BYTES (2 * 1024 * 1024)
#define FF_ANIMATION_DEFAULT_FPS 12
#define FF_ANIMATION_DEFAULT_LOOP 2
#define FF_ANIMATION_DEFAULT_TIMEOUT 5000

// ------------------------------------------------------------------ parser

static bool animationIsSeparator(const char* line) {
    if (line[0] != '-' || line[1] != '-' || line[2] != '-') {
        return false;
    }
    for (const char* p = line + 3; *p != '\0'; ++p) {
        if (*p != ' ' && *p != '\t' && *p != '\r') {
            return false;
        }
    }
    return true;
}

// Visible width of a line, mirroring the upstream logo line cache logic:
// ANSI CSI sequences and `$1`-`$9` color placeholders are zero-width, `$$` is a literal `$`.
static uint32_t animationLineWidth(const char* data, const char* end) {
    uint32_t width = 0;
    while (data < end) {
        if (*data == '\t') {
            width += 4;
            ++data;
            continue;
        }

        if (*data == '\e' && data + 1 < end && data[1] == '[') {
            const char* start = data;
            data += 2;
            while (data < end && (ffCharIsDigit(*data) || *data == ';')) {
                ++data;
            }
            if (data < end && isascii(*data)) {
                ++data; // zero-width CSI sequence
                continue;
            }
            width += (uint32_t) (data - start - 1);
            continue;
        }

        if (*data == '$') {
            ++data;
            if (data >= end || *data == '$' || *data == '\0') {
                width += 1;
                ++data;
                continue;
            }
            int index = *data - '1';
            if (index >= 0 && index < FASTFETCH_LOGO_MAX_COLORS) {
                ++data; // color placeholder, zero width
                continue;
            }
            width += 1; // literal `$`
            ++data;
            continue;
        }

        uint8_t charWidth = 0;
        uint8_t bytes = ffUtf8CharLenWidth(data, (uint32_t) (end - data), &charWidth);
        if (bytes == 0) {
            ++data; // invalid UTF-8, count as zero width
            continue;
        }
        width += charWidth;
        data += bytes;
    }
    return width;
}

static bool animationParseUInt(const char* value, uint32_t* result) {
    if (*value == '\0') {
        return false;
    }
    char* end = nullptr;
    unsigned long parsed = strtoul(value, &end, 10);
    if (end == value || parsed > UINT32_MAX) {
        return false;
    }
    while (*end == ' ' || *end == '\t' || *end == '\r') {
        ++end;
    }
    if (*end != '\0') {
        return false;
    }
    *result = (uint32_t) parsed;
    return true;
}

bool ffAnimationParseData(const char* data, FFAnimation* animation, FFstrbuf* error) {
    FF_LIST_AUTO_DESTROY lines = ffListCreate();

    // Split into lines, stripping CR and trailing newline
    {
        const char* p = data;
        while (*p != '\0') {
            const char* eol = strchr(p, '\n');
            const char* end = eol ? eol : p + strlen(p);
            if (end > p && end[-1] == '\r') {
                --end;
            }
            FFstrbuf* line = FF_LIST_ADD(FFstrbuf, lines);
            ffStrbufInitNS(line, (uint32_t) (end - p), p);
            if (!eol) {
                break;
            }
            p = eol + 1;
        }
    }

    // Detect an optional header (directives/comments before the first separator)
    bool hasHeader = false;
    for (uint32_t i = 0; i < lines.length; ++i) {
        const char* line = FF_LIST_GET(FFstrbuf, lines, i)->chars;
        if (*line == '\0') {
            continue;
        }
        hasHeader = *line == '!' || *line == '#';
        break;
    }

    uint32_t firstSeparator = lines.length;
    for (uint32_t i = 0; i < lines.length; ++i) {
        if (animationIsSeparator(FF_LIST_GET(FFstrbuf, lines, i)->chars)) {
            firstSeparator = i;
            break;
        }
    }

    if (hasHeader && firstSeparator == lines.length) {
        ffStrbufSetS(error, "animation header has no frame data (missing `---` separator)");
        return false;
    }

    // Parse header directives. Values are initialized to "unset" sentinels and resolved
    // against config/CLI and defaults in ffAnimationLoad.
    animation->fps = 0;
    animation->loop = -1;
    animation->timeout = 0;
    animation->holdFirst = false;
    bool holdSet = false;

    if (hasHeader) {
        for (uint32_t i = 0; i < firstSeparator; ++i) {
            const char* line = FF_LIST_GET(FFstrbuf, lines, i)->chars;
            if (*line == '\0' || *line == '#') {
                continue;
            }
            if (*line != '!') {
                ffStrbufSetF(error, "line %u: unexpected content in animation header (expected `!directive`, `#` comment or `---`)", i + 1);
                return false;
            }

            const char* p = line + 1;
            while (*p == ' ' || *p == '\t') {
                ++p;
            }
            const char* key = p;
            while (*p != '\0' && *p != ' ' && *p != '\t') {
                ++p;
            }
            uint32_t keyLength = (uint32_t) (p - key);
            while (*p == ' ' || *p == '\t') {
                ++p;
            }
            const char* value = p;

            uint32_t number = 0;
            if (keyLength == 3 && strncmp(key, "fps", 3) == 0) {
                if (!animationParseUInt(value, &number) || number < 1 || number > 60) {
                    ffStrbufSetF(error, "line %u: `!fps` must be an integer between 1 and 60", i + 1);
                    return false;
                }
                animation->fps = number;
            } else if (keyLength == 4 && strncmp(key, "loop", 4) == 0) {
                if (!animationParseUInt(value, &number) || number > 1000) {
                    ffStrbufSetF(error, "line %u: `!loop` must be an integer between 0 and 1000", i + 1);
                    return false;
                }
                animation->loop = (int32_t) number;
            } else if (keyLength == 7 && strncmp(key, "timeout", 7) == 0) {
                if (!animationParseUInt(value, &number) || number == 0 || number > 600000) {
                    ffStrbufSetF(error, "line %u: `!timeout` must be an integer between 1 and 600000 (milliseconds)", i + 1);
                    return false;
                }
                animation->timeout = number;
            } else if (keyLength == 4 && strncmp(key, "hold", 4) == 0) {
                if (strcmp(value, "first") == 0) {
                    animation->holdFirst = true;
                } else if (strcmp(value, "last") == 0) {
                    animation->holdFirst = false;
                } else {
                    ffStrbufSetF(error, "line %u: `!hold` must be `first` or `last`", i + 1);
                    return false;
                }
                holdSet = true;
            } else {
                ffStrbufSetF(error, "line %u: unknown animation directive `%.*s`", i + 1, (int) keyLength, key);
                return false;
            }
        }
    }
    (void) holdSet;

    // Collect frames: segments separated by `---`
    uint32_t maxLines = 0;
    uint32_t maxWidth = 0;
    uint64_t totalBytes = 0;

    uint32_t i = hasHeader ? firstSeparator + 1 : 0;
    uint32_t frameNumber = 0;
    while (i <= lines.length) {
        uint32_t j = i;
        while (j < lines.length && !animationIsSeparator(FF_LIST_GET(FFstrbuf, lines, j)->chars)) {
            ++j;
        }

        // Trim trailing empty lines
        uint32_t segmentEnd = j;
        while (segmentEnd > i && FF_LIST_GET(FFstrbuf, lines, segmentEnd - 1)->chars[0] == '\0') {
            --segmentEnd;
        }

        if (segmentEnd == i) {
            if (j >= lines.length && frameNumber > 0 && i == lines.length) {
                break; // trailing separator at EOF is fine
            }
            ffStrbufSetF(error, "frame %u is empty", frameNumber + 1);
            return false;
        }

        ++frameNumber;
        if (frameNumber > FF_ANIMATION_MAX_FRAMES) {
            ffStrbufSetF(error, "too many frames (max %u)", FF_ANIMATION_MAX_FRAMES);
            return false;
        }

        uint32_t segmentLines = segmentEnd - i;
        if (segmentLines > FF_ANIMATION_MAX_LINES) {
            ffStrbufSetF(error, "frame %u has too many lines (max %u)", frameNumber, FF_ANIMATION_MAX_LINES);
            return false;
        }
        if (segmentLines > maxLines) {
            maxLines = segmentLines;
        }

        FFstrbuf* frame = FF_LIST_ADD(FFstrbuf, animation->frames);
        ffStrbufInit(frame);
        for (uint32_t k = i; k < segmentEnd; ++k) {
            const FFstrbuf* line = FF_LIST_GET(FFstrbuf, lines, k);
            uint32_t width = animationLineWidth(line->chars, line->chars + line->length);
            if (width > FF_ANIMATION_MAX_WIDTH) {
                ffStrbufSetF(error, "frame %u line %u is too wide (%u columns, max %u)", frameNumber, k - i + 1, width, FF_ANIMATION_MAX_WIDTH);
                return false;
            }
            if (width > maxWidth) {
                maxWidth = width;
            }
            // Frames do not end with a newline: this matches how upstream logo files are
            // parsed, so an animation frame occupies exactly as many rows as the same art
            // printed as a static logo (see ffAnimationBegin for the region height).
            if (k > i) {
                ffStrbufAppendC(frame, '\n');
            }
            ffStrbufAppend(frame, line);
            totalBytes += line->length + 1;
        }

        if (totalBytes > FF_ANIMATION_MAX_BYTES) {
            ffStrbufSetF(error, "animation is too large (max %u bytes)", FF_ANIMATION_MAX_BYTES);
            return false;
        }

        if (j >= lines.length) {
            break;
        }
        i = j + 1;
    }

    if (frameNumber == 0) {
        ffStrbufSetS(error, "animation has no frames");
        return false;
    }

    // Pad all frames to the same height so every frame produces the same logo region.
    // Frames end without a trailing newline, so a frame with N lines has N-1 newlines.
    FF_LIST_FOR_EACH (FFstrbuf, frame, animation->frames) {
        uint32_t frameNewlines = 0;
        for (uint32_t k = 0; k < frame->length; ++k) {
            if (frame->chars[k] == '\n') {
                ++frameNewlines;
            }
        }
        while (frameNewlines < maxLines - 1) {
            ffStrbufAppendC(frame, '\n');
            ++frameNewlines;
        }
    }

    animation->height = maxLines;
    animation->width = maxWidth;
    return true;
}

// ------------------------------------------------------------- builtin lookup

static const FFBuiltinAnimation* animationFindBuiltin(const FFstrbuf* name) {
    if (name->length == 0) {
        return nullptr;
    }
    for (const FFBuiltinAnimation* animation = ffBuiltinAnimations; animation->name != nullptr; ++animation) {
        if (ffStrbufIgnCaseEqualS(name, animation->name)) {
            return animation;
        }
    }
    return nullptr;
}

static const FFBuiltinAnimation* animationGetBuiltinForOS(void) {
    const FFOSResult* os = ffDetectOS();

    const FFBuiltinAnimation* result = animationFindBuiltin(&os->id);
    if (result != nullptr) {
        return result;
    }

    result = animationFindBuiltin(&os->name);
    if (result != nullptr) {
        return result;
    }

    if (ffStrbufContainC(&os->idLike, ' ')) {
        FF_STRBUF_AUTO_DESTROY buf = ffStrbufCreate();
        for (
            uint32_t start = 0, end = ffStrbufFirstIndexC(&os->idLike, ' ');
            true;
            start = end + 1, end = ffStrbufNextIndexC(&os->idLike, start, ' ')) {
            ffStrbufSetNS(&buf, end - start, os->idLike.chars + start);
            result = animationFindBuiltin(&buf);
            if (result != nullptr) {
                return result;
            }

            if (end >= os->idLike.length) {
                break;
            }
        }
    } else {
        result = animationFindBuiltin(&os->idLike);
        if (result != nullptr) {
            return result;
        }
    }

    return animationFindBuiltin(&instance.state.platform.sysinfo.name);
}

// ------------------------------------------------------------------ loading

bool ffAnimationLoad(FFAnimation* animation, FFstrbuf* error) {
    FFOptionsLogo* options = &instance.config.logo;

    const char* data = nullptr;
    FF_STRBUF_AUTO_DESTROY fileContent = ffStrbufCreate();

    if (options->source.length == 0) {
        const FFBuiltinAnimation* builtin = animationGetBuiltinForOS();
        if (builtin == nullptr) {
            ffStrbufSetS(error, "no builtin animation found for the detected OS; use `--logo <name|path>`");
            return false;
        }
        data = builtin->data;
    } else {
        const FFBuiltinAnimation* builtin = animationFindBuiltin(&options->source);
        if (builtin != nullptr) {
            data = builtin->data;
        } else {
            FF_STRBUF_AUTO_DESTROY fullPath = ffStrbufCreateA(128);
            const char* path = nullptr;
            if (ffPathExists(options->source.chars, FF_PATHTYPE_FILE)) {
                path = options->source.chars;
            } else if (ffPathExpandEnv(options->source.chars, &fullPath) && ffPathExists(fullPath.chars, FF_PATHTYPE_FILE)) {
                path = fullPath.chars;
            }

            if (path == nullptr) {
                ffStrbufSetF(error, "animation not found: %s", options->source.chars);
                return false;
            }

            if (!ffAppendFileBuffer(path, &fileContent)) {
                ffStrbufSetF(error, "failed to read animation file: %s", path);
                return false;
            }

            data = fileContent.chars;
        }
    }

    animation->frames = ffListCreate();
    if (!ffAnimationParseData(data, animation, error)) {
        ffAnimationDestroy(animation);
        return false;
    }

    // Resolve playback options: CLI/config > file directive > default
    animation->fps = options->animationFps > 0
        ? options->animationFps
        : animation->fps > 0
        ? animation->fps
        : FF_ANIMATION_DEFAULT_FPS;
    animation->loop = options->animationLoop >= 0
        ? options->animationLoop
        : animation->loop >= 0
        ? animation->loop
        : FF_ANIMATION_DEFAULT_LOOP;
    animation->timeout = options->animationTimeout > 0
        ? options->animationTimeout
        : animation->timeout > 0
        ? animation->timeout
        : FF_ANIMATION_DEFAULT_TIMEOUT;
    if (options->animationHold >= 0) {
        animation->holdFirst = options->animationHold == 0;
    }

    return true;
}

void ffAnimationDestroy(FFAnimation* animation) {
    FF_LIST_FOR_EACH (FFstrbuf, frame, animation->frames) {
        ffStrbufDestroy(frame);
    }
    ffListDestroy(&animation->frames);
    animation->frames.length = 0;
}

// ------------------------------------------------------------------- state

static FFAnimation gAnimation;
static bool gAnimationLoaded = false;
static bool gAnimationEligible = false;
static uint32_t gAnimationInfoLines = 0;
static uint32_t gAnimationRegionHeight = 0;
static uint32_t gAnimationEndRow = 0;

static bool animationCanAnimate(void) {
#ifdef _WIN32
    return false;
#else
    if (instance.config.display.pipe) {
        return false;
    }
    if (!isatty(STDOUT_FILENO)) {
        return false;
    }
    if (instance.config.logo.position != FF_LOGO_POSITION_LEFT) {
        return false;
    }
    if (gAnimation.frames.length < 2) {
        return false;
    }
    return true;
#endif
}

bool ffAnimationHasBuiltinForOS(void) {
    return animationGetBuiltinForOS() != nullptr;
}

void ffAnimationPrint(void) {
    FFOptionsLogo* options = &instance.config.logo;

    FF_STRBUF_AUTO_DESTROY error = ffStrbufCreate();
    if (!ffAnimationLoad(&gAnimation, &error)) {
        if (instance.config.display.showErrors) {
            fprintf(stderr, "Logo: %s\n", error.chars);
        }
        ffLogoPrintDetected(FF_LOGO_SIZE_UNKNOWN);
        return;
    }

    gAnimationLoaded = true;

    // All frames must render with the same width so redrawing never touches the info columns
    if (options->width < gAnimation.width) {
        options->width = gAnimation.width;
    }

    // Without animation support (pipes, unsupported position, single frame) show the hold frame
    bool willAnimate = animationCanAnimate();
    uint32_t index = willAnimate ? 0 : (gAnimation.holdFirst ? 0 : gAnimation.frames.length - 1);
    ffLogoPrintAnimationFrame(FF_LIST_GET(FFstrbuf, gAnimation.frames, index)->chars);
}

bool ffAnimationBegin(void) {
    if (!gAnimationLoaded || !animationCanAnimate()) {
        return false;
    }

    gAnimationInfoLines = instance.state.keysHeight;
    // Frames end without a trailing newline, so upstream's line parser counts one line
    // less than the frame has; the last line is still printed from the line cache.
    gAnimationRegionHeight = instance.config.logo.paddingTop + gAnimation.height;
    gAnimationEligible = true;
    return true;
}

void ffAnimationListBuiltins(void) {
    for (const FFBuiltinAnimation* animation = ffBuiltinAnimations; animation->name != nullptr; ++animation) {
        puts(animation->name);
    }
}

// ----------------------------------------------------------------- renderer

#ifndef _WIN32

static volatile sig_atomic_t gAnimationStop = 0;

static void animationSignalHandler(int signal) {
    gAnimationStop = 1;
    char buffer[64];
    int length = snprintf(buffer, sizeof(buffer), "\e[%u;1H\e[?25h\e[0m", gAnimationEndRow);
    if (length > 0) {
        (void) !write(STDOUT_FILENO, buffer, (size_t) length);
    }
    exit(128 + signal);
}

static int gAnimationTtyFd = -1;

static bool animationKeyPressed(void) {
    if (gAnimationTtyFd < 0) {
        gAnimationTtyFd = open("/dev/tty", O_RDONLY | O_NOCTTY | O_CLOEXEC);
        if (gAnimationTtyFd < 0) {
            return false;
        }
    }

    struct pollfd pfd = { .fd = gAnimationTtyFd, .events = POLLIN };
    return poll(&pfd, 1, 0) > 0;
}

void ffAnimationRun(void) {
    if (!gAnimationEligible) {
        return;
    }
    gAnimationEligible = false;

    // Cursor is at the beginning of the row right below the printed output
    uint16_t cursorRow = 0;
    uint16_t cursorColumn = 0;
    if (ffGetTerminalResponse("\e[6n", 2, "%*[^0-9]%hu;%huR", &cursorRow, &cursorColumn) != nullptr) {
        return; // terminal doesn't answer DSR: leave the statically printed frame
    }

    uint32_t totalLines = gAnimationInfoLines > gAnimationRegionHeight ? gAnimationInfoLines : gAnimationRegionHeight;
    if (cursorRow <= totalLines) {
        return; // the logo region has scrolled out of the screen
    }
    uint32_t startRow = cursorRow - totalLines;

    struct winsize size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0 && instance.state.logoWidth >= size.ws_col) {
        return; // writing to the last column may wrap and corrupt the layout
    }

    gAnimationEndRow = cursorRow;
    gAnimationStop = 0;

    struct sigaction action = {};
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    action.sa_handler = animationSignalHandler;
    struct sigaction oldInt, oldTerm, oldQuit;
    sigaction(SIGINT, &action, &oldInt);
    sigaction(SIGTERM, &action, &oldTerm);
    sigaction(SIGQUIT, &action, &oldQuit);

    fputs("\e[?25l", stdout);
    fflush(stdout);

    uint32_t frameCount = gAnimation.frames.length;
    uint32_t frameDelay = 1000 / gAnimation.fps;
    bool infinite = gAnimation.loop == 0;
    uint64_t startTime = ffTimeGetNow();

    for (uint32_t loopIndex = 0; infinite || loopIndex < (uint32_t) gAnimation.loop; ++loopIndex) {
        for (uint32_t frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
            if (gAnimationStop) {
                goto cleanup;
            }
            if (infinite && (animationKeyPressed() || ffTimeGetNow() - startTime >= gAnimation.timeout)) {
                goto cleanup;
            }

            ffLogoPrintAnimationFrame(FF_LIST_GET(FFstrbuf, gAnimation.frames, frameIndex)->chars);

            fputs("\e[?2026h", stdout); // synchronized output (DEC 2026), ignored where unsupported
            for (uint32_t row = 0; row < gAnimationRegionHeight; ++row) {
                printf("\e[%u;1H", startRow + row);
                ffLogoPrintAnimationRow(row);
            }
            fputs("\e[?2026l", stdout);
            fflush(stdout);

            ffTimeSleep(frameDelay);
        }
    }

cleanup:
    printf("\e[%u;1H\e[?25h", gAnimationEndRow);
    fflush(stdout);

    sigaction(SIGINT, &oldInt, nullptr);
    sigaction(SIGTERM, &oldTerm, nullptr);
    sigaction(SIGQUIT, &oldQuit, nullptr);
}

#else // _WIN32

void ffAnimationRun(void) {
    // Windows: animation is not supported, the hold frame was already printed statically
}

#endif
