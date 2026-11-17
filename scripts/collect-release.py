import hashlib
import json
import os
import re
import shutil
import subprocess
import zipfile
from pathlib import Path


root = Path(__file__).resolve().parents[1]
package = root / 'native/build/package'
release = root / 'release'
platforms = {'macos-arm64': 'libdoctrine.dylib', 'windows-x86_64': 'doctrine.dll'}
minecraft_versions = ('26.2', '26.3')
licenses = {
    'FreeType-LICENSE.txt', 'RmlUi-LICENSE.txt', 'VulkanMemoryAllocator-LICENSE.txt',
    'stb-LICENSE.txt', 'volk-LICENSE.txt',
}


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def native_files():
    if {path.name for path in package.iterdir()} != set(platforms):
        raise SystemExit('Expected only the macOS ARM64 and Windows x64 native packages')
    result = {}
    for platform, library in platforms.items():
        directory = package / platform
        expected = licenses | {library, 'ui.vert.spv', 'ui.frag.spv'}
        if {path.name for path in directory.iterdir()} != expected:
            raise SystemExit(f'Unexpected native package contents for {platform}')
        for name in sorted(expected):
            data = (directory / name).read_bytes()
            if not data:
                raise SystemExit(f'Empty native package entry: {platform}/{name}')
            result[f'natives/{platform}/{name}'] = data
    return result


def validate_jar(source, version, expected):
    with zipfile.ZipFile(source) as jar:
        metadata = json.loads(jar.read('fabric.mod.json'))
        if metadata['id'] != 'doctrine' or metadata['version'] != version:
            raise SystemExit(f'Wrong mod id or version in {source.name}')
        entries = [name for name in jar.namelist() if name.startswith('natives/') and not name.endswith('/')]
        if sorted(entries) != sorted(expected):
            raise SystemExit(f'Missing, duplicate or unexpected native entries in {source.name}')
        for name, data in expected.items():
            if jar.read(name) != data:
                raise SystemExit(f'Packaged bytes differ from the native build: {source.name}: {name}')


def main():
    version = re.search(r'^mod_version=(\S+)$', (root / 'gradle.properties').read_text(), re.MULTILINE).group(1)
    if os.environ.get('GITHUB_REF_TYPE') == 'tag' and os.environ.get('GITHUB_REF_NAME') != f'v{version}':
        raise SystemExit('Release tag must match gradle.properties')
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
    if os.environ.get('GITHUB_SHA', commit) != commit:
        raise SystemExit('Checked-out source does not match the workflow commit')
    expected = native_files()
    jars = {}
    for minecraft in minecraft_versions:
        mod_version = f'{version}+{minecraft}'
        source = root / 'versions' / minecraft / 'build/libs' / f'doctrine-{mod_version}.jar'
        validate_jar(source, mod_version, expected)
        jars[source.name] = source
    release.mkdir(exist_ok=False)
    hashes = {}
    for name, source in jars.items():
        destination = release / name
        shutil.copyfile(source, destination)
        hashes[name] = sha256(destination.read_bytes())
    (release / 'SHA256SUMS.txt').write_text(''.join(f'{digest}  {name}\n' for name, digest in hashes.items()))
    repository = os.environ.get('GITHUB_REPOSITORY', 'riye-prog/enchant-doctor')
    server = os.environ.get('GITHUB_SERVER_URL', 'https://github.com')
    run = os.environ.get('GITHUB_RUN_ID')
    provenance = {
        'source': f'{server}/{repository}/tree/{commit}',
        'commit': commit,
        'workflow_run': f'{server}/{repository}/actions/runs/{run}' if run else None,
        'run_attempt': os.environ.get('GITHUB_RUN_ATTEMPT'),
        'sha256': hashes,
        'native_entries_sha256': {name: sha256(data) for name, data in expected.items()},
    }
    (release / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    print(json.dumps(provenance, indent=2))


if __name__ == '__main__':
    main()
