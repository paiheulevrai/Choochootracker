// Android-side plumbing for the Ableton Link backend (T28).
//
// Link's discovery layer uses multicast UDP; Android requires holding a
// WifiManager.MulticastLock for multicast packets to reach the app. The
// Java side exposes acquireLinkMulticastLock()/releaseLinkMulticastLock()
// on the activity (see MainActivity); this file resolves those methods
// through JNI and calls them around link.enable(), following the same
// SDL_AndroidGetJNIEnv pattern as midi_backend_android.cpp.
//
// The C entry points declared extern "C" in link_backend_link.cpp under
// ANDROID_BUILD:
//   linkBackendAndroidAcquireMulticastLock() -> 1 on success, 0 on failure
//   linkBackendAndroidReleaseMulticastLock() -> 1 on success, 0 on failure

#include <jni.h>
#include <SDL2/SDL.h>

namespace {

JavaVM* linkVm = nullptr;
jobject linkActivity = nullptr;
jmethodID acquireLock = nullptr;
jmethodID releaseLock = nullptr;

struct LinkAttachedEnv {
  JNIEnv* env = nullptr;
  bool attached = false;
  bool get(void) {
    if (!linkVm) return false;
    if (linkVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) return true;
    if (linkVm->AttachCurrentThread(&env, nullptr) != JNI_OK) return false;
    attached = true;
    return true;
  }
  ~LinkAttachedEnv(void) { if (attached) linkVm->DetachCurrentThread(); }
};

bool ensureLinkJava(void) {
  if (linkActivity) return true;
  JNIEnv* env = reinterpret_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
  jobject current = reinterpret_cast<jobject>(SDL_AndroidGetActivity());
  if (!env || !current) return false;
  env->GetJavaVM(&linkVm);
  linkActivity = env->NewGlobalRef(current);
  jclass cls = env->GetObjectClass(linkActivity);
  acquireLock = env->GetMethodID(cls, "acquireLinkMulticastLock", "()Z");
  releaseLock = env->GetMethodID(cls, "releaseLinkMulticastLock", "()Z");
  env->DeleteLocalRef(cls);
  if (!acquireLock || !releaseLock) {
    // Clear any pending NoSuchMethodError so it doesn't abort the app when
    // native code returns to Java.
    if (env->ExceptionCheck()) env->ExceptionClear();
    return false;
  }
  return true;
}

// Shared implementation: resolve the method, call it, report success.
int callLockMethod(jmethodID method) {
  if (!ensureLinkJava()) return 0;
  LinkAttachedEnv attached;
  if (!attached.get()) return 0;
  jboolean result = attached.env->CallBooleanMethod(linkActivity, method);
  if (attached.env->ExceptionCheck()) {
    attached.env->ExceptionClear();
    return 0;
  }
  return result ? 1 : 0;
}

}  // namespace

extern "C" int linkBackendAndroidAcquireMulticastLock(void) {
  return callLockMethod(acquireLock);
}

extern "C" int linkBackendAndroidReleaseMulticastLock(void) {
  return callLockMethod(releaseLock);
}
