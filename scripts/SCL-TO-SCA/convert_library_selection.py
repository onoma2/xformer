import os
import subprocess
import sys

# Configuration
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CONVERTER_SCRIPT = os.path.join(SCRIPT_DIR, "scl_to_sca.py")
MAX_NOTES = 32

def parse_scl_header(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            lines = [l.strip() for l in f if l.strip() and not l.strip().startswith('!')]
            if len(lines) >= 2:
                desc = lines[0]
                try:
                    count = int(lines[1])
                    return desc, count
                except ValueError:
                    pass
    except:
        pass
    return None, None

def next_available_slot(base_dir):
    existing = set()
    for name in os.listdir(base_dir):
        if name.upper().endswith(".SCA") and len(name) == 7 and name[:3].isdigit():
            existing.add(int(name[:3]))
    slot = 1
    while slot in existing:
        slot += 1
    return slot

def iter_scl_files(base_dir):
    for name in sorted(os.listdir(base_dir)):
        if name.lower().endswith(".scl"):
            yield os.path.join(base_dir, name)

def slot_filename(slot):
    return f"{slot:03d}.SCA"

print(f"Scanning {SCRIPT_DIR} for .scl files...")

scl_files = list(iter_scl_files(SCRIPT_DIR))
print(f"Found {len(scl_files)} .scl files")

slot = next_available_slot(SCRIPT_DIR)
converted = 0

for path in scl_files:
    desc, count = parse_scl_header(path)
    if count is None or count > MAX_NOTES or count <= 4:
        continue

    out_file = os.path.join(SCRIPT_DIR, slot_filename(slot))
    print(f"  {os.path.basename(path)} -> {os.path.basename(out_file)}")
    subprocess.run([sys.executable, CONVERTER_SCRIPT, path, out_file])
    converted += 1
    slot += 1

print(f"Done. Converted {converted} file(s).")
