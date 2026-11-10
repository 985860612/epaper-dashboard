# /// script
# requires-python = ">=3.11"
# ///
"""Build the minute-sleep firmware in the existing app partition; never uploads."""
from pathlib import Path
import argparse
import subprocess
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-path', type=Path, default=root / 'build/release')
args = parser.parse_args()
subprocess.run([
    'arduino-cli', 'compile', '--fqbn', 'esp32:esp32:esp32s3:CDCOnBoot=cdc',
    '--build-property', 'compiler.cpp.extra_flags=-flto',
    '--build-property', 'compiler.c.elf.extra_flags=-flto',
    '--build-path', str(args.build_path.resolve()), '--jobs', '0',
    str(root / 'firmware/epaper_dashboard'),
], check=True)
