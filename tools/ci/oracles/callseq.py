"""Compare the ordered call sequence of a function before and after a split.

usage: callseq.py FILE OLD_REF ENTRY

Only the helpers the split INTRODUCED are inlined (names in the working tree
that did not exist at OLD_REF), so a function carved into new helpers is
compared against the single function it replaced. Pre-existing callees stay
opaque on both sides.

An identical sequence means no call was dropped, added or moved between
branches. It does not check arguments, so it is a pre-build gate, not a
substitute for diffing compiler output.
"""
import difflib
import re
import subprocess
import sys

CALL = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "do", "else",
            "case", "defined", "va_start", "va_end", "va_arg"}
DEFN = re.compile(
    r"^(?:static\s+)?[A-Za-z_][\w \t*]*?\b(\w+)\s*\([^;{]*?\)\s*\{", re.M)


def bodies(text):
    out = {}
    for m in DEFN.finditer(text):
        name = m.group(1)
        if name in KEYWORDS:
            continue
        i = m.end() - 1
        depth = 0
        start = i
        while i < len(text):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    out[name] = text[start:i + 1]
                    break
            i += 1
    return out


def sequence(text, table, inlinable, seen):
    out = []
    for m in CALL.finditer(text):
        name = m.group(1)
        if name in KEYWORDS:
            continue
        if name in inlinable and name in table and name not in seen:
            out.extend(sequence(table[name], table, inlinable, seen | {name}))
        else:
            out.append(name)
    return out


def main():
    path, ref, entry = sys.argv[1], sys.argv[2], sys.argv[3]
    old_text = subprocess.run(["git", "show", "%s:%s" % (ref, path)],
                              capture_output=True, text=True,
                              encoding="utf-8").stdout
    new_text = open(path, encoding="utf-8").read()
    old_bodies = bodies(old_text)
    new_bodies = bodies(new_text)
    if entry not in old_bodies:
        print("%s: not found at %s" % (entry, ref))
        return
    if entry not in new_bodies:
        print("%s: not found in the working tree" % entry)
        return
    introduced = set(new_bodies) - set(old_bodies)
    old_seq = sequence(old_bodies[entry], old_bodies, set(), {entry})
    new_seq = sequence(new_bodies[entry], new_bodies, introduced, {entry})
    print("%s: old %d calls, new %d calls (%d helpers introduced)"
          % (entry, len(old_seq), len(new_seq), len(introduced)))
    diff = list(difflib.unified_diff(old_seq, new_seq, "old", "new",
                                     lineterm="", n=1))
    if not diff:
        print("  IDENTICAL call sequence")
        return
    for line in diff[:80]:
        print("  " + line)


if __name__ == "__main__":
    main()
