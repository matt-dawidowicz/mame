#!/usr/bin/env python3
# license:BSD-3-Clause
# copyright-holders:Matt Jordan
"""Generate original moving MPEG-1 scenes and independent FFmpeg references."""
import argparse
import base64
import hashlib
import json
import pathlib
import re
import subprocess
import struct
import tempfile
import zlib

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--root', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
parser.add_argument('--full-size', action='store_true', help='Generate full-size PAL/NTSC references in a separate header')
args = parser.parse_args()
root = args.root
assets = {}
profiles = ([(352, 288, 25, 1, 36), (352, 240, 30000, 1001, 45), (384, 288, 25, 1, 36)]
            if args.full_size else [(64, 48, 25, 1, 50), (80, 64, 30000, 1001, 60), (96, 48, 24000, 1001, 48)])
version = subprocess.check_output(['ffmpeg', '-version'], text=True).splitlines()[0].split()[2]
def ff(*args):
    subprocess.run(['ffmpeg', '-v', 'error', '-y', *map(str,args)], check=True)
with tempfile.TemporaryDirectory(prefix='cdi-original-motion-') as tmp:
    tmp = pathlib.Path(tmp)
    for scene, (width,height,num,den,count) in enumerate(profiles):
        raw = bytearray()
        for frame in range(count):
            for y in range(height):
                for x in range(width):
                    sx, sy = (x + frame * 2) % width, (y + frame) % height
                    value = 40 + ((sx//4) * 3 + (sy//4) * 5) % 140 if args.full_size else 40 + (sx * 2 + sy * 3) % 140
                    if ((sx//(20 if args.full_size else 5)) ^ (sy//(24 if args.full_size else 7))) & 1:
                        value += 12 if args.full_size else 25
                    if (x - frame * 3) % width < 13 and (y - frame) % height < 11: value = 210 - scene * 20
                    raw.append(value)
            for channel in range(2):
                for y in range(height//2):
                    for x in range(width//2):
                        raw.append(100 + (x + y + frame * (channel+1) + scene * 11 + channel * 27) % 60)
        inp, video, rgb = tmp/'input.yuv', tmp/'video.m1v', tmp/'video.rgb'
        inp.write_bytes(raw)
        ff('-f','rawvideo','-pixel_format','yuv420p','-video_size',f'{width}x{height}',
           '-framerate',f'{num}/{den}','-i',inp,'-c:v','mpeg1video','-threads','1',
           '-g','12','-bf','2','-flags','+cgop',
           *(['-b:v','1150k','-maxrate','1150k','-bufsize','327680'] if args.full_size else ['-q:v','3']),
           '-sc_threshold','1000000000','-f','mpeg1video',video)
        ff('-i',video,'-sws_flags','neighbor+bitexact','-pix_fmt','rgb24','-f','rawvideo',rgb)
        assert len(rgb.read_bytes()) == count*width*height*3
        types = json.loads(subprocess.check_output(['ffprobe','-v','error','-show_frames','-select_streams','v',
            '-show_entries','frame=pict_type','-of','json',str(video)],text=True))['frames']
        assert len(types) == count and set(t['pict_type'] for t in types) == {'I','P','B'}
        assets[f'VIDEO_{scene}'] = video.read_bytes() + b'\0\0\1\xb7'
        assets[f'RGB_{scene}_Z'] = zlib.compress(rgb.read_bytes(),9)
        assets[f'TYPES_{scene}'] = ''.join(t['pict_type'] for t in types).encode()
    old = (root/'tests/emu/philips/cdi_dvc_av_reference_data.h').read_text()
    body = old.split(' AUDIO {{',1)[1].split('}};',1)[0]
    audio = bytes(int(x,16) for x in re.findall(r'0x([0-9a-f]{2})',body))
    (tmp/'audio.mp2').write_bytes(audio*3)
    ff('-c:a','mp2','-i',tmp/'audio.mp2','-c:a','pcm_s16le','-f','s16le',tmp/'audio.pcm')
    pcm = (tmp/'audio.pcm').read_bytes()
    size = 98*1152*4
    assert len(pcm)==size*3
    second = struct.unpack('<'+str(size//2)+'h',pcm[size:2*size])
    third = struct.unpack('<'+str(size//2)+'h',pcm[2*size:])
    assert max(abs(a-b) for a,b in zip(second,third)) <= 1 # fixed decoder rounding state
    assets['PCM_STEADY_Z'] = zlib.compress(pcm[size:2*size],9)

if args.full_size:
    del assets['PCM_STEADY_Z'] # Shared unchanged audio reference lives in the small fixture.

namespace = 'cdi_full_reference' if args.full_size else 'cdi_motion_reference'
header = '// license:BSD-3-Clause\n// copyright-holders:Matt Jordan\n\n'
header += f'// Generated original moving video and FFmpeg {version} reference pixels/PCM.\n'
header += '// Regenerate with scripts/cdi_generate_motion_reference.py'+(' --full-size' if args.full_size else '')+'.\n#pragma once\n#include <array>\n#include <cstdint>\nnamespace '+namespace+'\n{\n'
if args.full_size: header += '// All byte arrays below are base64 encoded, excluding the terminating NUL.\n'
header += 'struct profile { unsigned width, height, rate_num, rate_den, frames; };\n'
header += 'constexpr profile PROFILES[] = { '+', '.join('{'+', '.join(map(str,p))+'}' for p in profiles)+' };\n'
for name,data in assets.items():
    if args.full_size:
        # Base64 keeps independently decoded full-frame references compact in source.
        encoded = base64.b64encode(data).decode('ascii')
        header += f'// SHA-256 {hashlib.sha256(data).hexdigest()}\nconstexpr char {name}[] =\n'
        header += ''.join('\t"'+encoded[i:i+120]+'"\n' for i in range(0,len(encoded),120))+';\n'
    else:
        header += f'// SHA-256 {hashlib.sha256(data).hexdigest()}\nconstexpr std::array<uint8_t, {len(data)}> {name} {{{{\n'
        header += ''.join('\t'+', '.join(f'0x{x:02x}' for x in data[i:i+16])+',\n' for i in range(0,len(data),16))+'}};\n'
header += '} // namespace '+namespace+'\n'
target = 'cdi_dvc_full_reference_data.h' if args.full_size else 'cdi_dvc_motion_reference_data.h'
(root/'tests/emu/philips'/target).write_text(header)
print(json.dumps({k:{'bytes':len(v),'sha256':hashlib.sha256(v).hexdigest()} for k,v in assets.items()},indent=2))
