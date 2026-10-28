# turtlexy_fvt

Private fork of [turtlexy](https://pypi.org/project/turtlexy/) for **Vuzhyk** task tabs.

This package is **not published** to PyPI. It ships inside the embedded Python interpreter (`Python38/Lib/site-packages/turtlexy_fvt`).

## What it does

- Same turtle API as `turtle` / `turtlexy` (forward, circle, fill, stamps, events, …).
- Drawing goes to the Vuzhyk task panel over stdout (`FVT …` JSON), not to a Tkinter window.
- Coordinate grid is **off by default**. Call `show_grid()` if you need it.

When a student runs code from a **task tab**, Vuzhyk remaps `import turtle` and `import turtlexy` to this package at runtime. The editor still shows `from turtle import *`.
