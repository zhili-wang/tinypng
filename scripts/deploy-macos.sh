#!/usr/bin/env bash
set -euo pipefail
bundle="$1"
qmake_bin="$2"
qt_bin="$(dirname "$qmake_bin")"
qt_plugins="$("$qmake_bin" -query QT_INSTALL_PLUGINS)"
qt_libs="$("$qmake_bin" -query QT_INSTALL_LIBS)"
arguments=(-always-overwrite -no-plugins "-libpath=$qt_libs")
# Qt 6.8 does not recognize this option; signing is applied after deployment.
deploy_help="$("$qt_bin/macdeployqt" -h 2>&1 || true)"
if [[ "$deploy_help" == *"-no-codesign"* ]]; then arguments+=(-no-codesign); fi
# Deploy only the plugins used by this Widgets application and its startup checks.
for relative in platforms/libqcocoa.dylib platforms/libqoffscreen.dylib \
    imageformats/libqgif.dylib imageformats/libqicns.dylib imageformats/libqico.dylib \
    imageformats/libqjpeg.dylib imageformats/libqwebp.dylib \
    tls/libqcertonlybackend.dylib tls/libqsecuretransportbackend.dylib; do
  source="$qt_plugins/$relative"
  if [[ ! -f "$source" ]]; then
    if [[ "$relative" == platforms/libqcocoa.dylib || "$relative" == tls/libqsecuretransportbackend.dylib ]]; then
      echo "缺少必要的 Qt 插件：$relative" >&2
      exit 1
    fi
    continue
  fi
  destination="$bundle/Contents/PlugIns/$relative"
  mkdir -p "$(dirname "$destination")"
  cp "$source" "$destination"
  arguments+=("-executable=$destination")
done
mkdir -p "$bundle/Contents/Resources"
printf '[Paths]\nPlugins = PlugIns\n' > "$bundle/Contents/Resources/qt.conf"
"$qt_bin/macdeployqt" "$bundle" "${arguments[@]}"
