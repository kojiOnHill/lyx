# file fen2ascii.py
# This file is part of LyX, the document processor.
# Licence details can be found in the file COPYING.

# author Kayvan A. Sylvan

# Full author contact details are available in file CREDITS.

# This script will convert a chess position in the FEN
# format to an ascii representation of the position.

import sys

fout = open(sys.argv[2], "w")

placement = open(sys.argv[1]).readline().split()[0]
row_codes = placement.split("/")

cont = 1
MARGIN = " " * 6
RULE = f"+{'-' * 15}+"

print(f"{MARGIN}   {RULE}", file=fout)
for i in range(8):
    cont += 1
    row = ""
    for p in row_codes[i]:
        if "1" <= p <= "8":
            for k in range(int(p)):
                cont += 1
                row += "| " if (cont % 2) else "|*"
        else:
            row += "|" + p
            cont += 1

    print(f"{MARGIN} {8 - i} {row}|", file=fout)

print(f"{MARGIN}   {RULE}", file=fout)
print(f"{MARGIN}    a b c d e f g h ", file=fout)
