"""Strip comments from headers, keeping the token stream identical.

usage: striphdr.py FILE [FILE...]
"""
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from comments import spans

GCC = "gcc"
CFLAGS = ["-E", "-P", "-Iinclude", "-Isrc", "-std=c99", "-D_GNU_SOURCE"]


def tokens_of(header):
    with tempfile.NamedTemporaryFile("w", suffix=".c", delete=False,
                                     encoding="utf-8") as f:
        f.write('#include "%s"\n' % header.replace("\\", "/"))
        probe = f.name
    try:
        out = subprocess.run([GCC] + CFLAGS + [probe], capture_output=True,
                             text=True)
        if out.returncode != 0:
            return None, out.stderr
        return "".join(out.stdout.split()), ""
    finally:
        os.unlink(probe)


def strip(text):
    found = spans(text)
    if not found:
        return text
    out = []
    last = 0
    for a, b in found:
        out.append(text[last:a])
        out.append(" " if text[a:b].startswith("/*") and "\n" not in text[a:b]
                   else "")
        last = b
    out.append(text[last:])
    result = "".join(out)
    lines = []
    for line in result.split("\n"):
        if line.strip() == "":
            lines.append("")
        else:
            lines.append(line.rstrip())
    text = "\n".join(lines)
    while "\n\n\n" in text:
        text = text.replace("\n\n\n", "\n\n")
    return text


def main():
    for path in sys.argv[1:]:
        before, err = tokens_of(path)
        if before is None:
            print("SKIP %s (probe failed): %s" % (path, err.strip()[:120]))
            continue
        raw = open(path, "rb").read()
        newline = "\r\n" if b"\r\n" in raw else "\n"
        text = raw.decode("utf-8")
        stripped = strip(text)
        if stripped == text:
            print("clean %s" % path)
            continue
        open(path, "w", encoding="utf-8", newline="").write(
            stripped.replace("\n", newline) if newline == "\r\n" else stripped)
        after, err = tokens_of(path)
        if after != before:
            open(path, "w", encoding="utf-8", newline="").write(text)
            print("REVERTED %s: token stream changed (%s)" % (path, err[:80]))
            continue
        print("stripped %s (%d -> %d bytes)"
              % (path, len(text), len(stripped)))


if __name__ == "__main__":
    main()
