#!/usr/bin/env python3
# license:BSD-3-Clause
# copyright-holders:Matt Jordan

"""Generate original MPEG-1 video and independent FFmpeg RGB/PCM references."""
import argparse, hashlib, json, pathlib, subprocess, tempfile, zlib

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--root', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
args = parser.parse_args()
root = args.root
dest = root / 'tests/emu/philips/fixtures/av'\ndest.mkdir(parents=True, exist_ok=True)
assets = {}
version = subprocess.check_output(['ffmpeg', '-version'], text=True).splitlines()[0].split()[2]
commands = []
def ff(*command):
    commands.append(['ffmpeg', '-v', 'error', '-y', *command])
    subprocess.run(commands[-1], check=True)

with tempfile.TemporaryDirectory(prefix='cdi-original-av-') as work:
    work = pathlib.Path(work)
    for scene in range(2):
        raw = bytearray()
        for frame in range(64):
            # Four independently changing flat macroblocks, with a scene cut.
            # Every display-order frame has a distinct full-frame signature.
            for y in range(32):
                for x in range(32):
                    block = (y // 16) * 2 + x // 16
                    raw.append(32 + (frame * (block + 1) + block * 39 + scene * 73) % 180)
            raw.extend(bytes([96 + 16 * scene]) * 256)
            raw.extend(bytes([160 - 32 * scene]) * 256)
        inp, encoded, ref = (work / name for name in ['input.yuv', 'video.m1v', 'video.rgb'])
        inp.write_bytes(raw)
        ff('-f', 'rawvideo', '-pixel_format', 'yuv420p', '-video_size', '32x32', '-framerate', '25', '-i', str(inp),
           '-c:v', 'mpeg1video', '-threads', '1', '-g', '12', '-bf', '2', '-q:v', '2', '-sc_threshold', '1000000000', '-f', 'mpeg1video', str(encoded))
        ff('-i', str(encoded), '-sws_flags', 'neighbor+bitexact', '-pix_fmt', 'rgb24', '-f', 'rawvideo', str(ref))
        assert len(ref.read_bytes()) == 64 * 32 * 32 * 3
        types = subprocess.check_output(['ffprobe', '-v', 'error', '-select_streams', 'v', '-show_entries', 'frame=pict_type', '-of', 'csv=p=0', str(encoded)], text=True)
        assert all(t in types for t in ['I', 'P', 'B'])
        assets[f'VIDEO_{scene}'] = encoded.read_bytes() + b'\x00\x00\x01\xb7'
        assets[f'RGB_{scene}_Z'] = zlib.compress(ref.read_bytes(), 9)
    # Original changing stereo waveform. Integer phase/amplitude modulation
    # makes whole-frame repeats observable, unlike repetition of one MP2 frame.
    pcm = bytearray()
    phase = [0, 123456]
    for sample in range(98 * 1152):
        for channel in range(2):
            phase[channel] = (phase[channel] + 70000 + channel * 23000 + sample * (channel + 1) // 3) % (1 << 24)
            triangle = (phase[channel] >> 8)
            triangle = 32767 - abs(triangle - 32768) * 2
            amplitude = 5000 + ((sample // 1152) * 137 + channel * 2333) % 9000
            value = triangle * amplitude // 32768
            pcm.extend(value.to_bytes(2, 'little', signed=True))
    inp, encoded, ref = work / 'source.pcm', work / 'audio.mp2', work / 'audio.pcm'
    inp.write_bytes(pcm)
    ff('-f', 's16le', '-ar', '44100', '-ac', '2', '-i', str(inp),
       '-c:a', 'mp2', '-b:a', '192k', '-frames:a', '98', '-f', 'mp2', str(encoded))
    assets['AUDIO'] = encoded.read_bytes()
    ff('-c:a', 'mp2', '-i', str(encoded), '-c:a', 'pcm_s16le', '-f', 's16le', str(ref))
    assert len(ref.read_bytes()) == 98 * 1152 * 4
    assets['PCM_Z'] = zlib.compress(ref.read_bytes(), 9)

manifest = {'ffmpeg': version, 'assets': {}}
for name, data in assets.items():
    filename = (name[:-2] + '.z') if name.endswith('_Z') else (name + '.bin')
    (dest / filename).write_bytes(data)
    manifest['assets'][filename] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
(dest / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\\n')
print(json.dumps(manifest, indent=2))
