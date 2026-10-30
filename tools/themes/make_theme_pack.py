#!/usr/bin/env python3
"""Semi fork theme pack: writes themes/<Name>.txt (2.2.3 theme files).

Each theme is a few hand-picked colours; the rest of the 42 roles is derived
from them (empty-cell tints, grid shades, labels, cursor) so every file sets
every role. Copy the files to the Themes folder of the card and choose them
under Config > General > Display theme.

    python3 -I tools/themes/make_theme_pack.py themes
"""
import os
import sys


def rgb(h):
    h = h.lstrip('#')
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def hexc(c):
    return '#%02X%02X%02X' % tuple(max(0, min(255, int(round(v)))) for v in c)


def mix(a, b, t):
    a, b = rgb(a), rgb(b)
    return hexc(tuple(a[i] + (b[i] - a[i]) * t for i in range(3)))


def lum(c):
    r, g, b = rgb(c)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


# name, base, story, colours (bg, panel, text, dim, accent, good, note, instr, fx1, fx2,
# alert, orange, cyan, yellow, wave, cursor, lines), optional explicit role overrides.
THEMES = [
    ("Iznik", "monochrome", "Iznik tiles: cobalt, turquoise and bole red on glazed white.",
     dict(bg="#F5F1E8", panel="#E9E3D4", text="#13234A", dim="#7A7F92", accent="#C0302A", good="#2E8B57",
          note="#1C3F94", instr="#138C8A", fx1="#C0302A", fx2="#3C7A3E", alert="#C0302A", orange="#C8661E",
          cyan="#138C8A", yellow="#B8860B", wave="#1C3F94", cursor="#1C3F94", lines="#D8D0BE")),
    ("Test Card", "original", "Broadcast colour bars: yellow, cyan, green, magenta on studio black.",
     dict(bg="#0B0B0B", panel="#141414", text="#EBEBEB", dim="#7F7F7F", accent="#EB1C1C", good="#14EB14",
          note="#EBEB14", instr="#14EBEB", fx1="#14EB14", fx2="#EB14EB", alert="#EB1C1C", orange="#EB8A14",
          cyan="#14EBEB", yellow="#EBEB14", wave="#EBEBEB", cursor="#1C1CEB", lines="#262626")),
    ("Acetate", "original", "A fresh dubplate: black lacquer, silver grooves, a red wax label.",
     dict(bg="#07080C", panel="#0F1117", text="#C9CED6", dim="#5D6470", accent="#D2232A", good="#E9DFC4",
          note="#DDE3EA", instr="#E9DFC4", fx1="#D2232A", fx2="#7FA7C9", alert="#FF3B30", orange="#D98E3A",
          cyan="#9FC2DE", yellow="#E9DFC4", wave="#AEB6C2", cursor="#2A2F3A", lines="#171A22")),
    ("Pirate FM", "original", "1994 pirate radio: amber LED dial over a navy night, rig lights green.",
     dict(bg="#040918", panel="#0A1228", text="#FFB45A", dim="#4E5C80", accent="#FF6A1F", good="#7CFC00",
          note="#FFB347", instr="#7CFC00", fx1="#FF3B3B", fx2="#41D1FF", alert="#FF3B3B", orange="#FF6A1F",
          cyan="#41D1FF", yellow="#FFD86B", wave="#FFB45A", cursor="#3A2400", lines="#121C38")),
    ("Oscilloscope", "original", "A phosphor trace on a lit graticule; the trigger level in yellow.",
     dict(bg="#020A05", panel="#06140B", text="#7CFFB2", dim="#2F7A52", accent="#E8FF5A", good="#7CFFB2",
          note="#5CFF9C", instr="#2EE6A8", fx1="#B6FFD9", fx2="#9CE65C", alert="#FF5A5A", orange="#E8C35A",
          cyan="#5AFFE0", yellow="#E8FF5A", wave="#C4FFE0", cursor="#0E3A22", lines="#0F3A22")),
    ("Risograph", "monochrome", "Riso print: fluorescent pink, blue and green inks on cream stock.",
     dict(bg="#F4EEDF", panel="#EAE2CE", text="#1F2A5C", dim="#8C8A9E", accent="#FF3FA4", good="#00A95C",
          note="#0078BF", instr="#FF3FA4", fx1="#00A95C", fx2="#E07B00", alert="#FF3FA4", orange="#E07B00",
          cyan="#0078BF", yellow="#E0A800", wave="#0078BF", cursor="#FF3FA4", lines="#DDD3BC")),
    ("Bauhaus", "monochrome", "Dessau 1925: primary red, blue and ochre with black rules on paper.",
     dict(bg="#EEEAE0", panel="#E1DCCF", text="#111111", dim="#7D7A72", accent="#D7261E", good="#1F4E9E",
          note="#1F4E9E", instr="#B8860B", fx1="#D7261E", fx2="#222222", alert="#D7261E", orange="#D9731A",
          cyan="#1F4E9E", yellow="#C99A00", wave="#111111", cursor="#111111", lines="#CFC9BA")),
    ("Bioluminescence", "original", "The deep sea: a dark abyss lit by cyan, violet and lure magenta.",
     dict(bg="#01060D", panel="#041222", text="#9FD8E8", dim="#3A6070", accent="#FF3DCB", good="#3DFFE0",
          note="#3DFFE0", instr="#8A6BFF", fx1="#FF3DCB", fx2="#2DE36A", alert="#FF3D6E", orange="#FF9A3D",
          cyan="#3DFFE0", yellow="#D8FF5A", wave="#5AE8FF", cursor="#0A2A44", lines="#06182B")),
    ("Hazard", "original", "Industrial: safety yellow and orange, signal blue, black rubber.",
     dict(bg="#0B0B0B", panel="#151515", text="#E6E6E6", dim="#7A7A7A", accent="#FFC400", good="#00A3E0",
          note="#FFC400", instr="#FF6A00", fx1="#00A3E0", fx2="#E6E6E6", alert="#FF2A00", orange="#FF6A00",
          cyan="#00A3E0", yellow="#FFC400", wave="#FFC400", cursor="#3A2E00", lines="#2A2A2A")),
    ("Mars Rover", "original", "Red-planet dust: rust, sand and oxide under a blue Martian sunset.",
     dict(bg="#120806", panel="#1E0F0B", text="#F1D2B6", dim="#7A5446", accent="#FF6A3D", good="#8FB6C9",
          note="#E89060", instr="#F5C26B", fx1="#C2553A", fx2="#8FB6C9", alert="#FF4A2A", orange="#FF6A3D",
          cyan="#8FB6C9", yellow="#F5C26B", wave="#F1D2B6", cursor="#3A1A10", lines="#2A1510")),
    ("Aurora", "original", "Northern lights: green and violet curtains over an arctic night.",
     dict(bg="#030B14", panel="#081624", text="#CDE7F0", dim="#4A6878", accent="#7CFF6B", good="#55F2A6",
          note="#55F2A6", instr="#B07CFF", fx1="#FF6BD6", fx2="#6BD3FF", alert="#FF6B8A", orange="#FFB36B",
          cyan="#6BD3FF", yellow="#E6FF6B", wave="#9CFFD0", cursor="#123A2E", lines="#0B1E2E")),
    ("Concrete", "monochrome", "Brutalist: raw grey concrete with a safety-orange warning.",
     dict(bg="#C9C5BE", panel="#BDB8B0", text="#1B1B1B", dim="#6E6A64", accent="#FF5A1F", good="#2B6E4F",
          note="#1B1B1B", instr="#4A4A4A", fx1="#FF5A1F", fx2="#2B5C8A", alert="#E03A10", orange="#FF5A1F",
          cyan="#2B5C8A", yellow="#B07A00", wave="#1B1B1B", cursor="#1B1B1B", lines="#B0ABA2")),
    ("Soundsystem", "original", "Speaker stack at a dance: red, gold and green on black.",
     dict(bg="#0A0A06", panel="#14140C", text="#EDE6C8", dim="#6E6A52", accent="#E2231A", good="#009B3A",
          note="#FFD100", instr="#00B347", fx1="#E2231A", fx2="#9AD0EC", alert="#FF3B30", orange="#FF8C1A",
          cyan="#9AD0EC", yellow="#FFD100", wave="#FFD100", cursor="#3A3000", lines="#1E1E14")),
]


