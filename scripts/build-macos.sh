#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
qt_root="${1:-${TINYPNG_QT_DIR:-}}"
version="${TINYPNG_VERSION:-1.2.1}"
architectures="${TINYPNG_MAC_ARCHS:-$(uname -m)}"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then echo "无效版本号：$version" >&2; exit 1; fi
if [[ -n "$qt_root" ]]; then
  qmake_bin="$qt_root/bin/qmake"
else
  qmake_bin="$(command -v qmake6 || command -v qmake || true)"
fi
if [[ -z "$qmake_bin" || ! -x "$qmake_bin" ]]; then
  echo "缺少 Qt SDK。请设置 TINYPNG_QT_DIR，或传入 Qt macOS Kit 路径。" >&2
  exit 1
fi
qt_bin="$(dirname "$qmake_bin")"
if [[ ! -x "$qt_bin/macdeployqt" ]]; then
  echo "Qt SDK 中缺少 macdeployqt，无法生成包含运行库的安装包。" >&2
  exit 1
fi
arch_label="${architectures// /-}"
if [[ "$architectures" == *x86_64* && "$architectures" == *arm64* ]]; then arch_label=universal; fi
mkdir -p "$project_root/build"
build_root="$(mktemp -d "$project_root/build/macos-$arch_label-$version.XXXXXX")"
checks_root="$build_root/checks"
installer_root="$project_root/Installer/$version"
mkdir -p "$checks_root" "$installer_root"
cd "$checks_root"
"$qmake_bin" "$project_root/tests/regression.pro" -after 'CONFIG+=release' 'CONFIG-=debug' "QMAKE_APPLE_DEVICE_ARCHS=$architectures"
make -j "$(sysctl -n hw.ncpu)"
QT_QPA_PLATFORM=offscreen ./regression
integration_root="$build_root/integration"
mkdir -p "$integration_root"
cd "$integration_root"
"$qmake_bin" "$project_root/tests/integration.pro" -after 'CONFIG+=release' 'CONFIG-=debug' "QMAKE_APPLE_DEVICE_ARCHS=$architectures"
make -j "$(sysctl -n hw.ncpu)"
QT_QPA_PLATFORM=offscreen ./integration
cd "$build_root"
"$qmake_bin" "$project_root/tinypng/VsTinypng.pro" -after 'CONFIG+=release' 'CONFIG-=debug' 'TARGET=TinyPNG' "VERSION=$version" "QMAKE_APPLE_DEVICE_ARCHS=$architectures"
make -j "$(sysctl -n hw.ncpu)"
deploy_log="$build_root/deploy.log"
if ! bash "$project_root/scripts/deploy-macos.sh" "$build_root/TinyPNG.app" "$qmake_bin" > "$deploy_log" 2>&1; then
  cat "$deploy_log" >&2
  exit 1
fi
cat "$deploy_log"
if grep -q '^ERROR:' "$deploy_log"; then
  echo "Qt 部署不完整，停止生成安装包。" >&2
  exit 1
fi
python3 "$project_root/scripts/verify-macos-bundle.py" "$build_root/TinyPNG.app"
# Apply a local signature after deployment; this does not notarize the app.
codesign --force --deep --sign - "$build_root/TinyPNG.app"
codesign --verify --deep --strict "$build_root/TinyPNG.app"
stage="$(mktemp -d "$build_root/dmg-stage.XXXXXX")"
trap 'rm -rf "$stage"' EXIT
cp -R "$build_root/TinyPNG.app" "$stage/"
ln -s /Applications "$stage/Applications"
package="$installer_root/TinyPNG-macOS-$arch_label-$version.dmg"
hdiutil create -volname TinyPNG -srcfolder "$stage" -ov -format UDZO "$package"
hdiutil verify "$package"
python3 "$project_root/scripts/package-checksums.py" "$installer_root"
printf '已生成：%s\n' "$package"
