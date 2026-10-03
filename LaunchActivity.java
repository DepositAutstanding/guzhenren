package com.gzr.game;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * 启动入口 —— 在真正拉起 NativeActivity 之前，先确认 native 库在不在。
 *
 * 为什么需要这一跳：
 *
 *   GzrActivity 继承 NativeActivity，它的 onCreate 内部会自己加载
 *   android.app.lib_name 指定的库；库缺失时直接抛
 *   `IllegalArgumentException: Unable to find native library`，
 *   进程随即崩溃，且**无法通过 try/catch 拦下来**——
 *   super.onCreate() 一抛出，后面的判断根本执行不到，
 *   而 Activity 又要求必须调用 super.onCreate，不能跳过。
 *
 *   故改为先起一个普通 Activity 试探：
 *     · 能加载 → 转起 GzrActivity（真正的游戏）
 *     · 不能   → 显示一段说明，不崩、也不进入游戏
 *
 *   这样至少能把「APK 里没有 .so」这件事用人话告诉使用者，
 *   而不是甩出一行只有开发者看得懂的堆栈。
 */
public class LaunchActivity extends Activity {

    private static final String TAG = "GuZhenRen";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        boolean ok;
        try {
            System.loadLibrary("guzhenren");
            ok = true;
        } catch (UnsatisfiedLinkError e) {
            ok = false;
            Log.e(TAG, "libguzhenren.so 不可用：" + e.getMessage());
        }

        if (ok) {
            startActivity(new Intent(this, GzrActivity.class));
            finish();
            return;
        }

        //  native 库不可用：给出可读说明
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(48, 64, 48, 48);
        root.setGravity(Gravity.CENTER_HORIZONTAL);

        TextView title = new TextView(this);
        title.setText("《蛊真人》无法启动");
        title.setTextSize(20f);
        title.setGravity(Gravity.CENTER_HORIZONTAL);
        root.addView(title);

        TextView body = new TextView(this);
        body.setText(
            "\n缺少 native 库 libguzhenren.so。\n\n" +
            "本游戏主体是 C++（NDK/CMake 工程），" +
            "当前构建环境没有编译 C++ 代码，只打出了 Java 部分。\n\n" +
            "请改用支持 NDK 的环境重新构建：\n" +
            "· Android Studio（安装 NDK + CMake）\n" +
            "· 或本机命令行：bash android/build.sh debug\n\n" +
            "自检命令：\n" +
            "unzip -l app-debug.apk | grep libguzhenren\n" +
            "（无输出即表示 .so 未打进 APK）"
        );
        body.setTextSize(14f);
        root.addView(body);

        setContentView(root);
    }
}
