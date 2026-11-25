// Android only: the on-screen pad's calls into the game (com.strecomp.st.StInput), and the program's output to logcat.
#ifdef __ANDROID__
#include <android/log.h>
#include <jni.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <thread>

#include "pad_input.hpp"

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSetButtons(JNIEnv*, jclass, jint held) {
    st::pad::held.store(uint32_t(held));
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSetMic(JNIEnv*, jclass, jboolean on) {
    st::pad::blow.store(on != JNI_FALSE);
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSetMicSample(JNIEnv*, jclass, jboolean on) {
    st::pad::blow_sample.store(on != JNI_FALSE);
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSetLid(JNIEnv*, jclass, jboolean closed) {
    st::pad::lid.store(closed != JNI_FALSE);
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeShowScreen(JNIEnv*, jclass, jint layout) {
    st::pad::show.store(int(layout));
}

extern "C" JNIEXPORT jint JNICALL Java_com_strecomp_st_StInput_nativeMapOffset(JNIEnv*, jclass) {
    return jint(st::pad::map_dy.load());
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_strecomp_st_StInput_nativeMapHidden(JNIEnv*, jclass) {
    return st::pad::map_hidden.load() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSetMap(JNIEnv*, jclass, jint x, jint y, jint w, jint h) {
    st::pad::map_x.store(int(x));
    st::pad::map_y.store(int(y));
    st::pad::map_h.store(int(h));
    st::pad::map_w.store(int(w));
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSetSmooth(JNIEnv*, jclass, jboolean on) {
    st::pad::smooth.store(on != JNI_FALSE ? 1 : 0);
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSetHdText(JNIEnv*, jclass, jboolean on) {
    st::pad::hd_text.store(on != JNI_FALSE ? 1 : 0);
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSetFps(JNIEnv*, jclass, jboolean on) {
    st::pad::fps.store(on != JNI_FALSE);
}

extern "C" JNIEXPORT jfloat JNICALL Java_com_strecomp_st_StInput_nativeGameFps(JNIEnv*, jclass) {
    return jfloat(st::pad::game_fps10.load()) / 10.0f;
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSaveState(JNIEnv*, jclass, jint slot) {
    st::pad::state_save.store(int(slot));
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeLoadState(JNIEnv*, jclass, jint slot) {
    st::pad::state_load.store(int(slot));
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSetVideo(JNIEnv*, jclass, jboolean gpu, jint scale) {
    st::pad::video.store((gpu != JNI_FALSE ? 1 << 8 : 0) | (int(scale) & 0xFF));
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_strecomp_st_StInput_nativeGpuFailed(JNIEnv*, jclass) {
    return st::pad::gpu_failed.load() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSetCompute(JNIEnv*, jclass, jboolean on) {
    st::pad::compute.store(on != JNI_FALSE ? 1 : 0);
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_strecomp_st_StInput_nativeComputeFailed(JNIEnv*, jclass) {
    return st::pad::compute_failed.load() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSetFpsCorner(JNIEnv*, jclass, jint corner) {
    st::pad::fps_corner.store(int(corner) & 3);
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSetStretch(JNIEnv*, jclass, jboolean on) {
    st::pad::stretch.store(on != JNI_FALSE ? 1 : 0);
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSetStick(JNIEnv*, jclass, jfloat x, jfloat y) {
    st::pad::stick_x.store(int(x * 1000.0f));
    st::pad::stick_y.store(int(y * 1000.0f));
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSetTextures(JNIEnv*, jclass, jint mode) {
    st::pad::textures.store(int(mode));
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSetWide(JNIEnv*, jclass, jint mode) {
    st::pad::wide_mode.store(std::clamp(int(mode), 0, 3));
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeSetStickMode(JNIEnv*, jclass, jboolean on) {
    st::pad::stick_mode.store(on != JNI_FALSE ? 1 : 0);
}

extern "C" JNIEXPORT void JNICALL Java_com_strecomp_st_StInput_nativeRequestResume(JNIEnv*, jclass) {
    st::pad::resume_done.store(false);
    st::pad::resume_request.store(true);
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_strecomp_st_StInput_nativeResumeDone(JNIEnv*, jclass) {
    return st::pad::resume_done.load() && !st::pad::resume_request.load() ? JNI_TRUE : JNI_FALSE;
}

namespace {

// Android drops a native program's stdout and stderr: send both, line by line, to logcat (tag "strecomp").
struct LogcatPipe {
    LogcatPipe() {
        int fds[2];
        if (pipe(fds) != 0) return;
        std::setvbuf(stdout, nullptr, _IOLBF, 0);
        std::setvbuf(stderr, nullptr, _IONBF, 0);
        dup2(fds[1], 1);
        dup2(fds[1], 2);
        close(fds[1]);
        const int in = fds[0];
        std::thread([in] {
            char buf[1024];
            size_t used = 0;
            for (;;) {
                const ssize_t n = read(in, buf + used, sizeof buf - 1 - used);
                if (n <= 0) break;
                used += size_t(n);
                size_t start = 0;
                for (size_t i = 0; i < used; i++) {
                    if (buf[i] != '\n') continue;
                    buf[i] = 0;
                    __android_log_write(ANDROID_LOG_INFO, "strecomp", buf + start);
                    start = i + 1;
                }
                if (start == 0 && used == sizeof buf - 1) {  // a line longer than the buffer: write it as it is
                    buf[used] = 0;
                    __android_log_write(ANDROID_LOG_INFO, "strecomp", buf);
                    used = 0;
                } else if (start > 0) {
                    for (size_t i = start; i < used; i++) buf[i - start] = buf[i];
                    used -= start;
                }
            }
        }).detach();
    }
} logcat_pipe;

}  // namespace
#endif
