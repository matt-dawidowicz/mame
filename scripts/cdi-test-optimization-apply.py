from pathlib import Path

path = Path("tests/emu/philips/cdi_audio_arithmetic.cpp")
text = path.read_text(encoding="utf-8")
old = '''\tfor (unsigned shift = 0; shift <= 15; ++shift)
\t{
\t\tfor (int32_t value = -32768; value <= 32767; ++value)
\t\t{
\t\t\tINFO("value=" << value << " shift=" << shift);
\t\t\tREQUIRE(cdic_hle::floor_shift_right(value, uint8_t(shift)) ==
\t\t\t\treference_floor_shift(value, uint8_t(shift)));
\t\t}
\t}
'''
new = '''\tbool domain_mismatch = false;
\tint32_t mismatch_value = 0;
\tunsigned mismatch_shift = 0;
\tint32_t mismatch_actual = 0;
\tint32_t mismatch_expected = 0;
\tfor (unsigned shift = 0; shift <= 15 && !domain_mismatch; ++shift)
\t{
\t\tfor (int32_t value = -32768; value <= 32767; ++value)
\t\t{
\t\t\tint32_t const actual = cdic_hle::floor_shift_right(value, uint8_t(shift));
\t\t\tint32_t const expected = reference_floor_shift(value, uint8_t(shift));
\t\t\tif (actual != expected)
\t\t\t{
\t\t\t\tdomain_mismatch = true;
\t\t\t\tmismatch_value = value;
\t\t\t\tmismatch_shift = shift;
\t\t\t\tmismatch_actual = actual;
\t\t\t\tmismatch_expected = expected;
\t\t\t\tbreak;
\t\t\t}
\t\t}
\t}
\tINFO("value=" << mismatch_value << " shift=" << mismatch_shift
\t\t<< " actual=" << mismatch_actual << " expected=" << mismatch_expected);
\tREQUIRE_FALSE(domain_mismatch);
'''
count = text.count(old)
if count != 1:
    raise SystemExit(f"XA exhaustive assertion loop: expected exactly one match, got {count}")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
