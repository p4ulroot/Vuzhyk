# -*- coding: utf-8 -*-
"""Persistent turtle session for a Vuzhyk task tab via turtlexy_fvt.

The same Turtle instance is shown at rest and then moved when the student
program runs — there is no separate Qt-drawn preview arrow.
"""
from __future__ import print_function

import argparse
import os
import sys
import traceback


_STEP_NAMES = (
    "forward", "fd", "backward", "bk", "back",
    "right", "rt", "left", "lt",
    "goto", "setpos", "setposition", "setx", "sety",
    "setheading", "seth", "home", "circle", "dot",
)

_step_saved = {}
_step_wrapped = False


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--script", default="student.py")
    p.add_argument("--width", type=int, default=400)
    p.add_argument("--height", type=int, default=400)
    p.add_argument("--sample-forward", type=int, default=0)
    p.add_argument("--sample-path", default="")
    p.add_argument("--check", default="")
    return p.parse_args()


def resolve_sample_path(args):
    path = (args.sample_path or "").strip()
    if path:
        return path
    if args.sample_forward:
        return "F%d" % args.sample_forward
    return ""


def parse_sample_path(path):
    tokens = path.replace(";", " ").split()
    cmds = []
    i = 0
    while i < len(tokens):
        raw = tokens[i]
        op = raw[:1].upper()
        rest = raw[1:]
        if op in ("F", "B", "L", "R") and rest:
            cmds.append((op, float(rest)))
            i += 1
            continue
        if op in ("F", "B", "L", "R") and i + 1 < len(tokens):
            cmds.append((op, float(tokens[i + 1])))
            i += 2
            continue
        i += 1
    return cmds


def apply_sample_path(turtle, path):
    turtle.pendown()
    for op, value in parse_sample_path(path):
        if op == "F":
            turtle.forward(value)
        elif op == "B":
            turtle.backward(value)
        elif op == "L":
            turtle.left(value)
        elif op == "R":
            turtle.right(value)
    turtle.penup()
    turtle.home()


def remap_turtle_imports():
    import turtlexy_fvt
    sys.modules["turtle"] = turtlexy_fvt
    sys.modules["turtlexy"] = turtlexy_fvt
    sys.modules["turtlexy_fvt"] = turtlexy_fvt
    return turtlexy_fvt


def wrap_for_step(module):
    global _step_wrapped
    if _step_wrapped:
        return
    from turtlexy_fvt._tk import wait_step

    for name in _STEP_NAMES:
        if not hasattr(module.Turtle, name):
            continue
        orig = getattr(module.Turtle, name)
        _step_saved[name] = orig

        def make(o):
            def wrapped(self, *args, **kwargs):
                result = o(self, *args, **kwargs)
                wait_step()
                return result
            return wrapped

        setattr(module.Turtle, name, make(orig))
    _step_wrapped = True


def unwrap_for_step(module):
    global _step_wrapped
    if not _step_wrapped:
        return
    for name, orig in _step_saved.items():
        setattr(module.Turtle, name, orig)
    _step_saved.clear()
    _step_wrapped = False


def _alive(screen, turtle):
    try:
        return turtle is not None and turtle in screen._turtles
    except Exception:
        return False


def _style_sample(sample):
    sample.hideturtle()
    sample.speed(0)
    sample.pencolor("#8EC5FF")
    sample.pensize(2)


def _draw_sample(T, path):
    sample = T.Turtle()
    _style_sample(sample)
    if path:
        apply_sample_path(sample, path)
    return sample


def _redraw_sample(sample, path):
    sample.reset()
    _style_sample(sample)
    if path:
        apply_sample_path(sample, path)


def _pause_animation(screen):
    # tracer(0) only — the second argument would overwrite the drawing delay.
    screen.tracer(0)


def _resume_animation(screen, T):
    delay = int(T._CFG.get("delay", 10) or 10)
    screen.delay(delay)
    screen.tracer(1, delay)


def setup_world(T, sample_path):
    screen = T.Screen()
    _pause_animation(screen)
    student = T.Turtle()
    student.showturtle()
    sample = _draw_sample(T, sample_path) if sample_path else None
    T.Turtle._pen = student
    _resume_animation(screen, T)
    screen.update()
    return student, sample


def restore_world(T, student, sample, sample_path):
    screen = T.Screen()
    _pause_animation(screen)

    keep = set()
    if not _alive(screen, student):
        student = T.Turtle()
    student.reset()
    student.showturtle()
    keep.add(student)

    if sample_path:
        if not _alive(screen, sample):
            sample = _draw_sample(T, sample_path)
        else:
            _redraw_sample(sample, sample_path)
        keep.add(sample)

    for extra in list(getattr(screen, "_turtles", [])):
        if extra in keep:
            continue
        try:
            extra.hideturtle()
            extra._clear()
            screen._turtles.remove(extra)
        except Exception:
            pass

    T.Turtle._pen = student
    _resume_animation(screen, T)
    screen.update()
    return student, sample


def _emit_check(ok, message):
    from turtlexy_fvt._tk import _emit
    _emit({"op": "check", "ok": bool(ok), "message": message})
    print("VUZHYK_TURTLE_CHECK %s %s" % ("OK" if ok else "FAIL", message), flush=True)


def _segments_from_turtle(turtle, screen):
    segs = []
    for item in list(getattr(turtle, "items", []) or []):
        try:
            pts = screen._pointlist(item)
        except Exception:
            continue
        for i in range(len(pts) - 1):
            x1, y1 = float(pts[i][0]), float(pts[i][1])
            x2, y2 = float(pts[i + 1][0]), float(pts[i + 1][1])
            if (x1 - x2) ** 2 + (y1 - y2) ** 2 > 0.25:
                segs.append(((x1, y1), (x2, y2)))
    return segs


