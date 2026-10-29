#!/usr/bin/env python3
"""Verify Git URL installation without submodules and coexistence with official Markdown using local bare mirrors, not path dependencies."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,default=ROOT/'.build/git-consumer-validation')
parser.add_argument('--revision',default='HEAD')
args=parser.parse_args()
output=args.output.resolve()
if output.exists():raise SystemExit('The output directory already exists; choose a new directory to preserve validation records')
output.mkdir(parents=True)
consumer=output/'consumer';(consumer/'Sources/Consumer').mkdir(parents=True)
repos=output/'repositories';repos.mkdir()
env=os.environ.copy();env.pop('SWIFTCI_USE_LOCAL_DEPS',None)
log=(output/'commands.log').open('w')
def run(arguments,cwd=ROOT):
    log.write('$ '+' '.join(map(str,arguments))+'\n');log.flush()
    result=subprocess.run(list(map(str,arguments)),cwd=cwd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    log.write(result.stdout);log.flush()
    if result.returncode:raise RuntimeError('Command failed; see '+str(output/'commands.log'))
    return result.stdout.strip()
revision=run(['git','rev-parse',args.revision])
baselines=json.loads((ROOT/'scripts/consumer-probe/baselines.json').read_text())
mirror_commands=[]
for name,source,tag in [('ai-markdown-parser-swift',ROOT,None)]+[(name,ROOT/'upstream'/name,baseline) for name,baseline in baselines.items()]:
    mirror=repos/(name+'.git');run(['git','clone','--bare',source,mirror])
    if tag:
        existing=subprocess.run(['git','--git-dir',str(mirror),'rev-parse','--verify','refs/tags/0.9.0'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode==0
        if not existing:run(['git','--git-dir',mirror,'tag','0.9.0',tag])
        assert run(['git','--git-dir',mirror,'rev-parse','0.9.0^{commit}'])==tag
    owner='ai-markdown' if name.startswith('ai-') else 'swiftlang'
    mirror_commands.append(['swift','package','config','set-mirror','--original',f'https://github.com/{owner}/{name}.git','--mirror',mirror.as_uri()])
(consumer/'Package.swift').write_text('''// swift-tools-version: 6.2
import PackageDescription
let package = Package(name: "ParserConsumer", platforms: [.macOS(.v26)], dependencies: [
    .package(url: "https://github.com/ai-markdown/ai-markdown-parser-swift.git", revision: "'''+revision+'''"),
    .package(url: "https://github.com/swiftlang/swift-markdown.git", exact: "0.9.0"),
], targets: [.executableTarget(name: "Consumer", dependencies: [
    .product(name: "AIMarkdownParser", package: "ai-markdown-parser-swift"),
    .product(name: "Markdown", package: "swift-markdown"),
])])
''')
shutil.copyfile(ROOT/'scripts/consumer-probe/main.swift',consumer/'Sources/Consumer/main.swift')
# Package.swift must exist first so SwiftPM does not write local configuration into an ancestor maintenance repository.
for command in mirror_commands:run(command,consumer)
results=[]
manifest=json.loads((ROOT/'Vendor/Parser/manifest.json').read_text())
for mode in ['debug','release']:
    text=run(['swift','run','-c',mode,'Consumer'],consumer)
    assert 'Official Markdown and AIMarkdownParser coexistence passed with real Git dependencies' in text
    symbols=set(re.findall(r'\b[BCDGIRSTVW]\s+_(\S+)',run(['nm','-g',consumer/'.build'/mode/'Consumer'])))
    for name in ['parser_new','parser_finish']:
        assert 'aimarkdown_cmark_'+name in symbols and 'cmark_'+name in symbols
    results.append({'configuration':mode,'customDefinitions':len(set(manifest['cGlobalSymbols'].values())&symbols),'officialOriginalDefinitions':len(set(manifest['cGlobalSymbols'])&symbols)})
checkout=consumer/'.build/checkouts/ai-markdown-parser-swift'
assert run(['git','rev-parse','HEAD'],checkout)==revision
for name in ['swift-markdown','swift-cmark']:
    assert not (checkout/'upstream'/name/'.git').exists()
status=run(['git','submodule','status'],checkout)
assert all(line.startswith('-') for line in status.splitlines())
run(['python3','scripts/generate-parser.py','--check'],checkout)
run(['python3','scripts/sync-upstream-tests.py','--check'],checkout)
(output/'result.json').write_text(json.dumps({'revision':revision,'results':results,'submodulesUninitialized':True,'dependencyKind':'Git URL with temporary local bare mirrors, not path dependencies; remote publication has not been verified','submoduleStatus':status},ensure_ascii=False,indent=2)+'\n')
print('Git dependency coexistence passed in Debug/Release without submodule initialization: '+str(output))
