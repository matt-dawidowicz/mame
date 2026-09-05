from pathlib import Path

path = Path("src/mame/philips/cdiaudio.h")
text = path.read_text(encoding="utf-8")
old = '''inline double nominal_attenuation_gain(uint8_t value)
{
\treturn attenuation_muted(value)
\t\t? 0.0
\t\t: std::pow(10.0, -double(attenuation_decibels(value)) / 20.0);
}
'''
new = '''inline double nominal_attenuation_gain(uint8_t value)
{
\tif (attenuation_muted(value))
\t\treturn 0.0;

\t// The register exposes only 128 nominal dB values.  Build the exact same
\t// Green Book curve once, then use indexed lookups in steady-state audio.
\tstatic const std::array<double, 128> gains = []
\t{
\t\tstd::array<double, 128> result{};
\t\tfor (unsigned db = 0; db < result.size(); ++db)
\t\t\tresult[db] = std::pow(10.0, -double(db) / 20.0);
\t\treturn result;
\t}();
\treturn gains[attenuation_decibels(value)];
}
'''
count = text.count(old)
if count != 1:
    raise SystemExit(f"nominal attenuation gain function: expected exactly one match, got {count}")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