def roles(c):
    light = lum(c["bg"]) > 128
    bg, panel, text = c["bg"], c["panel"], c["text"]
    toward = "#000000" if light else "#FFFFFF"
    r = {
        "background": bg,
        "title_bg": mix(bg, toward, 0.08 if light else 0.04),
        "title_text": text,
        "text": text,
        "inverse_text": bg,
        "inverse_bg": text,
        "dim": c["dim"],
        "scroll_thumb": mix(bg, text, 0.45),
        "scroll_track": mix(bg, toward, 0.05),
        "line2": mix(bg, toward, 0.02),
        "label_text": c["dim"],
        "label_text_off": mix(bg, c["dim"], 0.45),
        "label_bg": mix(panel, toward, 0.12),
        "strip": mix(bg, toward, 0.04),
        "panel": panel,
        "panel2": mix(panel, toward, 0.12),
        "accent": c["accent"],
        "alert": c["alert"],
        "good": c["good"],
        "yellow": c["yellow"],
        "orange": c["orange"],
        "cyan": c["cyan"],
        "note": c["note"],
        "instrument": c["instr"],
        "fx1": c["fx1"],
        "fx2": c["fx2"],
        # Empty-cell dashes: the column colour well into the background.
        "note_empty": mix(bg, c["note"], 0.30 if light else 0.25),
        "instrument_empty": mix(bg, c["instr"], 0.30 if light else 0.25),
        "fx1_empty": mix(bg, c["fx1"], 0.30 if light else 0.25),
        "fx2_empty": mix(bg, c["fx2"], 0.30 if light else 0.25),
        "grid_header": text if light else bg,
        "grid_bg": mix(bg, toward, 0.02),
        "grid_beat": mix(bg, toward, 0.06 if light else 0.05),
        "grid_muted": mix(bg, toward, 0.12 if light else 0.09),
        "grid_cursor_row": c["cursor"],
        "grid_cursor_text": bg if light else text,
        "grid_cursor_alt": mix(c["cursor"], c["accent"], 0.5),
        "grid_lines": c["lines"],
        "wave": c["wave"],
        "focus_fill": mix(bg, text, 0.25),
        "marker_grey": c["dim"],
        "marker_sel": mix(bg, text, 0.3),
    }
    return r


def write(out_dir, name, base, story, c):
    r = roles(c)
    lines = [f"# {name}: {story}",
             f"# Semi fork theme pack (tools/themes/make_theme_pack.py), built on the {base.capitalize()} theme.",
             "# Copy to the Themes folder of the card; Config > General > Display theme.",
             f"base = {base}", ""]
    for k, v in r.items():
        lines.append(f"{k} = {v}")
    path = os.path.join(out_dir, name + ".txt")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    return path


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "themes"
    os.makedirs(out, exist_ok=True)
    for name, base, story, c in THEMES:
        assert 1 <= len(name) <= 20, name
        print(write(out, name, base, story, c))


if __name__ == "__main__":
    main()
