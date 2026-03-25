#!/usr/bin/env python3
"""Convert a binary file to a C header with a static const unsigned char array."""
import sys
import os

def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <input_file> <symbol_name> <output_header>", file=sys.stderr)
        sys.exit(1)

    input_path = sys.argv[1]
    symbol = sys.argv[2]
    output_path = sys.argv[3]

    with open(input_path, "rb") as f:
        data = f.read()

    size = len(data)

    with open(output_path, "w") as out:
        out.write("#pragma once\n\n")
        out.write(f"// Embedded font: {os.path.basename(input_path)} ({size} bytes)\n")
        out.write(f"static const unsigned int {symbol}_size = {size};\n")
        out.write(f"static const unsigned char {symbol}_data[{size}] =\n{{\n")

        # Write 16 bytes per line
        for i in range(0, size, 16):
            chunk = data[i:i+16]
            line = ",".join(f"0x{b:02x}" for b in chunk)
            if i + 16 < size:
                line += ","
            out.write(f"    {line}\n")

        out.write("};\n")

    print(f"Wrote {output_path}: {symbol}_data[{size}] ({size} bytes)", file=sys.stderr)

if __name__ == "__main__":
    main()
