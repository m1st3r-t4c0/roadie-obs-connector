"""Verify Windows can resolve a plugin DLL against one isolated OBS distribution.
This does not call obs_module_load or create an OBS/Qt application.
Run each host in a separate process to keep different Qt runtimes isolated.
"""
import argparse
import ctypes
import json
import os
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--dll', required=True, type=Path)
parser.add_argument('--obs-root', required=True, type=Path)
parser.add_argument('--expected-minimum', default='30.0.0')
parser.add_argument('--expect-load-failure', action='store_true')
args = parser.parse_args()
if os.name != 'nt':
    parser.error('This probe requires Windows.')
ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x8000)
with os.add_dll_directory(str((args.obs_root / 'bin/64bit').resolve())):
    try:
        plugin = ctypes.WinDLL(str(args.dll.resolve()), winmode=0x1100)
    except OSError as error:
        print(json.dumps({'dependencyLoadPassed': False, 'winerror': error.winerror,
                          'expectedFailure': args.expect_load_failure, 'obsHostTested': False}))
        raise SystemExit(0 if args.expect_load_failure else 1)
    if args.expect_load_failure:
        raise SystemExit('The negative fixture unexpectedly loaded.')
    plugin.obs_module_ver.argtypes = []
    plugin.obs_module_ver.restype = ctypes.c_uint32
    version = plugin.obs_module_ver()
    minimum = f'{version >> 24}.{(version >> 16) & 255}.{version & 65535}'
    if minimum != args.expected_minimum:
        raise SystemExit(f'Unexpected module minimum: {minimum}')
    print(json.dumps({'dependencyLoadPassed': True, 'moduleMinimum': minimum,
                      'obsHostTested': False}))
