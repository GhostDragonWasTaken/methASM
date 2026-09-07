"""Strip line comments from Mettle sources.

usage: stripmettle.py FILE [FILE...]

A comment on its own line takes the line with it; a trailing comment leaves the
code and the indentation before it. Runs of blank lines collapse to one.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mettle_comments import line_comments


def strip(text):
    found = line_comments(text)
    if not found:
        return text
    mask = bytearray(len(text))
    for a, b in found:
        for i in range(a, b):
            mask[i] = 1

    out = []
    offset = 0
    for line in text.split("\n"):
        span = mask[offset:offset + len(line)]
        offset += len(line) + 1
        if not any(span):
            out.append(line.rstrip())
            continue
        kept = "".join(c for c, m in zip(line, span) if not m).rstrip()
        if kept.strip() == "":
            continue
        out.append(kept)
    text = "\n".join(out)
    while "\n\n\n" in text:
        text = text.replace("\n\n\n", "\n\n")
    return text.lstrip("\n")


def main():
    changed = 0
    for path in sys.argv[1:]:
        raw = open(path, "rb").read()
        crlf = b"\r\n" in raw
        text = raw.decode("utf-8", errors="replace")
        if crlf:
            text = text.replace("\r\n", "\n")
        stripped = strip(text)
        if stripped == text:
            continue
        body = stripped.replace("\n", "\r\n") if crlf else stripped
        open(path, "w", encoding="utf-8", newline="").write(body)
        changed += 1
    print("stripped %d files" % changed)


if __name__ == "__main__":
    main()
