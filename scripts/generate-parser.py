#!/usr/bin/env python3
"""Generate an isolated parser from pinned fork gitlinks without reapplying semantic patches."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parent.parent
DEST = ROOT / "Sources"
MANIFEST = ROOT / "scripts/parser-manifest.json"
MODULES = {"cmark_gfm": "AIMarkdownCMark", "cmark_gfm_extensions": "AIMarkdownCMarkExtensions",
           "CAtomic": "AIMarkdownCAtomic"}
# Preserve angle includes as well as quoted includes until header-name isolation.
TOKENS = re.compile(r'^[ \t]*#[ \t]*include[ \t]*<[^>\n]+>|//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\b[A-Za-z_][A-Za-z_0-9]*\b', re.S | re.M)


def run(args, **kwargs):
    return subprocess.check_output(args, text=True, **kwargs)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def hashes(directory):
    return {str(p.relative_to(directory)): sha(p) for p in sorted(directory.rglob("*"))
            if p.is_file()}


def gitlinks():
    result = {}
    for name in ["swift-markdown", "swift-cmark"]:
        record = run(["git", "-C", str(ROOT), "ls-files", "--stage", "--", "upstream/" + name]).strip().split()
        if len(record) != 4 or record[0] != "160000" or record[2] != "0":
            raise RuntimeError("Stage an explicit submodule revision first: " + name)
        result[name] = record[1]
    return result


def export(checkout, name, target):
    revision = gitlinks()[name]
    archive = subprocess.check_output(["git", "-C", str(checkout), "archive", revision])
    target.mkdir()
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        for member in tar.getmembers():
            if member.issym() or member.islnk() or member.name.startswith("/") or ".." in Path(member.name).parts:
                raise RuntimeError("Unsafe fork archive entry: " + member.name)
        tar.extractall(target)


def symbols(directory):
    """Enumerate external definitions from object files, including internal globals without a cmark prefix."""
    definitions = set()
    with tempfile.TemporaryDirectory(prefix="aimarkdown-symbols-") as temp:
        for index, source in enumerate(sorted(directory.rglob("*.c"))):
            obj = Path(temp) / f"{index}.o"
            subprocess.run(["clang", "-std=c99", "-c", str(source), "-o", str(obj),
                            "-I", str(directory / "AIMarkdownCMark/include"),
                            "-I", str(directory / "AIMarkdownCMarkExtensions/include"),
                            "-I", str(directory / "AIMarkdownCMarkExtensions"),
                            "-I", str(directory / "AIMarkdownCAtomic/include")], check=True, capture_output=True)
            for line in run(["nm", "-g", str(obj)]).splitlines():
                match = re.search(r"\b[BCDGIRSTVW]\s+(\S+)$", line)
                if match:
                    name = match[1]
                    definitions.add(name[1:] if platform.system() == "Darwin" else name)
    return sorted(definitions)


def generate(args, output):
    with tempfile.TemporaryDirectory(prefix="aimarkdown-sources-") as temp:
        temp = Path(temp)
        for name, checkout in [("swift-markdown", args.markdown_checkout), ("swift-cmark", args.cmark_checkout)]:
            export(checkout, name, temp / name)
        selections = [("swift-markdown", "Sources/Markdown", "AIMarkdownParser", {".swift"}),
                      ("swift-markdown", "Sources/CAtomic", "AIMarkdownCAtomic", {".c", ".h", ".modulemap"}),
                      ("swift-cmark", "src", "AIMarkdownCMark", {".c", ".h", ".inc", ".modulemap"}),
                      ("swift-cmark", "extensions", "AIMarkdownCMarkExtensions", {".c", ".h", ".inc", ".modulemap"})]
        for name, source_dir, dest_dir, suffixes in selections:
            source = temp / name / source_dir
            for path in sorted(source.rglob("*")):
                if path.is_file() and path.suffix in suffixes:
                    dest = output / dest_dir / path.relative_to(source)
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(path, dest)
        for name, files in {"swift-markdown": ["LICENSE.txt", "NOTICE.txt"], "swift-cmark": ["COPYING"]}.items():
            for filename in files:
                license_path = ROOT / "Licenses" / name / filename
                if license_path.read_bytes() != (temp / name / filename).read_bytes():
                    raise RuntimeError("Update the bundled upstream license: " + str(license_path))
        original_symbols = symbols(output)
        identifiers = set()
        for path in output.rglob("*"):
            if path.is_file() and "Licenses" not in path.parts:
                identifiers.update(re.findall(r"\b(?:cmark_\w+|CMARK_\w+|houdini_\w+|_cmarkup_\w+)\b", path.read_text()))
        mapping = {name: ("AIMARKDOWN_" if name.startswith("CMARK_") else "aimarkdown_") + name
                   for name in identifiers | set(original_symbols)}
        mapping.update(MODULES)
        headers = {path.name: "aimarkdown-" + path.name for path in output.rglob("*.h")}
        for path in output.rglob("*"):
            if not path.is_file() or "Licenses" in path.parts:
                continue
            content = path.read_text()
            content = TOKENS.sub(lambda match: mapping.get(match[0], match[0]), content)
            # Explicit module scanning selects dependencies by header name; isolate filenames to prevent the official module from selecting this copy.
            content = re.sub(r'(?m)(#\s*include\s*[<"]|\bheader\s+")([^>"\n]+)([>"])',
                             lambda match: match[1] + headers.get(match[2], match[2]) + match[3], content)
            content = "// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.\n" + content
            path.write_text(content)
        for path in list(output.rglob("*.h")):
            path.rename(path.with_name(headers[path.name]))
        renamed = symbols(output)
        expected = sorted(mapping[name] for name in original_symbols)
        if renamed != expected:
            raise RuntimeError("C symbol prefix audit failed")
        manifest = {
            "schemaVersion": 4,
            "sourceRoot": "Sources",
            "generatorSHA256": sha(Path(__file__)),
            "upstream": {name: {"url": f"https://github.com/ai-markdown/{name}.git", "revision": revision}
                         for name, revision in gitlinks().items()},
            "licenseHashes": hashes(ROOT / "Licenses"),
            "swiftModule": "AIMarkdownParser", "cModules": list(MODULES.values()),
            "selection": [{"upstream": name, "source": source, "destination": dest, "suffixes": sorted(suffixes)}
                          for name, source, dest, suffixes in selections],
            "transform": "Export pinned fork sources and isolate modules, C identifiers, and headers without reapplying semantic patches.",
            "headerNames": dict(sorted(headers.items())),
            "cGlobalSymbols": {name: mapping[name] for name in original_symbols},
            "files": hashes(output),
        }
        return manifest


def check():
    manifest = json.loads(MANIFEST.read_text())
    if manifest["generatorSHA256"] != sha(Path(__file__)):
        raise RuntimeError("The generator has changed; rebuild the snapshot")
    if manifest["licenseHashes"] != hashes(ROOT / "Licenses"):
        raise RuntimeError("License sources have changed")
    if (ROOT / ".git").exists() and gitlinks() != {name: entry["revision"] for name, entry in manifest["upstream"].items()}:
        raise RuntimeError("Gitlinks do not match the generated snapshot revisions")
    actual = hashes(DEST)
    if actual != manifest["files"]:
        raise RuntimeError("Generated output does not match the manifest")
    print(f"Parser snapshot verified: {len(actual)} files/{len(manifest['cGlobalSymbols'])} C symbols")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Check source and license hashes without upstream checkouts or a compiler")
    parser.add_argument("--rebuild-check", action="store_true", help="Rebuild from pinned Git objects and compare bytes without modifying Sources")
    parser.add_argument("--markdown-checkout", type=Path, default=ROOT / "upstream/swift-markdown")
    parser.add_argument("--cmark-checkout", type=Path, default=ROOT / "upstream/swift-cmark")
    args = parser.parse_args()
    if args.check:
        check()
    if args.rebuild_check or not args.check:
        with tempfile.TemporaryDirectory(prefix="aimarkdown-sources-output-") as temp:
            output = Path(temp) / "Sources"
            output.mkdir()
            manifest = generate(args, output)
            manifest_bytes = (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode()
            if args.rebuild_check:
                if hashes(output) != hashes(DEST) or manifest_bytes != MANIFEST.read_bytes():
                    raise RuntimeError("The rebuild from pinned Git objects differs from Sources")
                print("The rebuild from pinned Git objects is byte-identical to Sources")
            else:
                if DEST.exists():
                    shutil.rmtree(DEST)
                DEST.parent.mkdir(parents=True, exist_ok=True)
                shutil.copytree(output, DEST)
                MANIFEST.write_bytes(manifest_bytes)
                check()


if __name__ == "__main__":
    main()
