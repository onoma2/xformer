#!/usr/bin/env python3
import sys
import struct
import math
import os
import argparse

# Configuration
PROJECT_VERSION_LATEST = 33
FILE_TYPE_USERSCALE = 1
FILE_HEADER_VERSION = 0
USER_SCALE_MODE_VOLTAGE = 1
DEFAULT_USER_SCALE_SIZE_MAX = 32

# FNV Hash Constants
FNV_PRIME = 0x1000193
FNV_OFFSET_BASIS = 0x811c9dc5

def fnv1a_hash(data):
    hash_val = FNV_OFFSET_BASIS
    for byte in data:
        hash_val ^= byte
        hash_val = (hash_val * FNV_PRIME) & 0xFFFFFFFF
    return hash_val

def parse_scl(filepath):
    lines = []
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('!'):
                continue
            lines.append(line)

    if len(lines) < 2:
        raise ValueError("Invalid SCL file: too short")

    description = lines[0]
    count = int(lines[1].split()[0])
    
    values = []
    values.append(0.0) # Root

    for i in range(2, 2 + count):
        if i >= len(lines): break
        part = lines[i].split()[0]
        if '.' in part:
            cents = float(part)
            volts = cents / 1200.0
        elif '/' in part:
            num, den = map(float, part.split('/'))
            ratio = num / den
            volts = math.log2(ratio)
        else:
            ratio = float(part)
            volts = math.log2(ratio)
        
        values.append(volts)

    return description, values

def create_sca(name, volts, max_size=DEFAULT_USER_SCALE_SIZE_MAX):
    # Truncate or pad name to 8 chars
    safe_name = name[:8].ljust(8, '\0')
    
    # Performer Limit
    if len(volts) > max_size:
        print(f"Warning: Scale too large ({len(volts)}), truncating to {max_size}")
        volts = volts[:max_size]
    
    size = len(volts)
    
    # Convert to millivolts (int16)
    items = [int(v * 1000.0) for v in volts]

    # --- Serialize Data Block ---
    data_block = bytearray()
    
    # Name (9 bytes: 8 chars + null)
    name_buf = name[:8].encode('ascii', 'ignore') + b'\0'
    name_buf = name_buf.ljust(9, b'\0')
    data_block.extend(name_buf)
    
    # Mode (1 byte)
    data_block.append(USER_SCALE_MODE_VOLTAGE)
    
    # Size (1 byte)
    data_block.append(size)

    # Items (size * 2 bytes)
    for item in items:
        data_block.extend(struct.pack('<h', item)) # Little endian int16

    # --- Calculate Hash ---
    data_hash = fnv1a_hash(data_block)

    # --- Construct Final File ---
    output = bytearray()
    
    # File Header (10 bytes)
    output.append(FILE_TYPE_USERSCALE)
    output.append(FILE_HEADER_VERSION)
    header_name = name[:8].encode('ascii', 'ignore').ljust(8, b'\0')
    output.extend(header_name)
    
    # Versioned Writer Header (4 bytes)
    output.extend(struct.pack('<I', PROJECT_VERSION_LATEST))
    
    # Data Block
    output.extend(data_block)
    
    # Hash (4 bytes)
    output.extend(struct.pack('<I', data_hash))
    
    return output

def main():
    parser = argparse.ArgumentParser(description='Convert SCL files to Performer .SCA format')
    parser.add_argument('input', help='Input .scl file')
    parser.add_argument('output', help='Output .SCA file')
    parser.add_argument('--name', help='Internal name (max 8 chars), defaults to filename')
    parser.add_argument('--max-size', type=int, default=DEFAULT_USER_SCALE_SIZE_MAX, help='Max scale size (defaults to 32)')
    
    args = parser.parse_args()
    
    if not args.name:
        args.name = os.path.splitext(os.path.basename(args.input))[0].upper()
    
    try:
        desc, volts = parse_scl(args.input)
        print(f"Parsed '{desc}': {len(volts)} notes")
        
        sca_data = create_sca(args.name, volts, args.max_size)
        
        with open(args.output, 'wb') as f:
            f.write(sca_data)
            
        print(f"Created {args.output} ({len(sca_data)} bytes)")
        
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
