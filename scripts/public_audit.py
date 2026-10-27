#!/usr/bin/env python3
"""Check tracked history and release files for common private-data leaks."""
from pathlib import Path, PurePosixPath
import argparse
import re
import struct
import subprocess
import tarfile

RULES = {
    'home directory': rb'(?:/(?:home|Users)/[A-Za-z0-9_.-]+/|[A-Za-z]:[\\/]+Users[\\/]+[A-Za-z0-9_.-]+[\\/])',
    'device MAC/serial': rb'(?i)\b(?:[0-9a-f]{2}[:-]){5}[0-9a-f]{2}\b',
    'private network address': rb'\b(?:10\.[0-9]{1,3}|192\.168|172\.(?:1[6-9]|2[0-9]|3[01]))\.[0-9]{1,3}\.[0-9]{1,3}\b',
    'private key': rb'-----BEGIN (?:RSA |OPENSSH |EC |DSA |ENCRYPTED )?PRIVATE KEY-----',
    'access token': rb'\b(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|AKIA[0-9A-Z]{16}|xox[baprs]-[A-Za-z0-9-]{15,})',
    'URL credentials': rb'https?://[^\s/:@]+:[^\s/@]+@',
}
PRIVATE_PARTS = {'.git', '.aws', '.agents', '.codex', 'backups', 'private', 'sdcard'}


def path_findings(name):
    path = PurePosixPath(name)
    result = []
    if path.is_absolute() or '..' in path.parts or '\\' in name or re.match(r'^[A-Za-z]:', name) or any(ord(c)<32 for c in name):
        result.append('unsafe archive path')
    if any(part in PRIVATE_PARTS or part == '.env' or part.startswith('.env.') for part in path.parts):
        result.append('private file/directory')
    if path.suffix.lower() in ('.pem', '.key') or path.name.lower().startswith('x4-backup'):
        result.append('credential or flash backup file')
    return result


def data_findings(data):
    result = [name for name, pattern in RULES.items() if re.search(pattern, data)]
    if data.startswith(b'\x89PNG\r\n\x1a\n'):
        offset = 8
        while offset + 12 <= len(data):
            length = struct.unpack('>I', data[offset:offset+4])[0]
            kind = data[offset+4:offset+8]
            if kind in (b'tEXt', b'zTXt', b'iTXt', b'eXIf'):
                result.append('PNG text/EXIF metadata')
                break
            offset += length + 12
    return result


def check_file(name, data):
    if name.endswith('vendor/raylib/src/external/miniaudio.h'):
        data = data.replace(b'\n10.3.1.1. Linear Resampling\n', b'\n') # Upstream section number.
    return path_findings(name) + data_findings(data)


def git_objects(root, history):
    checkout = subprocess.run(['git', '-C', root, 'rev-parse', '--show-toplevel'], capture_output=True, text=True)
    if checkout.returncode or Path(checkout.stdout.strip()).resolve() != root.resolve():
        if history: raise SystemExit('--history requires a Git checkout')
        for path in sorted(root.rglob('*')):
            relative = path.relative_to(root)
            if relative.parts[0] in ('dist', 'research') or relative.parts[0].startswith('build') or '__pycache__' in relative.parts:
                continue
            if path.is_symlink(): yield relative.as_posix(), b'', ['symlink in public source']
            elif path.is_file(): yield relative.as_posix(), path.read_bytes(), []
        return
    if history:
        names = set()
        for commit in subprocess.check_output(['git', '-C', root, 'rev-list', '--all']).splitlines():
            for entry in subprocess.check_output(['git', '-C', root, 'ls-tree', '-r', '-z', commit]).split(b'\0'):
                if not entry: continue
                metadata, raw = entry.split(b'\t', 1)
                name = raw.decode('utf-8', 'replace')
                if (name, metadata[:6]) in names: continue
                names.add((name, metadata[:6]))
                extra = path_findings(name)
                if metadata.startswith(b'120000'): extra.append('symlink in public source')
                if extra: yield name, b'', extra
        records = subprocess.check_output(['git', '-C', root, 'rev-list', '--objects', '--all']).splitlines()
        objects = [(line.split(b' ', 1)[0], line.partition(b' ')[2].decode('utf-8', 'replace')) for line in records]
    else:
        records = subprocess.check_output(['git', '-C', root, 'ls-files', '-z']).split(b'\0')
        for raw in records:
            if raw:
                name = raw.decode()
                path = root / name
                if path.is_symlink():
                    yield name, b'', ['symlink in public source']
                else:
                    yield name, path.read_bytes(), []
        return
    # Batch protocol preserves binary blobs and never runs repository content.
    process = subprocess.Popen(['git', '-C', root, 'cat-file', '--batch'], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    try:
        for oid, name in objects:
            process.stdin.write(oid + b'\n'); process.stdin.flush()
            header = process.stdout.readline().split()
            if len(header) != 3:
                raise RuntimeError('Cannot read Git object')
            data = process.stdout.read(int(header[2]))
            if process.stdout.read(1) != b'\n':
                raise RuntimeError('Truncated Git object')
            if header[1] == b'blob':
                yield name or oid.decode(), data, []
            elif header[1] in (b'commit', b'tag'):
                yield oid.decode(), data, []
    finally:
        process.stdin.close(); process.stdout.close()
        if process.wait() != 0:
            raise RuntimeError('Git object reader failed')


def release_files(directory):
    for path in sorted(directory.rglob('*')):
        name = path.relative_to(directory).as_posix()
        if path.is_symlink():
            yield name, b'', ['symlink in release']
        elif path.is_file():
            if path.name.endswith('.tar.gz'):
                with tarfile.open(path) as archive:
                    for member in archive:
                        label = f'{name}:{member.name}'
                        if member.isfile():
                            yield label, archive.extractfile(member).read(), path_findings(member.name)
                        elif member.isdir():
                            yield label, b'', path_findings(member.name)
                        elif not member.isdir():
                            yield label, b'', ['nonregular archive member']
            else:
                yield name, path.read_bytes(), []


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--history', action='store_true', help='Inspect all reachable commits and blobs instead of the working tree')
    parser.add_argument('--dist', type=Path, help='Also inspect release binaries and every source archive member')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    streams = [git_objects(root, args.history)]
    if args.dist:
        if not args.dist.is_dir(): parser.error('Release directory does not exist')
        streams.append(release_files(args.dist))
    count = 0; failed = False
    for stream in streams:
        for name, data, extra in stream:
            count += 1
            findings = sorted(set(check_file(name, data) + extra))
            if findings:
                failed = True
                print(f'{name}: {", ".join(findings)}')
    if failed:
        raise SystemExit(1)
    print(f'PASS: {count} files/objects checked; no configured privacy patterns found')


if __name__ == '__main__':
    main()
