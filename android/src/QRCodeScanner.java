// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

package org.xcsoar;

import android.app.Activity;
import android.content.Intent;

/**
 * QR code scanner.
 *
 * Launches {@link QRScanActivity} which uses the Camera2 API and the
 * bundled ZXing core library — no external scanner app is required.
 *
 * When the result arrives in the host Activity's onActivityResult(),
 * call {@link #onActivityResult(int, Intent)} to forward it here.
 * The scanned string is then delivered to native code via JNI.
 */
class QRCodeScanner {
  /** Request code used with startActivityForResult(). */
  static final int REQUEST_CODE = 0x5CA3;

  /**
   * Start a QR-code scan.
   * Called from native code (JNI).
   *
   * @param activity  the host Activity (XCSoar)
   */
  public static void startScan(Activity activity) {
    Intent intent = new Intent(activity, QRScanActivity.class);
    activity.startActivityForResult(intent, REQUEST_CODE);
  }

  /**
   * Forward an onActivityResult() call from the host Activity.
   * Only call this when requestCode == REQUEST_CODE.
   *
   * @param resultCode  Activity.RESULT_OK on success
   * @param data        the Intent returned by the scanner
   */
  static void onActivityResult(int resultCode, Intent data) {
    String result = "";
    if (resultCode == Activity.RESULT_OK && data != null) {
      String scanned = data.getStringExtra(QRScanActivity.EXTRA_RESULT);
      if (scanned != null)
        result = scanned;
    }
    /* deliver empty string on cancel so the native callback is not left
       waiting indefinitely */
    onScanResultNative(result);
  }

  /** Native callback: delivers the scanned string to C++. */
  private static native void onScanResultNative(String result);
}
