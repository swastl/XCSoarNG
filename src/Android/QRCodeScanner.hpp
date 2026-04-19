// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#ifdef ANDROID

#include <functional>
#include <jni.h>

namespace QRCodeScanner {

/** Looks up the Java QRCodeScanner class and its static methods. */
void Initialise(JNIEnv *env) noexcept;

/** Releases the Java class reference. */
void Deinitialise(JNIEnv *env) noexcept;

/**
 * Start a QR-code scan via an Android ZXing Intent.
 * @callback is invoked on the native UI thread with the scanned string,
 * or with an empty string if the scan was cancelled / failed.
 */
void StartScan(std::function<void(const char *result)> callback) noexcept;

} // namespace QRCodeScanner

#endif // ANDROID
