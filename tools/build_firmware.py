# /// script
# requires-python = ">=3.11"
# ///
"""Build the single-application firmware using the project partition table; never uploads."""
from pathlib import Path
import argparse
import csv
import subprocess
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-path', type=Path, default=root / 'build/release')
args = parser.parse_args()
partition_file = root / 'firmware/epaper_dashboard/partitions.csv'
rows = [[v.strip() for v in row] for row in csv.reader(
    line for line in partition_file.read_text().splitlines() if line.strip() and not line.lstrip().startswith('#'))]
apps = [row for row in rows if row[1] == 'app']
assert len(apps) == 1 and apps[0][2] == 'factory', 'Expected a single factory app without OTA'
assert not any(row[1:3] == ['data', 'ota'] for row in rows), 'OTA data is not used'
app_size = int(apps[0][4], 0)
assert int(apps[0][3], 0) == 0x10000 and app_size == 0x280000
expected_data = {'nvs': (0x9000, 0x5000), 'spiffs': (0x290000, 0x160000), 'coredump': (0x3f0000, 0x10000)}
for name, expected in expected_data.items():
    row = next(row for row in rows if row[0] == name)
    assert (int(row[3], 0), int(row[4], 0)) == expected, f'{name} must retain its original location'

subprocess.run([
    'arduino-cli', 'compile', '--fqbn', 'esp32:esp32:esp32s3:CDCOnBoot=cdc',
    '--build-property', f'upload.maximum_size={app_size}',
    '--build-property', 'compiler.cpp.extra_flags=-flto',
    '--build-property', 'compiler.c.elf.extra_flags=-flto',
    '--build-path', str(args.build_path.resolve()), '--jobs', '0',
    str(root / 'firmware/epaper_dashboard'),
], check=True)

binary = args.build_path.resolve() / 'epaper_dashboard.ino.bin'
assert binary.stat().st_size <= app_size, 'Application image exceeds the factory partition'
assert (args.build_path.resolve() / 'partitions.csv').read_bytes() == partition_file.read_bytes()
print(f'App image: {binary.stat().st_size} / {app_size} bytes; {app_size - binary.stat().st_size} bytes free')
