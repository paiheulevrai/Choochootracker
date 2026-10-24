#include "../src/corelib/corelib_assets.h"
#include "../src/corelib/corelib_file.h"
#include <SDL2/SDL.h>
#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int androidGetWorkspacePath(char* buffer, int bufferSize) {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity) return 1;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(cls, "getWorkspacePath", "()Ljava/lang/String;");
    jstring path = method ? (jstring)env->CallObjectMethod(activity, method) : NULL;
    const char* value = path ? env->GetStringUTFChars(path, NULL) : NULL;
    if (value) snprintf(buffer, bufferSize, "%s", value);
    if (value) env->ReleaseStringUTFChars(path, value);
    if (path) env->DeleteLocalRef(path);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    return value ? 0 : 1;
}

// The app's private workspace: always a real path, even when the user's picked
// folder is the active workspace. Bundled content is seeded here.
int androidGetPrivateWorkspacePath(char* buffer, int bufferSize) {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity) return 1;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(cls, "getPrivateWorkspacePath", "()Ljava/lang/String;");
    jstring path = method ? (jstring)env->CallObjectMethod(activity, method) : NULL;
    const char* value = path ? env->GetStringUTFChars(path, NULL) : NULL;
    if (value) snprintf(buffer, bufferSize, "%s", value);
    if (value) env->ReleaseStringUTFChars(path, value);
    if (path) env->DeleteLocalRef(path);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    return value ? 0 : 1;
}

// Runs a no-argument void method of the activity (pickWorkingFolder, clearWorkingFolder).
void androidWorkingFolderCommand(const char* name) {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity) return;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(cls, name, "()V");
    if (method) env->CallVoidMethod(activity, method);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
}

void androidSetTouchHaptics(int enabled) {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity) return;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(cls, "setTouchHaptics", "(Z)V");
    if (method) env->CallVoidMethod(activity, method, enabled != 0);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
}

void androidPerformTouchHaptic(void) {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity) return;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(cls, "performTouchHaptic", "()V");
    if (method) env->CallVoidMethod(activity, method);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
}

int androidGetWorkingFolderStatus(char* buffer, int bufferSize) {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity) return 1;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(cls, "getWorkingFolderStatus", "()Ljava/lang/String;");
    jstring status = method ? (jstring)env->CallObjectMethod(activity, method) : NULL;
    const char* value = status ? env->GetStringUTFChars(status, NULL) : NULL;
    if (value) snprintf(buffer, bufferSize, "%s", value);
    if (value) env->ReleaseStringUTFChars(status, value);
    if (status) env->DeleteLocalRef(status);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    return value ? 0 : 1;
}

void androidOpenDocument(const char* mimeType, const char* relativeDirectory) {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity) return;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(cls, "openDocument", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (method) {
        jstring mime = env->NewStringUTF(mimeType);
        jstring directory = env->NewStringUTF(relativeDirectory);
        env->CallVoidMethod(activity, method, mime, directory);
        env->DeleteLocalRef(mime);
        env->DeleteLocalRef(directory);
    }
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
}

void androidSaveDocument(const char* path, const char* mimeType) {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity) return;
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(cls, "saveDocument", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if (method) {
        const char* name = strrchr(path, '/');
        name = name ? name + 1 : path;
        jstring source = env->NewStringUTF(path);
        jstring mime = env->NewStringUTF(mimeType);
        jstring filename = env->NewStringUTF(name);
        env->CallVoidMethod(activity, method, source, mime, filename);
        env->DeleteLocalRef(source);
        env->DeleteLocalRef(mime);
        env->DeleteLocalRef(filename);
    }
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
}

int assetsInit(void) {
    char workspacePath[1024];
    if (androidGetWorkspacePath(workspacePath, sizeof(workspacePath))) return 1;
    // Java completes the non-destructive private asset seed before SDL starts.
    // The active workspace may overlay it for a direct-storage build.
    if (chdir(workspacePath) != 0) return 1;
    return 0;
}
