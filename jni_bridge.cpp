// ============================================================================
//  JNI 桥 —— Java 侧中文输入法与本体的连接
//
//  为什么需要这一层：
//    NativeActivity 的原生 IME 不支持中文（只能输入 ASCII），
//    而开局取名、取假名都必须打中文。
//    故在 Java 侧放一个透明的 EditText 覆盖层承接输入法，
//    用户输入的中文通过这里回调到 C++，再喂给 ImGui。
// ============================================================================
#include <jni.h>

#include "AndroidBackend.hpp"

//  单实例：由 nativeSetBackend 在启动时保存
static gr::android::AndroidBackend* g_backend = nullptr;

extern "C" {

//  保存 backend 实例指针
JNIEXPORT void JNICALL
Java_com_gzr_game_GzrActivity_nativeSetBackend(JNIEnv*, jclass, jlong ptr) {
    g_backend = reinterpret_cast<gr::android::AndroidBackend*>(ptr);
}

//  输入框提交文本（中文逐段回调）
JNIEXPORT void JNICALL
Java_com_gzr_game_GzrActivity_nativeCommitText(JNIEnv* env, jclass, jstring text) {
    if (!text || !g_backend) return;
    const char* s = env->GetStringUTFChars(text, nullptr);
    if (!s) return;
    g_backend->commitText(s);
    env->ReleaseStringUTFChars(text, s);
}

//  软键盘显示状态变化
JNIEXPORT void JNICALL
Java_com_gzr_game_GzrActivity_nativeImeVisible(JNIEnv*, jclass, jboolean v) {
    if (g_backend) g_backend->setImeVisible(v == JNI_TRUE);
}

} // extern "C"
