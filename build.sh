#!/bin/bash
# ============================================================================
#  Android 一键构建
#
#  用法:
#     export ANDROID_SDK_ROOT=/path/to/Android/Sdk
#     export ANDROID_NDK_HOME=/path/to/Android/Sdk/ndk/25.2.9519653
#     bash android/build.sh [debug|release]
#
#  产物:
#     android/app/build/outputs/apk/debug/app-debug.apk
#     android/app/build/outputs/apk/release/app-release.apk
#
#  说明:
#     本工程无法在沙盒内完成真实构建 —— 沙盒网络仅放行白名单，
#     下载不到 gradle / Android SDK / NDK。
#     故这里提供的是【可在 Android Studio 直接打开并构建】的完整工程。
#     build.sh 只在已装好 SDK/NDK 的机器上跑。
# ============================================================================
set -e

VARIANT="${1:-debug}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ="$SCRIPT_DIR"

echo "=== 《蛊真人》Android 构建 ==="
echo "工程: $PROJ"
echo "变体: $VARIANT"

# ---------- 环境检查 ----------
if [ -z "$ANDROID_SDK_ROOT" ] && [ -z "$ANDROID_HOME" ]; then
    echo "错误: 未设置 ANDROID_SDK_ROOT / ANDROID_HOME"
    echo "请先安装 Android SDK 与 NDK 25，然后:"
    echo "  export ANDROID_SDK_ROOT=\$HOME/Android/Sdk"
    echo "  export ANDROID_NDK_HOME=\$ANDROID_SDK_ROOT/ndk/25.2.9519653"
    exit 1
fi

SDK="${ANDROID_SDK_ROOT:-$ANDROID_HOME}"
echo "SDK: $SDK"

if [ ! -d "$SDK" ]; then
    echo "错误: SDK 目录不存在: $SDK"
    exit 1
fi

#  生成本地配置，让 Gradle 找到 SDK
echo "sdk.dir=$SDK" > "$PROJ/local.properties"

# ---------- 字体检查 ----------
FONT="$PROJ/app/src/main/assets/fonts/AlibabaPuHuiTi-2-105-Heavy.ttf"
if [ ! -f "$FONT" ]; then
    echo "错误: 缺少中文字体 $FONT"
    echo "Android 系统字体不含完整 CJK 字形，必须自带字体，否则中文显示为方块。"
    exit 1
fi
echo "字体: $(basename "$FONT") ($(stat -c%s "$FONT") 字节)"

# ---------- 构建 ----------
cd "$PROJ"
chmod +x ./gradlew 2>/dev/null || true

#  优先用系统 gradle，避免下载 wrapper 分发包
if command -v gradle >/dev/null 2>&1; then
    GRADLE_CMD="gradle"
elif [ -f ./gradlew ]; then
    GRADLE_CMD="./gradlew"
else
    echo "错误: 既未安装 gradle，也没有 gradlew wrapper"
    exit 1
fi

echo "开始构建..."
$GRADLE_CMD "assemble${VARIANT^}" --stacktrace

# ---------- 回填 .so 到 jniLibs ----------
#  目的：把本次 CMake 产出的 .so 复制进 app/src/main/jniLibs/<abi>/。
#  有了它，后续即使换用不执行 CMake 的打包工具（如手机端 IDE），
#  也能直接把 .so 打进 APK —— 见 app/build.gradle 里的 hasPrebuilt 分支。
SO_DIR="$PROJ/app/build/intermediates/cxx"
if [ -d "$SO_DIR" ]; then
    find "$SO_DIR" -name 'libguzhenren.so' 2>/dev/null | while read -r so; do
        abi=$(basename "$(dirname "$so")")
        case "$abi" in
            arm64-v8a|armeabi-v7a|x86_64|x86) ;;
            *) continue ;;
        esac
        dst="$PROJ/app/src/main/jniLibs/$abi"
        mkdir -p "$dst"
        cp -f "$so" "$dst/"
        echo "已回填: jniLibs/$abi/libguzhenren.so"
    done
fi

echo "=== 构建完成 ==="
find "$PROJ/app/build/outputs/apk" -name "*.apk" 2>/dev/null | while read -r apk; do
    echo "  $apk ($(stat -c%s "$apk") 字节)"

    #  ---------- 关键校验：.so 是否真的打进了 APK ----------
    #  这是本工程最常见的失败：APK 编出来了，但 CMake 没跑（或跑失败），
    #  于是 APK 里没有 lib/*/libguzhenren.so，
    #  安装后一启动就在 System.loadLibrary 处抛 UnsatisfiedLinkError。
    #  在这里提前查出来，比等到真机闪退再反推省事得多。
    if command -v unzip >/dev/null 2>&1; then
        echo "  -- 检查 native 库 --"
        libs=$(unzip -Z1 "$apk" 2>/dev/null | grep -E '^lib/.+libguzhenren\.so$')
        if [ -z "$libs" ]; then
            echo "  [警告] APK 中没有 libguzhenren.so！"
            echo "         应用安装后必然闪退（UnsatisfiedLinkError）。"
            echo "         请确认：构建是否执行了 CMake / externalNativeBuild。"
        else
            echo "$libs" | sed 's/^/         已包含: /'
        fi
    fi
done
