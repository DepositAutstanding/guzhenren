# jniLibs —— 放置【预编译好】的 native 库

## 为什么有这个目录

`libguzhenren.so` 由 C++（NDK/CMake）编译而来。
若你的构建环境**不执行 CMake**（多数手机端编译工具如此），
APK 里就不会有这个库，启动时必然报：

```
Unable to find native library guzhenren
```

解决办法：在**任何**有 NDK 的环境里先把 .so 编译出来，
放进本目录，再让打包工具打 APK。
放在这里的 .so 会被自动打进 `lib/<abi>/`，**无需再跑 CMake**。

## 目录结构

```
jniLibs/
└── arm64-v8a/libguzhenren.so     ← 你的机器（/lib/arm64）需要这个
```

其他 ABI（可选）：`armeabi-v7a/`、`x86_64/`

## 怎么拿到 .so

在有 NDK 的机器上：

```bash
bash android/build.sh debug
```

脚本会在构建后自动把产出的 .so 复制进本目录，
并校验 APK 中是否确实包含它。

## 注意

本目录的 .so 是**构建产物**，不入库（见 .gitignore）。
