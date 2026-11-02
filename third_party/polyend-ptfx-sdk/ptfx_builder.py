#!/usr/bin/env python3
"""
Polyend Tracker Modular FX (.ptfx) Plugin Builder
Compiles position-independent ARM Cortex-M4F C plugins and packages them
into .ptfx files with a 344-byte PtfxHeader.
"""

import argparse
import os
import re
import struct
import subprocess
import sys
import tempfile

PTFX_MAGIC = 0x58465450  # "PTFX" in little endian
PTFX_VERSION = 1
HEADER_SIZE = 472
PARAM_DEF_SIZE = 32
MAX_PARAMS = 12

FLAG_MAP = {
    "EFFECT": 0x0000,
    "GENERATOR": 0x0001,
    "STEREO": 0x0002,
    "WAVETABLE": 0x0004,
    "CONVERT_TO_MONO": 0x0008,
    "IMX_ONLY": 0x0010,
}

LINKER_SCRIPT = """
SECTIONS
{
    . = 0;
    .text : {
        *(.text*)
        *(.rodata*)
    }
    . = ALIGN(4);
    .data : {
        *(.data*)
    }
    . = ALIGN(4);
    __ptfx_strings_start = .;
    .ptfx_strings : {
        *(.ptfx_strings*)
    }
    . = ALIGN(4);
    __bss_start = .;
    .bss : {
        *(.bss*)
        *(COMMON)
    }
    __bss_end = .;
}
"""

