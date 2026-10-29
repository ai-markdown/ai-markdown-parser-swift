#!/usr/bin/env python3
"""Copy upstream tests from the pinned swift-markdown gitlink, replacing only module imports and qualified names."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parent.parent
DEST = ROOT / 'Tests/UpstreamMarkdownTests'
MANIFEST = ROOT / 'Tests/UpstreamMarkdownTests.manifest.json'

def sha(data): return hashlib.sha256(data).hexdigest()
def hashes(root): return {str(p.relative_to(root)):sha(p.read_bytes()) for p in sorted(root.rglob('*')) if p.is_file()}
def generate(output):
    row=subprocess.check_output(['git','-C',str(ROOT),'ls-files','--stage','--','upstream/swift-markdown'],text=True).split()
    assert len(row)==4 and row[0]=='160000' and row[2]=='0'
    revision=row[1]
    data=subprocess.check_output(['git','-C',str(ROOT/'upstream/swift-markdown'),'archive',revision,'Tests/MarkdownTests'])
    original={}
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        for member in archive.getmembers():
            if member.isdir(): continue
            if not member.isfile() or '..' in Path(member.name).parts: raise RuntimeError('Unsafe archive')
            relative=Path(member.name).relative_to('Tests/MarkdownTests')
            content=archive.extractfile(member).read(); original[str(relative)]=sha(content)
            if relative.suffix=='.swift':
                content=content.decode().replace('import Markdown','import AIMarkdownParser').replace('Markdown.Markup','AIMarkdownParser.Markup').encode()
            target=output/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(content)
    return {'revision':revision,'generatorSHA256':sha(Path(__file__).read_bytes()),'originalHashes':original,'files':hashes(output),'transform':'Replace only import Markdown and two Markdown.Markup qualified names; preserve original assertions and resources.'}

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check',action='store_true')
parser.add_argument('--rebuild-check',action='store_true')
args=parser.parse_args()
if args.check:
    manifest=json.loads(MANIFEST.read_text());assert manifest['files']==hashes(DEST)
    assert manifest['generatorSHA256']==sha(Path(__file__).read_bytes())
    print(f"Upstream tests: {len(manifest['files'])} files passed integrity checks")
if args.rebuild_check or not args.check:
    with tempfile.TemporaryDirectory() as temp:
        output=Path(temp);manifest=generate(output)
        if args.rebuild_check:
            assert manifest==json.loads(MANIFEST.read_text()) and hashes(output)==hashes(DEST)
            print('Upstream tests rebuilt from the pinned gitlink are byte-identical')
        else:
            if DEST.exists():shutil.rmtree(DEST)
            shutil.copytree(output,DEST)
            MANIFEST.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
