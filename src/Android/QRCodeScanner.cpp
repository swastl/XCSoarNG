// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "QRCodeScanner.hpp"
#include "Main.hpp"
#include "Context.hpp"
#include "java/Global.hxx"
#include "java/String.hxx"
#include "java/Class.hxx"
#include "ui/event/Globals.hpp"
#include "ui/event/Queue.hpp"
#include "util/Compiler.h"

#include <jni.h>
#include <mutex>
#include <string>

/** Pending scan callback, protected by #scan_callback_mutex. */
static std::mutex scan_callback_mutex;
static std::function<void(const char *)> scan_callback;

/** Java QRCodeScanner class. */
static Java::TrivialClass qr_cls;

/** QRCodeScanner.startScan(Activity) */
static jmethodID startScan_method;

void
QRCodeScanner::Initialise(JNIEnv *env) noexcept
{
  qr_cls.Find(env, "org/xcsoar/QRCodeScanner");
  startScan_method = env->GetStaticMethodID(qr_cls, "startScan",
                                            "(Landroid/app/Activity;)V");
}

void
QRCodeScanner::Deinitialise(JNIEnv *env) noexcept
{
  qr_cls.Clear(env);
}

void
QRCodeScanner::StartScan(std::function<void(const char *result)> callback) noexcept
{
  if (!qr_cls.IsDefined())
    return;

  {
    const std::lock_guard lock{scan_callback_mutex};
    scan_callback = std::move(callback);
  }

  JNIEnv *env = Java::GetEnv();
  env->CallStaticVoidMethod(qr_cls, startScan_method, context->Get());

  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    /* Java call failed — fire callback with empty result so callers
       are not left waiting indefinitely. */
    std::function<void(const char *)> cb;
    {
      const std::lock_guard lock{scan_callback_mutex};
      cb = std::move(scan_callback);
      scan_callback = nullptr;
    }
    if (cb)
      cb("");
  }
}

/**
 * Dispatched onto the native UI thread via InjectCall.
 * ctx is a heap-allocated std::string with the scan result.
 */
static void
DispatchScanResult(void *ctx) noexcept
{
  auto *result = static_cast<std::string *>(ctx);

  std::function<void(const char *)> cb;
  {
    const std::lock_guard lock{scan_callback_mutex};
    cb = std::move(scan_callback);
    /* Explicitly reset to empty so a concurrent StartScan() call
       will not find a stale callback after this dispatch. */
    scan_callback = nullptr;
  }

  if (cb)
    cb(result->c_str());

  delete result;
}

extern "C"
gcc_visibility_default
JNIEXPORT void JNICALL
Java_org_xcsoar_QRCodeScanner_onScanResultNative(JNIEnv *env,
                                                 [[maybe_unused]] jclass cls,
                                                 jstring result)
{
  if (UI::event_queue == nullptr)
    return;

  auto *s = new std::string(Java::String::GetUTFChars(env, result).c_str());
  UI::event_queue->InjectCall(DispatchScanResult, s);
}
