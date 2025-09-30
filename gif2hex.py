#!/usr/bin/env python3
import argparse, os, textwrap

def format_hex_array(data: bytes, per_line: int) -> str:
    chunks = []
    for i in range(0, len(data), per_line):
        line = ", ".join(f"0x{b:02x}" for b in data[i:i+per_line])
        chunks.append("    " + line)
    return ",\n".join(chunks)

def main():
    p = argparse.ArgumentParser(
        description="Convert a file (e.g., GIF) to a hex byte array."
    )
    p.add_argument("input", help="Path to input file (e.g., image.gif)")
    p.add_argument("-n", "--name", default="gif_data",
                   help="C variable name (default: gif_data)")
    p.add_argument("-w", "--per-line", type=int, default=16,
                   help="Bytes per line (default: 16)")
    p.add_argument("--no-c-wrap", action="store_true",
                   help="Print only comma-separated hex bytes without C declaration")
    p.add_argument("-u", "--unsigned", action="store_true",
                   help="Use 'unsigned char' instead of 'uint8_t'")
    args = p.parse_args()

    with open(args.input, "rb") as f:
        data = f.read()

    hex_body = format_hex_array(data, args.per_line)

    if args.no_c_wrap:
        print(hex_body)
        return

    typename = "unsigned char" if args.unsigned else "const unsigned char"
    size_comment = f"// source: {os.path.basename(args.input)}, {len(data)} bytes"
    decl = textwrap.dedent(f"""\
        {size_comment}
        {typename} {args.name}[] = {{
        {hex_body}
        }};
        const unsigned int {args.name}_len = {len(data)};
    """)
    print(decl)

if __name__ == "__main__":
    main()
