package com.gzr.game;

import android.app.NativeActivity;
import android.content.Context;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;

/**
 * 宿主 Activity。
 *
 * 继承 NativeActivity —— 它会自动加载 libguzhenren.so 并调用 android_main，
 * 主循环在 native 侧跑，Java 侧只负责一件事：中文输入。
 *
 * 为什么要这个 Overlay：
 *   NativeActivity 的原生 IME 只能输入 ASCII，打不出中文。
 *   而开局取名、取假名都必须打中文（且要能与原著人物名录比对）。
 *   故在此放一个几乎不可见的 EditText 承接输入法，
 *   把用户提交的文本通过 JNI 回调给 native，再喂给 ImGui。
 */
public class GzrActivity extends NativeActivity {

    private EditText imeSink;
    private InputMethodManager imm;

    //  native 方法（实现见 cpp/jni_bridge.cpp）
    public static native void nativeSetBackend(long ptr);
    public static native void nativeCommitText(String text);
    public static native void nativeImeVisible(boolean visible);

    /**
     * 加载 native 库。
     *
     * 原生库没打进 APK 时，System.loadLibrary 会抛 UnsatisfiedLinkError。
     * 放在 static 块里会让进程在类初始化阶段直接崩 —— 日志只有一行堆栈，
     * 看不出到底是没编出来、ABI 不匹配，还是没解包。
     * 故在此捕获并给出可读的提示。
     *
     * 注意：NativeActivity 自己也会按 meta-data 的 android.app.lib_name
     * 加载同名库，这里的显式加载是提前一步、便于定位问题。
     */
    private static boolean loadNative() {
        try {
            System.loadLibrary("guzhenren");
            return true;
        } catch (UnsatisfiedLinkError e) {
            android.util.Log.e("GuZhenRen",
                "libguzhenren.so 加载失败：APK 中未包含该库。" +
                "常见原因：① 构建未执行 CMake（externalNativeBuild）；" +
                "② 目标 ABI 未产出；③ native 库未被解包。详见 android/README.md", e);
            return false;
        }
    }

    private static volatile boolean sNativeLoaded;

    static {
        sNativeLoaded = loadNative();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        //  native 库缺失时给出可见提示并体面退出，
        //  而不是让进程在后续调用 native 方法时崩溃。
        if (!sNativeLoaded) {
            android.widget.Toast.makeText(this,
                "未能加载 libguzhenren.so：本工程为 NDK（C++）工程，" +
                "需支持 CMake 的构建环境（建议用 Android Studio）",
                android.widget.Toast.LENGTH_LONG).show();
            return;
        }

        imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);

        //  透明输入框：1×1 像素、全透明。
        //  不能 setVisibility(GONE) —— GONE 的 View 无法唤起输入法。
        imeSink = new EditText(this);
        imeSink.setLayoutParams(new FrameLayout.LayoutParams(1, 1, Gravity.START | Gravity.TOP));
        imeSink.setAlpha(0.0f);
        imeSink.setBackgroundColor(0x00000000);
        imeSink.setCursorVisible(false);
        imeSink.setSingleLine(false);
        //  中文拼音输入需要这些类型
        imeSink.setInputType(android.text.InputType.TYPE_CLASS_TEXT
                | android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        imeSink.setImeOptions(android.view.inputmethod.EditorInfo.IME_FLAG_NO_EXTRACT_UI
                | android.view.inputmethod.EditorInfo.IME_ACTION_NONE);

        //  输入法提交文本后立刻清空 sink，逐段送 native。
        //  为什么要清空：EditText 会累积全部历史，
        //  若每次把整串送过去，第二次回调就会把第一段重复送一遍。
        imeSink.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int st, int c, int a) { }
            @Override public void onTextChanged(CharSequence s, int st, int b, int c) { }

            @Override
            public void afterTextChanged(Editable s) {
                if (s == null || s.length() == 0) return;
                String text = s.toString();
                nativeCommitText(text);
                //  清空会再次触发本回调，故先移除监听再清空再加回
                imeSink.removeTextChangedListener(this);
                s.clear();
                imeSink.addTextChangedListener(this);
            }
        });

        addContentView(imeSink, imeSink.getLayoutParams());
    }

    /** 唤起软键盘（native 在输入框获得焦点时调用） */
    public void showIme() {
        runOnUiThread(() -> {
            imeSink.requestFocus();
            if (imm != null) imm.showSoftInput(imeSink, InputMethodManager.SHOW_IMPLICIT);
            nativeImeVisible(true);
        });
    }

    /** 收起软键盘 */
    public void hideIme() {
        runOnUiThread(() -> {
            if (imm != null) imm.hideSoftInputFromWindow(imeSink.getWindowToken(), 0);
            nativeImeVisible(false);
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    @Override
    protected void onPause() {
        super.onPause();
    }
}
