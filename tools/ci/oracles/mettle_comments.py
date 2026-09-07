import os
import sys

BACKSLASH = chr(92)


def line_comments(text):
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"':
            i += 1
            while i < n:
                if text[i] == BACKSLASH:
                    i += 2
                    continue
                if text[i] == '"':
                    i += 1
                    break
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append((i, j))
            i = j
            continue
        i += 1
    return out


def main():
    show = "--show" in sys.argv
    roots = [a for a in sys.argv[1:] if a != "--show"]
    rows = []
    total = 0
    for root_dir in roots:
        for root, _, files in os.walk(root_dir):
            for f in files:
                if not f.endswith(".mettle"):
                    continue
                path = os.path.join(root, f)
                text = open(path, encoding="utf-8", errors="replace").read()
                found = line_comments(text)
                if not found:
                    continue
                rows.append((len(found), path))
                total += len(found)
                if show:
                    for a, b in found[:6]:
                        line = text.count("\n", 0, a) + 1
                        print("%s:%d: %s" % (path, line, text[a:b][:100]))
    rows.sort(reverse=True)
    for n, path in rows[:15]:
        print("%5d  %s" % (n, path))
    print("files: %d  comments: %d" % (len(rows), total))


if __name__ == "__main__":
    main()
