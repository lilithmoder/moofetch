# moofetch browser converter

A single-file (HTML + CSS + JS) converter that turns an animated GIF/WebP — or a set of
image frames — into a moofetch `.anim` file. Everything runs locally in the browser;
nothing is uploaded.

* **Hosted:** <https://lilithmoder.github.io/moofetch/> (deployed from this directory by
  `.github/workflows/pages.yml`)
* **Run locally:** open `index.html` directly in a browser, or serve the directory:

  ```sh
  python3 -m http.server -d web 8000
  # then open http://localhost:8000
  ```

## Notes

* Animated GIF decoding uses the `ImageDecoder` API (Chrome, Edge, Safari and recent
  Firefox). If it is unavailable, drop individual PNG/JPG frames instead — each file
  becomes one frame, sorted by file name.
* Options: width in terminal cells, fps, loop, timeout, hold frame, character set,
  truecolor or plain, transparent-edge trimming and brightness inversion. The animation
  is previewed live and can be downloaded or copied.
* The output format is documented in [`../examples/README.md`](../examples/README.md);
  generated files are validated against moofetch's real parser by
  `tests/web_converter.test.cjs`.
