// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

package org.xcsoar;

import android.app.Activity;
import android.content.Intent;
import android.util.Log;

/**
 * ZXing Intent-based QR code scanner.
 *
 * Starts an ACTION_SCAN intent compatible with ZXing / Barcode Scanner
 * apps.  When the result arrives in the host Activity's onActivityResult(),
 * call {@link #onActivityResult(int, Intent)} to forward it here.
 * The scanned string is then delivered to native code via JNI.
 */
class QRCodeScanner {
  private static final String TAG = "XCSoar";

  /** Request code used with startActivityForResult(). */
  static final int REQUEST_CODE = 0x5CA3;

  /**
   * Start a QR-code scan via the ZXing Intent.
   * Called from native code (JNI).
   *
   * @param activity  the host Activity (XCSoar)
   */
  public static void startScan(Activity activity) {
    try {
      Intent intent = new Intent("com.google.zxing.client.android.SCAN");
      intent.putExtra("SCAN_MODE", "QR_CODE_MODE");
      activity.startActivityForResult(intent, REQUEST_CODE);
    } catch (Exception e) {
      Log.w(TAG, "QRCodeScanner: failed to start QR scanner: " + e.getMessage());
    }
  }

  /**
   * Forward an onActivityResult() call from the host Activity.
   * Only call this when requestCode == REQUEST_CODE.
   *
   * @param resultCode  Activity.RESULT_OK on success
   * @param data        the Intent returned by the scanner app
   */
  static void onActivityResult(int resultCode, Intent data) {
    if (resultCode == Activity.RESULT_OK && data != null) {
      String result = data.getStringExtra("SCAN_RESULT");
      if (result != null && !result.isEmpty()) {
        onScanResultNative(result);
        return;
      }
    }
    /* scan cancelled or failed — deliver an empty string so the
       native callback can distinguish "no result" from "not called" */
    onScanResultNative("");
  }

  /** Native callback: delivers the scanned string to C++. */
  private static native void onScanResultNative(String result);
}