def _student_segments(T, sample):
    screen = T.Screen()
    segs = []
    for turtle in list(getattr(screen, "_turtles", []) or []):
        if sample is not None and turtle is sample:
            continue
        segs.extend(_segments_from_turtle(turtle, screen))
    return segs


def _dist_point_to_seg(px, py, a, b):
    ax, ay = a
    bx, by = b
    vx, vy = bx - ax, by - ay
    length2 = vx * vx + vy * vy
    if length2 < 1e-12:
        return ((px - ax) ** 2 + (py - ay) ** 2) ** 0.5
    t = ((px - ax) * vx + (py - ay) * vy) / length2
    t = max(0.0, min(1.0, t))
    qx, qy = ax + t * vx, ay + t * vy
    return ((px - qx) ** 2 + (py - qy) ** 2) ** 0.5


def _point_near_any_seg(px, py, segs, tol):
    for a, b in segs:
        if _dist_point_to_seg(px, py, a, b) <= tol:
            return True
    return False


def check_sample_line(T, sample, x1=0.0, y1=0.0, x2=100.0, y2=0.0):
    """Compare student ink with one segment. Code does not matter."""
    try:
        T.Screen().update()
    except Exception:
        pass
    segs = _student_segments(T, sample)
    if not segs:
        return False, u"На рисунке нет линии — исправь программу и нарисуй её."

    sample_a, sample_b = (float(x1), float(y1)), (float(x2), float(y2))
    return _compare_to_sample_segs(segs, [(sample_a, sample_b)])


def check_match_sample(T, sample):
    """Compare student ink with every stroke of the hidden sample turtle."""
    try:
        T.Screen().update()
    except Exception:
        pass
    segs = _student_segments(T, sample)
    if not segs:
        return False, u"На рисунке нет линии — исправь программу и нарисуй её."
    if sample is None:
        return False, u"Нет образца для проверки."
    sample_segs = _segments_from_turtle(sample, T.Screen())
    if not sample_segs:
        return False, u"Нет образца для проверки."
    return _compare_to_sample_segs(segs, sample_segs)


def _compare_to_sample_segs(student_segs, sample_segs):
    tol = 4.0
    steps = 20
    for a, b in sample_segs:
        for i in range(steps + 1):
            t = i / float(steps)
            px = a[0] + (b[0] - a[0]) * t
            py = a[1] + (b[1] - a[1]) * t
            if not _point_near_any_seg(px, py, student_segs, tol):
                return False, u"Рисунок не совпадает с образцом."

    for a, b in student_segs:
        if not _point_near_any_seg(a[0], a[1], sample_segs, tol):
            return False, u"На рисунке есть лишние линии."
        if not _point_near_any_seg(b[0], b[1], sample_segs, tol):
            return False, u"На рисунке есть лишние линии."
    return True, u"Верно: рисунок совпадает с образцом."


def run_student(script_path):
    globals_dict = {
        "__name__": "__main__",
        "__file__": script_path,
        "__builtins__": __builtins__,
    }
    with open(script_path, "r", encoding="utf-8") as f:
        source = f.read()
    code = compile(source, script_path, "exec")
    exec(code, globals_dict)


def main():
    args = parse_args()
    os.environ["VUZHYK_FVT"] = "1"
    os.environ["VUZHYK_FVT_WIDTH"] = str(args.width)
    os.environ["VUZHYK_FVT_HEIGHT"] = str(args.height)
    sys.argv = [args.script]

    T = remap_turtle_imports()
    T._CFG["canvwidth"] = args.width
    T._CFG["canvheight"] = args.height
    T._CFG["width"] = args.width
    T._CFG["height"] = args.height

    from turtlexy_fvt._tk import wait_app_command, clear_cancel

    sample_path = resolve_sample_path(args)
    student, sample = setup_world(T, sample_path)
    dirty = False
    print("VUZHYK_TURTLE_READY", flush=True)

    while True:
        cmd = wait_app_command()
        parts = cmd.split(None, 1)
        name = parts[0].upper() if parts else ""

        if name in ("QUIT", "EXIT"):
            break

        if name == "RESET":
            unwrap_for_step(T)
            clear_cancel()
            student, sample = restore_world(T, student, sample, sample_path)
            dirty = False
            print("VUZHYK_TURTLE_IDLE", flush=True)
            continue

        if name != "RUN":
            continue

        step = False
        check = args.check
        if len(parts) > 1:
            tokens = parts[1].split()
            if tokens:
                step = tokens[0] in ("1", "true", "True", "step")
            if len(tokens) > 1:
                check = tokens[1]

        unwrap_for_step(T)
        clear_cancel()
        if dirty:
            student, sample = restore_world(T, student, sample, sample_path)
        if step:
            wrap_for_step(T)

        had_error = False
        cancelled = False
        try:
            run_student(args.script)
        except SystemExit:
            cancelled = True
        except Exception:
            traceback.print_exc()
            print("VUZHYK_TURTLE_ERROR", flush=True)
            had_error = True
        finally:
            unwrap_for_step(T)
            if check and not cancelled:
                if had_error:
                    _emit_check(
                        False,
                        u"В программе ошибка — исправь её и выполни снова.")
                else:
                    ok, message = check_match_sample(T, sample)
                    _emit_check(ok, message)
            clear_cancel()
            dirty = True
            print("VUZHYK_TURTLE_DONE", flush=True)
            print("VUZHYK_TURTLE_IDLE", flush=True)


if __name__ == "__main__":
    main()
