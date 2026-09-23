#!/usr/bin/env python3
# license:BSD-3-Clause
# copyright-holders:Matt Jordan
"""Extract immutable CD-i DVC certification corpus into runtime fixture files."""
import base64
import hashlib
import json
import pathlib
import re
import shutil

root = pathlib.Path(__file__).resolve().parents[1]
source = root / 'tests/emu/philips/fixtures/source'
dest_root = root / 'tests/emu/philips/fixtures'

hex_decl = re.compile(
    r'// SHA-256 ([0-9a-f]{64})\s+constexpr std::array<uint8_t,\s*(\d+)>\s+(\w+)\s*\{\{(.*?)\}\};',
    re.S)
base64_decl = re.compile(
    r'// SHA-256 ([0-9a-f]{64})\s+constexpr char\s+(\w+)\[\]\s*=\s*(.*?);',
    re.S)

def filename(name):
    return (name[:-2] + '.z') if name.endswith('_Z') else (name + '.bin')

def write_asset(directory, name, data, expected_hash):
    actual = hashlib.sha256(data).hexdigest()
    if actual != expected_hash:
        raise RuntimeError(f'{name}: SHA-256 mismatch {actual} != {expected_hash}')
    path = directory / filename(name)
    path.write_bytes(data)
    return {'bytes': len(data), 'sha256': actual}

def extract_hex(source_name, output_name):
    text = (source / source_name).read_text()
    directory = dest_root / output_name
    shutil.rmtree(directory, ignore_errors=True)
    directory.mkdir(parents=True)
    manifest = {'source': source_name, 'assets': {}}
    matches = list(hex_decl.finditer(text))
    if not matches:
        raise RuntimeError(f'no byte arrays found in {source_name}')
    for match in matches:
        expected_hash, declared_size, name, body = match.groups()
        data = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-fA-F]{2})', body))
        if len(data) != int(declared_size):
            raise RuntimeError(f'{name}: size mismatch {len(data)} != {declared_size}')
        manifest['assets'][filename(name)] = write_asset(directory, name, data, expected_hash)
    (directory / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')

def extract_base64(source_name, output_name):
    text = (source / source_name).read_text()
    directory = dest_root / output_name
    shutil.rmtree(directory, ignore_errors=True)
    directory.mkdir(parents=True)
    manifest = {'source': source_name, 'assets': {}}
    matches = list(base64_decl.finditer(text))
    if not matches:
        raise RuntimeError(f'no base64 arrays found in {source_name}')
    for match in matches:
        expected_hash, name, body = match.groups()
        encoded = ''.join(re.findall(r'"([^"]*)"', body))
        data = base64.b64decode(encoded, validate=True)
        manifest['assets'][filename(name)] = write_asset(directory, name, data, expected_hash)
    (directory / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')

extract_hex('cdi_dvc_av_reference_data.txt', 'av')
extract_hex('cdi_dvc_motion_reference_data.txt', 'motion')
extract_base64('cdi_dvc_full_reference_data.txt', 'full')
print(dest_root)