def find_toolchain(prefix=""):
    if prefix:
        return prefix
    candidates = [
        os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "agt", "10.3", "bin", "arm-none-eabi-")),
        "arm-none-eabi-",
    ]
    for c in candidates:
        try:
            res = subprocess.run([c + "gcc", "--version"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if res.returncode == 0:
                return c
        except FileNotFoundError:
            continue
    return "arm-none-eabi-"

def parse_comments(source_code):
    meta = {}
    for line in source_code.splitlines():
        line = line.strip()
        m = re.match(r"^//\s*PTFX_(\w+)\s*:\s*(.*)$", line)
        if m:
            meta[m.group(1).upper()] = m.group(2).strip()
    return meta

def parse_params_from_source(source_code):
    params = []
    # Match PLUGIN_PARAMS array definition
    match = re.search(r"PluginParam\s+PLUGIN_PARAMS\s*\[\s*\d*\s*\]\s*=\s*\{(.*?)\};", source_code, re.DOTALL)
    if not match:
        return params

    block = match.group(1)
    # Remove single-line comments inside PLUGIN_PARAMS block
    block_clean = re.sub(r"//[^\n]*", "", block)

    # Tokenize entries: either PTFX_PARAM_*(...) macros or { ... } brace initializers
    entry_pattern = re.compile(
        r"(PTFX_PARAM_LIST\s*\(\s*\"([^\"]*)\"\s*,\s*([^,]+),\s*\"([^\"]*)\"\s*\))"
        r"|(PTFX_PARAM_(FLOAT|INT)\s*\(\s*\"([^\"]*)\"\s*,\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*\"([^\"]*)\"\s*\))"
        r"|(\{\s*\"([^\"]*)\"\s*,\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*\"([^\"]*)\"\s*,\s*(\w+)\s*\})"
    )

    for m in entry_pattern.finditer(block_clean):
        if m.group(1):
            # PTFX_PARAM_LIST(label, def_idx, options_pipe_sep)
            label = m.group(2)
            def_val = m.group(3)
            raw_opts = m.group(4)
            opts = [o.strip()[:10] for o in raw_opts.split("|") if o.strip()][:16]
            if not opts:
                opts = ["Default"]
            try:
                p_def = float(def_val.strip().rstrip("f"))
            except ValueError:
                p_def = 0.0
            p_def = float(min(max(0, int(round(p_def))), len(opts) - 1))
            params.append({
                "label": label[:7],
                "min": 0.0,
                "max": float(len(opts) - 1),
                "default": p_def,
                "step": 1.0,
                "unit": "",
                "type": 'l',
                "options": opts
            })
        elif m.group(5):
            # PTFX_PARAM_FLOAT / PTFX_PARAM_INT
            kind = m.group(6)
            label = m.group(7)
            min_val, max_val, def_val, step, unit = m.group(8), m.group(9), m.group(10), m.group(11), m.group(12)
            try:
                p_min = float(min_val.strip().rstrip("f"))
                p_max = float(max_val.strip().rstrip("f"))
                p_def = float(def_val.strip().rstrip("f"))
                p_step = float(step.strip().rstrip("f"))
                p_type = 'd' if "INT" in kind else 'f'
                params.append({
                    "label": label[:7],
                    "min": p_min,
                    "max": p_max,
                    "default": p_def,
                    "step": p_step,
                    "unit": unit[:5],
                    "type": p_type,
                    "options": []
                })
            except ValueError:
                continue
        elif m.group(13):
            # Brace initializer: { "Label", min, max, default, step, "unit", PARAM_TYPE }
            label = m.group(14)
            min_val, max_val, def_val, step, unit, ptype = m.group(15), m.group(16), m.group(17), m.group(18), m.group(19), m.group(20)
            try:
                p_min = float(min_val.strip().rstrip("f"))
                p_max = float(max_val.strip().rstrip("f"))
                p_def = float(def_val.strip().rstrip("f"))
                p_step = float(step.strip().rstrip("f"))
                if "LIST" in ptype:
                    opts = [o.strip()[:10] for o in unit.split("|") if o.strip()][:16]
                    if not opts:
                        opts = ["Default"]
                    p_def = float(min(max(0, int(round(p_def))), len(opts) - 1))
                    params.append({
                        "label": label[:7],
                        "min": 0.0,
                        "max": float(len(opts) - 1),
                        "default": p_def,
                        "step": 1.0,
                        "unit": "",
                        "type": 'l',
                        "options": opts
                    })
                else:
                    p_type = 'd' if "INT" in ptype else 'f'
                    params.append({
                        "label": label[:7],
                        "min": p_min,
                        "max": p_max,
                        "default": p_def,
                        "step": p_step,
                        "unit": unit[:5],
                        "type": p_type,
                        "options": []
                    })
            except ValueError:
                continue
    return params

def build_ptfx(c_file, output_file, name=None, author=None, flags_str=None, scratch=None, toolchain_prefix=""):
    with open(c_file, "r") as f:
        source = f.read()

    meta = parse_comments(source)

    display_name = name or meta.get("NAME") or os.path.splitext(os.path.basename(c_file))[0]

    flags_raw = flags_str or meta.get("FLAGS") or "EFFECT"
    flags = 0
    for flag_part in flags_raw.replace("|", ",").split(","):
        flag_clean = flag_part.strip().upper()
        if flag_clean in FLAG_MAP:
            flags |= FLAG_MAP[flag_clean]

    scratch_size = int(scratch if scratch is not None else meta.get("SCRATCH", 0))

    params = parse_params_from_source(source)
    if len(params) > MAX_PARAMS:
        print(f"Warning: Only first {MAX_PARAMS} parameters will be used.")
        params = params[:MAX_PARAMS]

    # Build packed string table for PARAM_LIST parameters
    string_table = bytearray()
    for p in params:
        if p["type"] == 'l' and p.get("options"):
            p["rel_list_offset"] = len(string_table)
            p["item_count"] = len(p["options"])
            for opt in p["options"]:
                opt_bytes = opt.encode("ascii", errors="replace")[:10] + b'\0'
                string_table.extend(opt_bytes)
        else:
            p["rel_list_offset"] = 0
            p["item_count"] = 0

    toolchain = find_toolchain(toolchain_prefix)
    sdk_dir = os.path.dirname(os.path.abspath(__file__))

    with tempfile.TemporaryDirectory() as tmpdir:
        ld_path = os.path.join(tmpdir, "plugin.ld")
        with open(ld_path, "w") as f:
            f.write(LINKER_SCRIPT)

        obj_path = os.path.join(tmpdir, "plugin.o")
        strings_s_path = os.path.join(tmpdir, "strings.s")
        strings_o_path = os.path.join(tmpdir, "strings.o")
        elf_path = os.path.join(tmpdir, "plugin.elf")
        bin_path = os.path.join(tmpdir, "plugin.bin")

        gcc_cmd = [
            toolchain + "gcc",
            "-mcpu=cortex-m4",
            "-mthumb",
            "-mfloat-abi=hard",
            "-mfpu=fpv4-sp-d16",
            "-O3",
            "-fno-builtin",
            "-fno-tree-loop-distribute-patterns",
            "-fPIC",
            "-mpic-data-is-text-relative",
            "-mlong-calls",
            "-nostdlib",
            f"-I{sdk_dir}",
            "-c", c_file,
            "-o", obj_path
        ]
        res = subprocess.run(gcc_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if res.returncode != 0:
            print("Compilation Error:\n" + res.stderr.decode(), file=sys.stderr)
            sys.exit(1)

        ld_objs = [obj_path]
        if len(string_table) > 0:
            byte_list = ", ".join(f"0x{b:02x}" for b in string_table)
            with open(strings_s_path, "w") as f:
                f.write('.section .ptfx_strings,"a",%progbits\n')
                f.write('.balign 4\n')
                f.write(f'.byte {byte_list}\n')
            asm_cmd = [
                toolchain + "gcc",
                "-mcpu=cortex-m4",
                "-mthumb",
                "-c", strings_s_path,
                "-o", strings_o_path
            ]
            res = subprocess.run(asm_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if res.returncode != 0:
                print("String Table Assembly Error:\n" + res.stderr.decode(), file=sys.stderr)
                sys.exit(1)
            ld_objs.append(strings_o_path)

        ld_cmd = [
            toolchain + "ld",
            "-T", ld_path,
            "-e", "0",
            *ld_objs,
            "-o", elf_path
        ]
        res = subprocess.run(ld_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if res.returncode != 0:
            print("Link Error:\n" + res.stderr.decode(), file=sys.stderr)
            sys.exit(1)

        nm_cmd = [toolchain + "nm", elf_path]
        res = subprocess.run(nm_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if res.returncode != 0:
            print("NM Error:\n" + res.stderr.decode(), file=sys.stderr)
            sys.exit(1)

        symbols = {}
        for line in res.stdout.decode().splitlines():
            parts = line.split()
            if len(parts) >= 3:
                symbols[parts[2]] = int(parts[0], 16)

        init_offset = symbols.get("plugin_init", 0)
        set_param_offset = symbols.get("plugin_set_param", 0)
        process_block_offset = symbols.get("plugin_process_block", 0)

        strings_base = symbols.get("__ptfx_strings_start", 0)
        bss_start = symbols.get("__bss_start", 0)
        bss_end = symbols.get("__bss_end", 0)
        bss_size = bss_end - bss_start

        objcopy_cmd = [
            toolchain + "objcopy",
            "-O", "binary",
            elf_path,
            bin_path
        ]
        res = subprocess.run(objcopy_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if res.returncode != 0:
            print("Objcopy Error:\n" + res.stderr.decode(), file=sys.stderr)
            sys.exit(1)

        with open(bin_path, "rb") as f:
            code_bytes = f.read()

        code_size = len(code_bytes)

        # Build 344-byte Header
        name_bytes = display_name.encode("ascii", errors="replace")[:31].ljust(32, b'\0')
        author_bytes = b'\0' * 16  # Reserved / unused in compiled binary

        # Prefix: 88 bytes
        header_prefix = struct.pack(
            "<IH H 32s 16s B 3s I I I I I I I",
            PTFX_MAGIC,
            PTFX_VERSION,
            flags,
            name_bytes,
            author_bytes,
            len(params),
            b'\0\0\0',
            scratch_size,
            HEADER_SIZE,
            code_size,
            bss_size,
            init_offset,
            set_param_offset,
            process_block_offset
        )

        assert len(header_prefix) == 88, f"Header prefix length is {len(header_prefix)}, expected 88"

        # Parameters: 8 * 32 = 256 bytes
        param_bytes = bytearray()
        for i in range(MAX_PARAMS):
            if i < len(params):
                p = params[i]
                lbl = p["label"].encode("ascii", errors="replace")[:7].ljust(8, b'\0')
                if p["type"] == 'l':
                    abs_list_offset = strings_base + p["rel_list_offset"]
                    unit = struct.pack("<H B 3s", abs_list_offset, p["item_count"], b'\0\0\0')
                else:
                    unit = p["unit"].encode("ascii", errors="replace")[:5].ljust(6, b'\0')
                dtype = ord(p["type"])
                p_def = struct.pack(
                    "<8s f f f f 6s B B",
                    lbl,
                    p["min"],
                    p["max"],
                    p["default"],
                    p["step"],
                    unit,
                    dtype,
                    0
                )
            else:
                p_def = bytes(PARAM_DEF_SIZE)
            assert len(p_def) == PARAM_DEF_SIZE, f"Param def size is {len(p_def)}, expected 32"
            param_bytes.extend(p_def)

        full_header = header_prefix + bytes(param_bytes)
        assert len(full_header) == HEADER_SIZE, f"Full header size is {len(full_header)}, expected {HEADER_SIZE}"

        with open(output_file, "wb") as f:
            f.write(full_header)
            f.write(code_bytes)

        print(f"Successfully generated: {output_file}")
        print(f"  Display Name: {display_name}")
        print(f"  Flags:        0x{flags:04X} ({flags_raw})")
        print(f"  Parameters:   {len(params)}")
        print(f"  Code Size:    {code_size} bytes")
        print(f"  BSS Size:     {bss_size} bytes")
        print(f"  Scratch RAM:  {scratch_size} bytes")
        print(f"  Total Size:   {HEADER_SIZE + code_size} bytes")

def main():
    parser = argparse.ArgumentParser(description="Polyend Tracker .ptfx Plugin Builder")
    parser.add_argument("input_c", help="C source file for the plugin")
    parser.add_argument("-o", "--output", help="Output .ptfx file path")
    parser.add_argument("--name", help="Display name shown in Sample Editor")
    parser.add_argument("--author", help="Author handle or name (ignored during build, kept for source attribution)")
    parser.add_argument("--flags", help="Capabilities (e.g. EFFECT, STEREO, GENERATOR, CONVERT_TO_MONO)")
    parser.add_argument("--scratch", type=int, help="Scratch working RAM in bytes")
    parser.add_argument("--toolchain", default="", help="Prefix for arm-none-eabi- tools")
    args = parser.parse_args()

    out_file = args.output or os.path.splitext(args.input_c)[0] + ".ptfx"
    build_ptfx(args.input_c, out_file, args.name, None, args.flags, args.scratch, args.toolchain)

if __name__ == "__main__":
    main()
