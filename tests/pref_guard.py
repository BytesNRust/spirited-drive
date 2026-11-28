"""Every registered test runs with TNFS_PREF_DIR set to a scratch folder in the build, so none can reach the player's own folder
(~/Library/Application Support/Spirited Drive/ and its kin) by default: a tnfs started with no --user writes its log, its settings and a
Replay.rpl there (found 2026-11-10: a gate had rewritten the real Replay.rpl and spirited-drive.log).
usage: pref_guard.py CTEST BUILD_DIR [VAR]   (VAR: TNFS_PREF_DIR)"""
import json
import subprocess
import sys

FEWEST = 100   # fewer tests listed than this is not the suite's listing


def has_scratch(test, prefix):
    return any(entry.startswith(prefix)
               for prop in test.get("properties", []) if prop["name"] == "ENVIRONMENT"
               for entry in prop["value"])


def main(argv):
    ctest, build = argv[1], argv[2]
    var = argv[3] if len(argv) > 3 else "TNFS_PREF_DIR"
    listing = subprocess.run([ctest, "--test-dir", build, "--show-only=json-v1"], capture_output=True, check=False)
    if listing.returncode != 0:
        sys.exit(f"ctest --show-only failed ({listing.returncode})")
    tests = json.loads(listing.stdout.decode("utf-8", "replace"))["tests"]
    if len(tests) < FEWEST:
        sys.exit(f"only {len(tests)} tests listed: the listing is not the suite's")
    missing = [t["name"] for t in tests if not has_scratch(t, f"{var}={build}/")]
    if missing:
        sys.exit(f"tests with no {var}=<build>/... in their environment: {' '.join(missing)}")
    print(f"pref_guard: all {len(tests)} tests have {var} in the build")


if __name__ == "__main__":
    main(sys.argv)
