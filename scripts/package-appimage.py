#!/usr/bin/env python3
"""Package the production binary on Linux. Requires appimagetool.
Host libc/GPU drivers are not bundled; build on the oldest supported distro.
Never replaces an existing AppImage or packages private recordings.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary',required=True,type=Path)
p.add_argument('--tool',required=True,type=Path)
p.add_argument('--output',required=True,type=Path)
p.add_argument('--work',required=True,type=Path)
a=p.parse_args()
if not a.output.name.startswith('openwatch') or a.output.suffix!='.AppImage':p.error('Output must be named openwatch*.AppImage')
if a.output.exists():p.error('Output already exists; choose a new version')
root=Path(tempfile.mkdtemp(prefix='openwatch-AppDir-',dir=a.work)).resolve()
bin_dir=root/'usr/bin';lib_dir=root/'usr/lib';plugins=root/'usr/plugins'
for d in (bin_dir,lib_dir,plugins):d.mkdir(parents=True,exist_ok=True)
shutil.copy2(a.binary,bin_dir/'openwatch')
qt=Path('/usr/lib/x86_64-linux-gnu/qt6/plugins')
for category in ('platforms','imageformats','xcbglintegrations','wayland-shell-integration','wayland-graphics-integration-client','wayland-decoration-client'):
 if (qt/category).exists():shutil.copytree(qt/category,plugins/category)
skip=re.compile(r'^(ld-linux.*|lib(c|m|pthread|dl|rt|resolv|util|nss_.*)\.so.*|lib(GL|EGL|GLX|GLdispatch|OpenGL)\.so.*)$')
queue=[bin_dir/'openwatch']+list(plugins.rglob('*.so'));seen=set();originals=[]
while queue:
 binary=queue.pop()
 for line in subprocess.check_output(['ldd',str(binary)],text=True).splitlines():
  if 'not found' in line:raise RuntimeError(line)
  match=re.search(r'=> (/\S+)',line)
  if not match:continue
  path=Path(match.group(1));name=path.name
  if name in seen or skip.match(name):continue
  seen.add(name);shutil.copy2(path,lib_dir/name);queue.append(path);originals.append(path)
# Include available distribution copyright notices for bundled dependencies.
notices=root/'usr/share/doc/openwatch';notices.mkdir(parents=True)
for path in originals:
 lookup=subprocess.run(['dpkg-query','-S',str(path.resolve())],capture_output=True,text=True)
 for line in lookup.stdout.splitlines():
  package=line.split(': ')[0].split(':')[0];copyright=Path('/usr/share/doc')/package/'copyright'
  if copyright.is_file():shutil.copy2(copyright,notices/(package+'-copyright'))
source=Path(__file__).resolve().parents[1]
shutil.copy2(source/'LICENSE',notices/'LICENSE')
shutil.copy2(source/'docs/sources.md',notices/'sources.md')
(root/'AppRun').write_text('''#!/bin/sh
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$HERE/usr/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$HERE/usr/plugins/platforms"
exec "$HERE/usr/bin/openwatch" "$@"
''');(root/'AppRun').chmod(0o755)
(root/'openwatch.desktop').write_text('''[Desktop Entry]
Type=Application
Name=OpenWatch
Exec=openwatch
Icon=openwatch
Categories=AudioVideo;Video;
Comment=Video management and recorder playback
''')
(root/'openwatch.svg').write_text('''<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256"><rect width="256" height="256" rx="48" fill="#0c131d"/><circle cx="128" cy="112" r="66" fill="none" stroke="#54dfba" stroke-width="16"/><circle cx="128" cy="112" r="24" fill="#54dfba"/><text x="128" y="222" text-anchor="middle" font-family="sans-serif" font-size="30" fill="white">VMS</text></svg>''')
env={**os.environ,'ARCH':'x86_64'}
subprocess.run([str(a.tool.resolve()),'--no-appstream','--mksquashfs-opt','-processors','--mksquashfs-opt','2',str(root),str(a.output.resolve())],env=env,check=True)
print('APPDIR',root)
print('APPIMAGE',a.output.resolve())
