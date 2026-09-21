#!/usr/bin/env node
/**
 * Tests the pure logic of the browser converter (web/index.html) under Node and validates
 * a generated .anim file with moofetch's real parser.
 *
 * Usage: node tests/web_converter.test.cjs
 */
"use strict";

const fs = require("fs");
const os = require("os");
const path = require("path");
const { execFileSync } = require("child_process");

const root = path.join(__dirname, "..");
let failures = 0;

function check(condition, message) {
    if (condition) {
        console.log("  ok   " + message);
    } else {
        console.log("  FAIL " + message);
        failures++;
    }
}

// Extract the inline script and load it as a CommonJS module
const html = fs.readFileSync(path.join(root, "web", "index.html"), "utf8");
const script = /<script>([\s\S]*?)<\/script>/.exec(html);
if (!script) {
    console.error("could not find the inline script in web/index.html");
    process.exit(2);
}
const modulePath = path.join(os.tmpdir(), "moofetch-converter-under-test.cjs");
fs.writeFileSync(modulePath, script[1]);
const converter = require(modulePath);

console.log("test: pixelsToLines");
{
    // 2x1 image: black pixel, white pixel
    const pixels = new Uint8ClampedArray([
        0, 0, 0, 255,
        255, 255, 255, 255,
    ]);
    const lines = converter.pixelsToLines(pixels, 2, 1, { charset: "standard", color: "none" });
    check(lines.length === 1 && lines[0].length === 2, "one line of two cells");
    check(lines[0][0] === " ", "black maps to the darkest charset character (space)");
    check(lines[0][1] === "@", "white maps to the densest charset character (@)");

    const colored = converter.pixelsToLines(pixels, 2, 1, { charset: "standard", color: "truecolor" });
    check(colored[0].includes("\x1b[38;2;255;255;255m@"), "truecolor escape emitted for white");
    check(!colored[0].includes("\x1b[38;2;0;0;0m"), "no color escape for background/space cells");

    const transparent = new Uint8ClampedArray([255, 255, 255, 0]);
    check(converter.pixelsToLines(transparent, 1, 1, { charset: "standard", color: "truecolor" })[0] === " ",
        "transparent pixels become spaces");

    const inverted = converter.pixelsToLines(pixels, 2, 1, { charset: "standard", color: "none", invert: true });
    check(inverted[0][0] === "@" && inverted[0][1] === " ", "invert swaps the ramp");
}

console.log("test: targetHeight");
{
    check(converter.targetHeight(100, 50, 20) === 5, "100x50 at width 20 -> 5 rows");
    check(converter.targetHeight(1, 100, 10) === 500, "tall images are not clamped by this helper");
}

console.log("test: buildAnim");
{
    const frames = [["ab", "cd"], ["ef", "gh"]];
    const text = converter.buildAnim(frames, { fps: 12, loop: 2, timeout: 5000, hold: "", title: "Test" });
    check(text.startsWith("# Test\n"), "title comment");
    check(text.includes("!fps 12\n"), "fps directive");
    check(text.includes("!loop 2\n"), "loop directive");
    check(!text.includes("!timeout"), "no timeout directive for finite loops");
    check(text.includes("!hold") === false, "no hold directive when unset");
    check(text.split("\n").filter((line) => line === "---").length === 2, "one separator per frame");

    const infinite = converter.buildAnim(frames, { fps: 8, loop: 0, timeout: 1234, hold: "first" });
    check(infinite.includes("!timeout 1234\n"), "timeout directive for infinite loops");
    check(infinite.includes("!hold first\n"), "hold directive");
}

console.log("test: generated file passes moofetch's parser");
{
    const width = 24;
    const frames = [];
    for (let frame = 0; frame < 4; frame++) {
        const pixels = new Uint8ClampedArray(width * 3 * 4);
        for (let i = 0; i < width * 3; i++) {
            const lit = ((i + frame * 7) % width) < width / 2;
            pixels[i * 4] = lit ? 255 : 0;
            pixels[i * 4 + 1] = lit ? 200 : 0;
            pixels[i * 4 + 2] = lit ? 100 : 0;
            pixels[i * 4 + 3] = 255;
        }
        frames.push(converter.pixelsToLines(pixels, width, 3, { charset: "standard", color: "truecolor" }));
    }
    const text = converter.buildAnim(frames, { fps: 12, loop: 1, timeout: 5000, hold: "last", title: "converter test" });
    const animPath = path.join(os.tmpdir(), "moofetch-converter-test.anim");
    fs.writeFileSync(animPath, text);

    const moofetch = path.join(root, "build", "moofetch");
    if (!fs.existsSync(moofetch)) {
        console.log("  skip moofetch binary not built");
    } else {
        try {
            const output = execFileSync("python3", [path.join(root, "tools", "gif2anim"), "--check", animPath, "--moofetch", moofetch], { encoding: "utf8" });
            check(output.includes("valid"), "parser accepts the generated file: " + output.trim());
        } catch (error) {
            check(false, "parser rejected the generated file: " + (error.stdout || error.message));
        }
    }
}

console.log();
if (failures > 0) {
    console.log(failures + " failure(s)");
    process.exit(1);
}
console.log("all web converter tests passed");
