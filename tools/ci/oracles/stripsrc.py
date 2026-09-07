"""Strip comments from C sources, proving only comments were removed.

Verification: `gcc -fpreprocessed -dD -E -P` on the file itself, which drops
comments without resolving includes. The two runs must agree token for token.

usage: stripsrc.py FILE [FILE...]
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from comments import spans

GCC = "gcc"
FLAGS = ["-fpreprocessed", "-dD", "-E", "-P", "-x", "c"]


def tokens_of(path):
    out = subprocess.run([GCC] + FLAGS + [path], capture_output=True,
                         text=True, encoding="utf-8", errors="replace")
    if out.returncode != 0:
        return None
    return "".join(out.stdout.split())


def strip(text):
    found = spans(text)
    if not found:
        return text
    out = []
    last = 0
    for a, b in found:
        out.append(text[last:a])
        if text[a:b].startswith("/*") and "\n" not in text[a:b]:
            out.append(" ")
        last = b
    out.append(text[last:])
    result = "".join(out)
    result = "\n".join(line.rstrip() for line in result.split("\n"))
    while "\n\n\n" in result:
        result = result.replace("\n\n\n", "\n\n")
    return result


def main():
    changed = 0
    for path in sys.argv[1:]:
        before = tokens_of(path)
        if before is None:
            print("SKIP %s (probe failed)" % path)
            continue
        raw = open(path, "rb").read()
        crlf = b"\r\n" in raw
        text = raw.decode("utf-8", errors="replace")
        stripped = strip(text)
        if stripped == text:
            continue
        body = stripped.replace("\n", "\r\n") if crlf else stripped
        open(path, "w", encoding="utf-8", newline="").write(body)
        after = tokens_of(path)
        if after != before:
            open(path, "w", encoding="utf-8", newline="").write(text)
            print("REVERTED %s: token stream changed" % path)
            continue
        changed += 1
        print("stripped %s (%d -> %d bytes)" % (path, len(text),
                                                len(stripped)))
    print("changed %d files" % changed)


if __name__ == "__main__":
    main()
