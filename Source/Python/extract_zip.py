"""
Extracts a .zip into your project folder.
- zip_path: absolute path to a .zip on disk
"""
import sys
import zipfile
from pathlib import Path

args = sys.argv
if len(args) != 3:
    raise ValueError("Invalid number of arguments")

if not args[1]:
    raise ValueError("No .zip path argument provided")

if not args[2]:
    raise ValueError("No destination path argument provided")

zp = Path(args[1]).expanduser().resolve()
if not zp.exists():
    raise ValueError(f"Not a valid path: {zp}")

output_dir = Path(args[2]).expanduser().resolve()
output_dir.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(zp, 'r') as z:
    z.extractall(output_dir)