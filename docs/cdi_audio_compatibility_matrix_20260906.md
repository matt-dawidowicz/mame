# CD-i Audio Compatibility Matrix — 2026-09-06

## Purpose

This matrix is the title-facing companion to `docs/cdi_audio_fidelity_campaign.md`.
It distinguishes three things that must not be conflated:

1. media properties that are actually recorded in MAME's CD-i software list;
2. subsystem behavior that is certified by deterministic regression tests; and
3. end-to-end title behavior that has been observed in a retained title run.

A title is not marked compatible merely because the relevant low-level tests are
green.  Conversely, absence of a retained title run is not treated as evidence that
the title is broken.

## Certified implementation baseline

- Branch: `audio/cdi-fidelity-100-campaign-20260905`
- Code/test baseline: `0416e3067975236d3ca4a90c0cb461d3a153398d`
- CD-i fast CI run: `34055417260`
- Helper gate: 16,862,832 assertions in 187 cases — all passed.
- Full-machine integration gate: 21 assertions in 6 cases — all passed.

The integration gate includes the live SCC68070 DMA-to-DVC Layer II path and the
simultaneous DVC audio/video save-load continuation fixture.  These results certify
the implementation boundary exercised by the tests; they do not by themselves
certify every retail title or undocumented analogue/silicon edge.

## Status vocabulary

- **Media-confirmed** — the repository's software-list metadata explicitly records
  the relevant media property, such as DVC compatibility or CD-Audio tracks.
- **Model-certified** — the corresponding MAME implementation path is covered by
  maintained deterministic tests at the current certified baseline.
- **Title-runtime certified** — a retained end-to-end title run demonstrates the
  stated behavior on this campaign branch.
- **Not title-certified** — no retained title run supporting the claim is present
  in the campaign evidence ledger.
- **Evidence blocked** — the remaining distinction depends on physical hardware,
  private firmware behavior, or reference-media observations that are not currently
  available.

## Representative title matrix

| Title / software-list entry | Repository media evidence | XA / CDIC | CD-DA | DVC / MPEG | Current title-facing result |
| --- | --- | --- | --- | --- | --- |
| **The 7th Guest (Europe)** — `7thguest` | The game disc is explicitly tagged `DVC`; the second disc is a seven-track CD-Audio music disc.  The software-list entry itself is marked `supported="no"`. | No XA title claim is made from the software-list metadata alone. | **Media-confirmed.** The generic CD-DA model has certified 75 Hz delivery, 588 stereo frames/sector, pre-start gating, ADR/control preservation, and emphasis routing. Exact play/pause/seek, track/index, lead and sample-alignment behavior remains outside title certification. | **Media-confirmed + model-certified.** Parser, Layer II decode, FMA stream selection, live SCC DMA ingress, save/load reconstruction and software A/V clock arithmetic are regression-certified. | **Not title-runtime certified.** This row is a high-value DVC/CD-DA target, but the matrix does not override the software-list `supported="no"` state or invent a successful playthrough. |
| **BURN-CYCLE - The Music (Europe)** — `burncycm` | The software list records eleven CD-Audio tracks and describes the entry as `[CD-Audio]`. | N/A for this dedicated audio disc. | **Media-confirmed + model-certified at the sector/output boundary.** It is an appropriate reference target for multi-track CD-DA transport, Q-position and track-boundary work. | N/A. | **Not title-runtime certified.** Multi-track transport and subcode remain the relevant unclosed title-facing gates. |
| **Hotel Mario (USA)** — `hotmariou` | The software list records a normal single CD-i data disc and does not tag it DVC or expose separate CD-Audio tracks. | **Candidate only.** The repository metadata does not prove that this title's in-disc audio path is XA ADPCM; a media audit or retained runtime trace is required before calling it an XA representative. | No separate CD-Audio evidence in the software-list entry. | No DVC tag in the software-list entry. | **Baseline non-DVC title candidate; not an XA certification.** It is retained here specifically to prevent a single-track data-disc assumption from being silently promoted to an XA fact. |
| **Link - The Faces of Evil (USA)** — `linkfoeu` | The software list records a single CD-i data disc and does not tag it DVC or expose separate CD-Audio tracks. | **Candidate only.** As with Hotel Mario, a disc-content audit is required before assigning an XA title role. | No separate CD-Audio evidence in the software-list entry. | No DVC tag in the software-list entry. | **Baseline non-DVC title candidate; not an XA certification.** |

