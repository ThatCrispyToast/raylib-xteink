#!/usr/bin/env python3
"""Exercise the public-file guard without embedding real secrets in fixtures."""
from pathlib import Path
import io
import struct
import tarfile
import tempfile
from public_audit import check_file, data_findings, release_files

assert not check_file('src/main.c', b'int main(void) { return 0; }')
assert not data_findings(b'Author <contributor@example.org> https://example.org 127.0.0.1')
for name in ('backups/flash.bin', '.env.local', 'src/identity.key', '../escape', '/absolute', 'x4-backup.bin', 'C:\\escape', 'bad\nname'):
    assert check_file(name, b''), name
for data in (
    b'/home/' + b'builder/project/main.c', b'C:\\Users\\' + b'builder\\main.c',
    b'aa:bb:' + b'cc:dd:ee:ff', b'192.' + b'168.1.2',
    b'-----BEGIN ' + b'PRIVATE KEY-----', b'ghp_' + b'x'*24,
    b'https://' + b'user:password@example.org',
):
    assert data_findings(data), data
png = b'\x89PNG\r\n\x1a\n' + struct.pack('>I', 3) + b'tEXt' + b'abc' + b'\0'*4
assert data_findings(png) == ['PNG text/EXIF metadata']
with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    (root/'firmware.bin').write_bytes(b'prefix\0' + b'/home/' + b'builder/secret\0')
    with tarfile.open(root/'source.tar.gz', 'w:gz') as archive:
        info = tarfile.TarInfo('project/.env'); info.size=3
        archive.addfile(info, io.BytesIO(b'key'))
        link = tarfile.TarInfo('project/link'); link.type=tarfile.SYMTYPE; link.linkname='../secret'
        archive.addfile(link)
        directory = tarfile.TarInfo('../escape'); directory.type=tarfile.DIRTYPE
        archive.addfile(directory)
    findings = {name: check_file(name, data)+extra for name, data, extra in release_files(root)}
    assert all(findings.values()) and len(findings)==4, findings
print('PASS: private paths, identifiers, credential patterns, PNG metadata, binary and archive leaks rejected')
