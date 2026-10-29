from pathlib import Path
import subprocess
import sys

bundle = Path(sys.argv[1]).resolve()
frameworks = bundle / 'Contents/Frameworks'
executable_dir = bundle / 'Contents/MacOS'
magic_values = {bytes.fromhex(s) for s in ['cafebabe', 'cafebabf', 'feedface', 'feedfacf', 'cefaedfe', 'cffaedfe']}
count = 0
errors = []
for path in bundle.rglob('*'):
    if not path.is_file() or path.is_symlink():
        continue
    with path.open('rb') as f:
        if f.read(4) not in magic_values:
            continue
    count += 1
    identities = subprocess.check_output(['otool', '-D', str(path)], text=True)
    identities = {line.strip() for line in identities.splitlines() if line.strip() and not line.rstrip().endswith(':')}
    output = subprocess.check_output(['otool', '-L', str(path)], text=True)
    for line in output.splitlines()[1:]:
        dependency = line.strip().split(' (compatibility version', 1)[0]
        if not dependency or dependency.endswith(':') or dependency in identities:
            continue
        if dependency.startswith(('/System/', '/usr/lib/')):
            continue
        if dependency.startswith('@executable_path/'):
            target = executable_dir / dependency.removeprefix('@executable_path/')
        elif dependency.startswith('@loader_path/'):
            target = path.parent / dependency.removeprefix('@loader_path/')
        elif dependency.startswith('@rpath/'):
            target = frameworks / dependency.removeprefix('@rpath/')
        else:
            errors.append(f'{path.name}: external dependency {dependency}')
            continue
        if not target.exists():
            errors.append(f'{path.name}: missing dependency {dependency}')
if errors:
    raise SystemExit('\n'.join(errors))
if not count or not (bundle / 'Contents/Resources/qt.conf').is_file():
    raise SystemExit('Missing executables or Qt plugin configuration')
print(f'Verified {count} bundled Mach-O binaries; no external SDK dependencies')
