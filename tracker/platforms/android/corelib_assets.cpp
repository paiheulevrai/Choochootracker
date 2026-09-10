#include "../src/corelib/corelib_assets.h"
#include "../src/corelib/corelib_file.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static AAssetManager* assetManager = NULL;

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

static AAssetManager* getAssetManager(void) {
    if (assetManager) return assetManager;

    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (!env) return NULL;

    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!activity) return NULL;

    jclass activityClass = env->GetObjectClass(activity);
    jmethodID getAssets = env->GetMethodID(activityClass, "getAssets", "()Landroid/content/res/AssetManager;");
    jobject assetManagerObj = env->CallObjectMethod(activity, getAssets);

    assetManager = AAssetManager_fromJava(env, assetManagerObj);

    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(activityClass);
    env->DeleteLocalRef(assetManagerObj);

    return assetManager;
}

static const char* assetDirs[] = {
    "choochootracker_data/fonts",
    "choochootracker_data/instruments",
    "choochootracker_data/pitch-tables",
    "choochootracker_data/projects",
    "choochootracker_data/samples",
    "choochootracker_data/themes",
    "choochootracker_data/AY_wavetables",
    "choochootracker_data/SR_wavetables",
    "choochootracker_data/waveforms",
    "choochootracker_data/title",
    NULL
};

static int fileExists(const char* path) {
    return access(path, F_OK) == 0;
}

static int copyAssetFile(const char* assetPath, const char* destPath) {
    AAssetManager* mgr = getAssetManager();
    if (!mgr) return 1;

    AAsset* asset = AAssetManager_open(mgr, assetPath, AASSET_MODE_BUFFER);
    if (!asset) return 1;

    char dirPath[512];
    strcpy(dirPath, destPath);
    char* lastSlash = strrchr(dirPath, '/');
    if (lastSlash) {
        *lastSlash = 0;
        mkdir(dirPath, 0755);
    }

    FILE* outFile = fopen(destPath, "wb");
    if (!outFile) {
        AAsset_close(asset);
        return 1;
    }

    const void* buffer = AAsset_getBuffer(asset);
    size_t size = AAsset_getLength(asset);

    if (fwrite(buffer, 1, size, outFile) != size) {
        fclose(outFile);
        AAsset_close(asset);
        return 1;
    }

    fclose(outFile);
    AAsset_close(asset);
    return 0;
}

static void copyAssetTree(AAssetManager* mgr, const char* assetDir, const char* destDir) {
    mkdir(destDir, 0755);
    AAssetDir* dir = AAssetManager_openDir(mgr, assetDir);
    if (!dir) return;
    const char* filename;
    while ((filename = AAssetDir_getNextFileName(dir)) != NULL) {
        char assetPath[1024];
        char destPath[1024];
        snprintf(assetPath, sizeof(assetPath), "%s/%s", assetDir, filename);
        snprintf(destPath, sizeof(destPath), "%s/%s", destDir, filename);
        if (!fileExists(destPath) && copyAssetFile(assetPath, destPath) != 0)
            copyAssetTree(mgr, assetPath, destPath);
    }
    AAssetDir_close(dir);
}

int assetsInit(void) {
    if (!getAssetManager()) return 1;

    char dataPath[1024];
    if (androidGetWorkspacePath(dataPath, sizeof(dataPath))) return 1;
    mkdir(dataPath, 0755);

    AAssetManager* mgr = getAssetManager();

    for (int i = 0; assetDirs[i] != NULL; i++) {
        const char* assetDir = assetDirs[i];
        const char* subpath = strchr(assetDir, '/') + 1;

        char destDir[512];
        snprintf(destDir, sizeof(destDir), "%s/%s", dataPath, subpath);
        copyAssetTree(mgr, assetDir, destDir);
    }

    // Bundled projects store their sample paths relative to the application
    // data root (for example "samples/909/BT0A0D0.WAV"). Android starts the
    // native process in a different working directory, so make that root the
    // current directory once the private workspace is ready.
    if (chdir(dataPath) != 0) return 1;

    return 0;
}