## XA implementation coverage independent of a retail-title claim

The absence of a media-audited XA retail-title row does **not** mean XA is
unvalidated.  The campaign's XA implementation gate is substantially stronger than
an ordinary smoke test:

- exhaustive Mode-2 file/channel/submode/filter routing;
- all supported and unsupported coding combinations;
- malformed duplicated-subheader and sound-group signaling;
- predictor/filter/range coverage;
- 4-bit and 8-bit, mono and stereo, 18.9 and 37.8 kHz paths;
- an independently corroborated two-sector 4-bit stereo PCM reference against
  FFmpeg's fixed-point XA decoder;
- hardware-captured alternating `$2800`/`$3200` CDIC audio-buffer behavior;
- 30-minute sample/sector arithmetic-drift coverage.

Those tests certify the XA/CDIC implementation model.  What is missing for the
**title compatibility matrix** is a retained disc-level audit tying a particular
retail title to the XA path and then exercising that title end-to-end.  Until such
an audit exists, the matrix deliberately says "candidate" rather than "passed XA
compatibility".

## DVC implementation coverage

The maintained DVC suite covers the software-visible mechanics required by a Full
Motion title:

- legal and malformed MPEG-1 Layer II headers;
- all 32 FMA audio stream IDs without `Cx`/`Dx` aliasing;
- direct MPEG-audio access points and program-stream routing;
- independent Layer II PCM reference frames;
- exact FMA attenuation table and matrix behavior;
- starvation/refill and ISO termination;
- requested/current stream switching;
- live SCC68070 channel-2 DMA transfer through FMA ingress;
- program-end, stream-switch and simultaneous audio/video save-load reconstruction;
- 30-minute software clock-domain drift bounds.

The remaining DVC title-facing uncertainty is not a generic parser/DMA/save-state
hole.  It is concentrated in physical DAC transition behavior, prohibited in-stream
rate-change response, exact Philips DSP arithmetic attribution, physical long-run
clock behavior, and a retained real branching-FMV title/fixture.

## CD-DA implementation coverage

The campaign already certifies the following generic CD-DA mechanics:

- CDIC command classification for CD-DA reads;
- CD-DA PCM bypass of CDIC RAM;
- 588 stereo frames per 1/75-second sector (`588 * 75 = 44,100`);
- first pre-start sector retention and AUDCTL playback gating;
- per-sector buffer/subcode cadence;
- Q ADR/control preservation and pre-emphasis routing;
- the known CDIC Q-storage offset used by the current model.

The compatibility matrix therefore treats a dedicated CD-Audio disc as a valid
media target, but does not certify play/pause/resume/stop/seek latency, exact
track/index transitions, lead-in/lead-out, multi-session handling, or synthesized
Q-position packets without reference-disc evidence.

## Minimum future title-runtime suite

A title-runtime certification pass should retain enough evidence that another
researcher can reproduce the result.  At minimum:

1. **XA/CDIC title:** first audit the disc to prove that the chosen scene actually
   uses XA ADPCM, then retain a scene covering sustained playback, a channel/filter
   change, stop/restart, and save/load.
2. **DVC title:** use a software-list DVC title such as The 7th Guest and retain a
   sequence containing sustained MPEG audio/video, at least one stream or branch
   transition, and a save/load continuation point.
3. **CD-DA title/disc:** use a multi-track disc such as BURN-CYCLE - The Music and
   retain track starts, a track transition, pause/resume, stop/restart, and Q-position
   observations.
4. Record the exact software-list shortname, CHD/disc SHA-1, machine configuration,
   campaign commit, command line, and any required input sequence.

## Compatibility conclusion

At `0416e3067975236d3ca4a90c0cb461d3a153398d`, the XA/CDIC and DVC implementation
models are strongly regression-certified and the generic CD-DA sector/output model
is covered.  The repository does **not** contain enough retained retail-title runtime
evidence to label the representative titles above as end-to-end compatibility
passes.  This matrix makes that limitation explicit rather than converting subsystem
unit/integration success into unsupported title claims.
