#!/usr/bin/env python3
"""Verify offline dependency provenance; optionally report host linked versions.

This does not fetch dependencies, identify distribution backports, or certify
that a dependency is free of vulnerabilities. --runtime requires host headers.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def verify(root=ROOT):
    lock = json.loads((root / 'config/dependencies-lock.json').read_text())
    if lock.get('format') != 1:
        raise ValueError('unsupported dependency lock format')
    dependency = lock['nuklear']
    required = {'third_party/nuklear/nuklear.h',
                'third_party/nuklear/demo/sdl_renderer/nuklear_sdl_renderer.h',
                'third_party/nuklear/LICENSE'}
    if set(dependency['files']) != required:
        raise ValueError('incomplete Nuklear input/license inventory')
    for name, expected in dependency['files'].items():
        path = root / name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError('dependency checksum differs from lock: ' + name)
    header = (root / 'third_party/nuklear/nuklear.h').read_text()
    match = re.search(r'VERSION:\s*\*\s*v([0-9.]+)', header)
    if not match or match[1] != dependency['header_version']:
        raise ValueError('Nuklear header version differs from lock')
    if 'ALTERNATIVE A - MIT License' not in (root / 'third_party/nuklear/LICENSE').read_text():
        raise ValueError('Nuklear MIT alternative is missing')
    if not re.fullmatch('[0-9a-f]{40}', dependency['revision']):
        raise ValueError('invalid pinned Nuklear revision')
    # Source archives without Git still have exact file-level integrity checks.
    if (root / '.git').exists() and shutil.which('git'):
        result = subprocess.run(['git', '-C', str(root), 'ls-files', '--stage',
                                 'third_party/nuklear'], text=True, capture_output=True, check=True)
        fields = result.stdout.split()
        if len(fields) < 2 or fields[0] != '160000' or fields[1] != dependency['revision']:
            raise ValueError('Nuklear gitlink differs from dependency lock')
    return lock


def wal_reset_fixed_upstream(number):
    """Recognized upstream fixed branches; False says nothing about backports."""
    return (number >= 3051003 or 3050007 <= number < 3051000 or
            3044006 <= number < 3045000)


PROBE = r'''
#include <SDL.h>
#include <sqlite3.h>
#include <stdio.h>
int main(void)
{
    SDL_version runtime;
    SDL_GetVersion(&runtime);
    printf("sdl_header=%d.%d.%d\n", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
    printf("sdl_runtime=%d.%d.%d\n", runtime.major, runtime.minor, runtime.patch);
    printf("sqlite_header=%s\n", SQLITE_VERSION);
    printf("sqlite_runtime=%s\n", sqlite3_libversion());
    printf("sqlite_runtime_number=%d\n", sqlite3_libversion_number());
    printf("sqlite_source_id=%s\n", sqlite3_sourceid());
    printf("sqlite_threadsafe=%d\n", sqlite3_threadsafe());
    return 0;
}
'''


def runtime_report():
    def sdl_flags(option):
        return shlex.split(subprocess.check_output(['sdl2-config', option], text=True))
    with tempfile.TemporaryDirectory(prefix='wena-dependency-probe-') as directory:
        source = Path(directory) / 'probe.c'
        binary = Path(directory) / 'probe'
        source.write_text(PROBE)
        compiler = shlex.split(os.environ.get('CC', 'cc'))
        subprocess.run(compiler + ['-std=c89', '-pedantic-errors', '-Wall', '-Wextra', '-Werror'] +
                       sdl_flags('--cflags') + [str(source), '-o', str(binary)] +
                       sdl_flags('--libs') + ['-lsqlite3'], check=True)
        output = subprocess.check_output([str(binary)], text=True)
    report = dict(line.split('=', 1) for line in output.splitlines())
    report['sqlite_runtime_number'] = int(report['sqlite_runtime_number'])
    report['sqlite_threadsafe'] = int(report['sqlite_threadsafe'])
    report['sqlite_wal_reset_known_fixed_upstream_version'] = wal_reset_fixed_upstream(report['sqlite_runtime_number'])
    report['sqlite_distribution_backports'] = 'not inspected; verify distributor advisory and package revision'
    report['scope'] = 'current host probe, not the runtime of a deployed desktop package'
    report['sdl_backend_minimum'] = '2.0.22'
    for field in ('sdl_header', 'sdl_runtime'):
        parts = tuple(map(int, report[field].split('.')))
        if parts[0] != 2 or parts < (2, 0, 22):
            raise ValueError(field + ' does not satisfy the pinned SDL2 backend minimum')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', action='store_true')
    args = parser.parse_args()
    try:
        lock = verify()
        report = {'nuklear': lock['nuklear']}
        if args.runtime:
            report['host_runtime'] = runtime_report()
        print(json.dumps(report, indent=2))
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit('dependency check failed: ' + str(error)) from error


if __name__ == '__main__':
    main()
