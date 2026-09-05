# CD-i code-audit addendum — MCD212 control-stream bounds

Date: 2026-09-05

## Confirmed defect

`mcd212_device::process_ica()` and `mcd212_device::process_dca()` fetch 32-bit control commands as two adjacent host `uint16_t` array reads. Their local word address can reach the final word of the 512 KiB plane RAM, and a NOP/control chain can also advance through the top of the plane. Unlike the VSR pixel path, the control-stream fetch currently does not mask/wrap each host array index.

Consequences:

- an ICA reload to byte address `0x07ffff` produces word address `0x3ffff`; the second half of the next command would read word `0x40000`, one word past the plane array;
- a DCA beginning at the final aligned command pair (`0x07fffc`) can fetch that pair safely, but a continuing command immediately advances the next fetch beyond the host array;
- this is a host memory-safety defect, independent of the still-open question of exact control-fetch slot timing.

The existing VSR data path already masks every byte fetch with `0x0007ffff`, establishing the current emulator's 512 KiB plane-address wrap model. The control path must apply the equivalent 18-bit word index before every host array access.

## Oracle added

`src/mame/philips/mcd212_control_stream.h` defines:

- 512 KiB plane / 0x40000-word bounds;
- byte-to-word address normalization;
- wrapped word advancement;
- safe two-word command-pair indices.

`tests/emu/philips/mcd212_control_stream.cpp` exhausts all 0x40000 word starting positions and explicitly checks the top-of-plane wrap cases.

The source hunk remains open until it can be applied narrowly without replacing unrelated `mcd212.cpp` text. The intended source behavior is to wrap the word index after every ICA/DCA word fetch and when skipping unused DCA command-window words. No timing, opcode, or command semantic change is required.
