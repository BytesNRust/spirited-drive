"""README.md, RELEASE_NOTES.md and the README.txt that tools/package_windows.sh writes into the Windows zip: wording from the announcement texts: the phrases the analyst's notes say never to use (a claim about the palette or the filter that is not true of
Restomod), and the lines that must be there (per-mode text, network, Lunar Springs, the 8-bit palette). Stdlib only; usage: readme_wording.py REPO"""
import re
import sys

NEVER = ("the original 256 colours", "the original 256 colors", "keeps the original palette", "not one extra colour", "nothing is smoothed over", "no filtering",
         "no smoothing of textures", "256-colour palette")
MUST = ("Factory spec, recalls done.", "Original paint, new bearings.", "Original block, modern running gear.", "All of it in 8-bit.", "they never leave it",
        "very beta", "on macOS only for now", "ran out of time", "Always in the workshop.", "not available to download yet")


NOTES_MUST = ("All of it still 8-bit", "very beta", "Always in the workshop", "[X.Y.Z]", "Three ways to play")


WINDOWS_MUST = ("unknown publisher", "More info", "Run anyway", "Bring your own CD", "Network races are not in the Windows version yet", "%APPDATA%",
                "spirited-drive.log", "Open the log folder", "Restored '96")


def windows_readme(script):
    """the text between cat > "$DIR/README.txt" <<EOF and the closing EOF"""
    m = re.search(r'cat > "\$DIR/README\.txt" <<EOF\n(.*?)\nEOF\n', script, re.S)
    return m.group(1) if m else ""


def problems(text, must=MUST, draft=True):
    low = re.sub(r"\s+", " ", text).lower()
    flat = re.sub(r"\s+", " ", text.replace("*", ""))
    out = [f"never write: {p}" for p in NEVER if p in low]
    out += [f"missing: {m}" for m in must if m.replace("*", "") not in flat]
    if draft and "DRAFT" not in text:
        out.append("no DRAFT mark: the announcement text waits for the owner's approval")
    return out


def main(repo):
    found = problems(open(f"{repo}/README.md", encoding="utf-8").read())
    found += [f"RELEASE_NOTES.md: {p}" for p in problems(open(f"{repo}/RELEASE_NOTES.md", encoding="utf-8").read(), NOTES_MUST)]
    win = windows_readme(open(f"{repo}/tools/package_windows.sh", encoding="utf-8").read())
    if not win:
        found.append("package_windows.sh: no README.txt text found")
    found += [f"package_windows.sh README.txt: {p}" for p in problems(win, WINDOWS_MUST, draft=False)] if win else []
    if "DRAFT" in win:
        found.append("package_windows.sh README.txt: a DRAFT mark would ship in the zip")
    for p in found:
        print("FAIL:", p)
    planted = problems("Nothing is smoothed over. In the original 256 colours. DRAFT")
    if not any("smoothed over" in p for p in planted) or not any("original 256 colours" in p for p in planted):
        print("FAIL: the check does not see a planted phrase")
        found.append("sabotage")
    print("ok" if not found else f"{len(found)} problems")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
