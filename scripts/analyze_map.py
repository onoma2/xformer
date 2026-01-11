#!/usr/bin/env python3
import argparse
import re
import subprocess
from pathlib import Path


SECTION_RE = re.compile(
    r"^\s+(\.(?:bss|data|ccmram_bss)(?:\.\S+)?)\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s*(.*)$"
)
HEADER_RE = re.compile(r"^\s+(\.(?:bss|data|ccmram_bss)\.\S+)\s*$")
DETAIL_RE = re.compile(r"^\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(.*)$")


def parse_map(path: Path):
    entries = []
    section_totals = {".bss": 0, ".data": 0, ".ccmram_bss": 0}
    with path.open("r", encoding="utf-8", errors="ignore") as handle:
        lines = iter(handle)
        for line in lines:
            match = SECTION_RE.match(line)
            if match:
                section, addr_hex, size_hex, rest = match.groups()
                if section in section_totals:
                    section_totals[section] += int(size_hex, 16)
                    continue
                if section.startswith(".*") or rest.startswith("*("):
                    continue
                size = int(size_hex, 16)
                if size == 0:
                    continue
                entries.append({
                    "section": section,
                    "addr": int(addr_hex, 16),
                    "size": size,
                    "rest": rest.strip(),
                })
                continue

            header = HEADER_RE.match(line)
            if not header:
                continue
            section = header.group(1)
            try:
                detail_line = next(lines)
            except StopIteration:
                break
            detail = DETAIL_RE.match(detail_line)
            if not detail:
                continue
            addr_hex, size_hex, rest = detail.groups()
            size = int(size_hex, 16)
            if size == 0:
                continue
            entries.append({
                "section": section,
                "addr": int(addr_hex, 16),
                "size": size,
                "rest": rest.strip(),
            })
    return entries, section_totals


def format_size(size: int) -> str:
    if size < 1024:
        return f"{size} B"
    return f"{size / 1024:.1f} KB"


def summarize(entries, section_totals, section_prefixes, top_n):
    filtered = [e for e in entries if any(e["section"].startswith(p) for p in section_prefixes)]
    total = 0
    for section_name, size in section_totals.items():
        if section_name in section_prefixes:
            total += size
    if total == 0:
        total = sum(e["size"] for e in filtered)
    filtered.sort(key=lambda e: e["size"], reverse=True)
    return total, filtered[:top_n]


def parse_objdump_symbols(output: str, sections):
    symbols = []
    for line in output.splitlines():
        parts = line.split()
        if len(parts) < 6:
            continue
        section = parts[3]
        if section not in sections:
            continue
        try:
            size = int(parts[4], 16)
        except ValueError:
            continue
        name = " ".join(parts[5:])
        symbols.append({
            "section": section,
            "size": size,
            "name": name,
        })
    symbols.sort(key=lambda e: e["size"], reverse=True)
    return symbols


def run_objdump(elf: Path, sections):
    section_args = []
    for section in sections:
        section_args.extend(["--section", section])
    cmd = ["arm-none-eabi-objdump", "-t", *section_args, str(elf)]
    result = subprocess.run(cmd, capture_output=True, text=True, check=False)
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description="Analyze linker .map RAM usage.")
    parser.add_argument("map", type=Path, help="Path to .map file")
    parser.add_argument("--top", type=int, default=20, help="Top N entries")
    parser.add_argument("--ccm", action="store_true", help="Also list CCM symbols via objdump")
    parser.add_argument("--sram", action="store_true", help="Also list SRAM symbols via objdump")
    args = parser.parse_args()

    entries, section_totals = parse_map(args.map)

    sram_total, sram_top = summarize(entries, section_totals, [".bss", ".data"], args.top)
    ccm_total, ccm_top = summarize(entries, section_totals, [".ccmram_bss"], args.top)

    print("SRAM (.data + .bss)")
    print(f"Total: {sram_total} bytes ({format_size(sram_total)})")
    for e in sram_top:
        print(f"{e['size']:8d} {format_size(e['size']):>8} {e['section']:<40} {e['rest']}")

    print("\nCCM (.ccmram_bss)")
    print(f"Total: {ccm_total} bytes ({format_size(ccm_total)})")
    for e in ccm_top:
        print(f"{e['size']:8d} {format_size(e['size']):>8} {e['section']:<40} {e['rest']}")

    if args.ccm or args.sram:
        elf = args.map.parent / "src/apps/sequencer/sequencer"
        if not elf.exists():
            elf = args.map.with_suffix("")
        if elf.exists():
            if args.ccm:
                print("\nCCM Symbols (objdump)")
                output = run_objdump(elf, [".ccmram_bss"])
                symbols = parse_objdump_symbols(output, {".ccmram_bss"})
                for sym in symbols[:args.top]:
                    print(f"{sym['size']:8d} {format_size(sym['size']):>8} {sym['section']:<12} {sym['name']}")
            if args.sram:
                print("\nSRAM Symbols (objdump)")
                output = run_objdump(elf, [".bss", ".data"])
                symbols = parse_objdump_symbols(output, {".bss", ".data"})
                for sym in symbols[:args.top]:
                    print(f"{sym['size']:8d} {format_size(sym['size']):>8} {sym['section']:<12} {sym['name']}")
        else:
            print("\nSymbols (objdump): ELF not found")


if __name__ == "__main__":
    main()
