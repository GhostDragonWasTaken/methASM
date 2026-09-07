"""Report real C comments, ignoring string and character literals.

usage: comments.py DIR [DIR...]      list files and comment counts
       comments.py --show FILE       print each comment with its line number
"""
import os
import sys


def spans(text):
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            quote = c
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == quote:
                    i += 1
                    break
                i += 1
            continue
        if c == "/" and i + 1 < n:
            if text[i + 1] == "/":
                j = text.find("\n", i)
                j = n if j < 0 else j
                out.append((i, j))
                i = j
                continue
            if text[i + 1] == "*":
                j = text.find("*/", i + 2)
                j = n if j < 0 else j + 2
                out.append((i, j))
                i = j
                continue
        i += 1
    return out


def scan(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    found = spans(text)
    lines = 0
    for a, b in found:
        lines += text.count("\n", a, b) + 1
    return text, found, lines


def main():
    if sys.argv[1] == "--show":
        path = sys.argv[2]
        text, found, _ = scan(path)
        for a, b in found:
            line = text.count("\n", 0, a) + 1
            body = text[a:b]
            head = body.split("\n")[0][:110]
            print("%s:%d: %s%s" % (path, line, head,
                                   " ..." if "\n" in body else ""))
        return
    total_files = 0
    total_lines = 0
    rows = []
    for root_dir in sys.argv[1:]:
        for root, _, files in os.walk(root_dir):
            for f in files:
                if not f.endswith((".c", ".h")):
                    continue
                path = os.path.join(root, f)
                _, found, lines = scan(path)
                if found:
                    rows.append((lines, len(found), path))
                    total_files += 1
                    total_lines += lines
    rows.sort(reverse=True)
    for lines, count, path in rows[:40]:
        print("%5d lines %4d comments  %s" % (lines, count, path))
    print("files with comments: %d   comment lines: %d"
          % (total_files, total_lines))


if __name__ == "__main__":
    main()
