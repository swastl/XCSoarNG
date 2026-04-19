// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

package org.xcsoar;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.Gravity;
import android.view.TextureView;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.TextView;

import com.google.zxing.BinaryBitmap;
import com.google.zxing.PlanarYUVLuminanceSource;
import com.google.zxing.ReaderException;
import com.google.zxing.Result;
import com.google.zxing.common.HybridBinarizer;
import com.google.zxing.qrcode.QRCodeReader;

import java.io.IOException;
import java.util.List;

/**
 * Self-contained QR code scanner Activity using Camera1 + ZXing core.
 * No external app is required.  Camera1 is used deliberately over Camera2
 * because it has a much simpler surface-setup path that works reliably
 * across all Android versions and device brands.
 *
 * Requests the CAMERA permission at runtime if not already granted.
 * Returns the scanned string via {@link #EXTRA_RESULT} in the result Intent.
 */
@SuppressWarnings("deprecation")   /* Camera1 API is deprecated but still works */
public class QRScanActivity extends Activity {
  private static final String TAG = "QRScanActivity";

  public static final String EXTRA_RESULT = "qr_result";

  private static final int REQUEST_CAMERA_PERMISSION = 1001;

  /** Minimum interval between ZXing decode attempts (ms). */
  private static final long ANALYSIS_INTERVAL_MS = 300;

  private TextureView textureView;

  /** Camera1 instance; owned and used exclusively on the background thread. */
  private Camera camera;

  private HandlerThread backgroundThread;
  private Handler backgroundHandler;

  private final QRCodeReader qrReader = new QRCodeReader();
  private volatile boolean scanning = true;
  private volatile long lastAnalysisTime = 0;

  /** Guards against double-open (onResume + onRequestPermissionsResult race). */
  private volatile boolean cameraStarted = false;

  // ── Lifecycle ──────────────────────────────────────────────────────────────

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    buildUi();
    checkAndRequestCamera();
  }

  @Override
  protected void onResume() {
    super.onResume();
    scanning = true;
    startBackgroundThread();
    maybeOpenCamera();
  }

  @Override
  protected void onPause() {
    /* Post the close task so it runs before the thread exits. */
    closeCamera();
    stopBackgroundThread();   /* quitSafely() + join() */
    super.onPause();
  }

  // ── UI ─────────────────────────────────────────────────────────────────────

  private void buildUi() {
    FrameLayout frame = new FrameLayout(this);

    textureView = new TextureView(this);
    frame.addView(textureView,
        new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT));

    TextView hint = new TextView(this);
    hint.setText("Point camera at QR code");
    hint.setTextColor(0xFFFFFFFF);
    hint.setBackgroundColor(0x88000000);
    hint.setPadding(16, 8, 16, 8);
    FrameLayout.LayoutParams hintParams = new FrameLayout.LayoutParams(
        FrameLayout.LayoutParams.WRAP_CONTENT,
        FrameLayout.LayoutParams.WRAP_CONTENT,
        Gravity.TOP | Gravity.CENTER_HORIZONTAL);
    hintParams.topMargin = 40;
    frame.addView(hint, hintParams);

    Button cancelBtn = new Button(this);
    cancelBtn.setText("Cancel");
    cancelBtn.setOnClickListener(v -> {
      setResult(RESULT_CANCELED);
      finish();
    });
    FrameLayout.LayoutParams btnParams = new FrameLayout.LayoutParams(
        FrameLayout.LayoutParams.WRAP_CONTENT,
        FrameLayout.LayoutParams.WRAP_CONTENT,
        Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
    btnParams.bottomMargin = 60;
    frame.addView(cancelBtn, btnParams);

    setContentView(frame);
  }

  // ── Permission ─────────────────────────────────────────────────────────────

  private boolean hasCameraPermission() {
    return getPackageManager().checkPermission(
        Manifest.permission.CAMERA, getPackageName()) == PackageManager.PERMISSION_GRANTED;
  }

  private void checkAndRequestCamera() {
    if (hasCameraPermission()) {
      maybeOpenCamera();
    } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
      requestPermissions(
          new String[]{Manifest.permission.CAMERA},
          REQUEST_CAMERA_PERMISSION);
    } else {
      maybeOpenCamera();
    }
  }

  @Override
  public void onRequestPermissionsResult(int requestCode,
                                          String[] permissions,
                                          int[] grantResults) {
    if (requestCode == REQUEST_CAMERA_PERMISSION) {
      if (grantResults.length > 0
          && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
        maybeOpenCamera();
      } else {
        setResult(RESULT_CANCELED);
        finish();
      }
    }
  }

  // ── Camera ─────────────────────────────────────────────────────────────────

  /**
   * Single entry point for starting the camera.  Idempotent.
   * {@link #closeCamera()} resets the guard for the next onResume cycle.
   */
  private synchronized void maybeOpenCamera() {
    if (cameraStarted || !hasCameraPermission() || backgroundHandler == null)
      return;

    if (textureView.isAvailable()) {
      cameraStarted = true;
      startCameraOnBackground(textureView.getSurfaceTexture());
    } else {
      textureView.setSurfaceTextureListener(new TextureView.SurfaceTextureListener() {
        @Override
        public void onSurfaceTextureAvailable(SurfaceTexture st, int w, int h) {
          synchronized (QRScanActivity.this) {
            if (cameraStarted || backgroundHandler == null) return;
            cameraStarted = true;
          }
          startCameraOnBackground(st);
        }
        @Override public void onSurfaceTextureSizeChanged(SurfaceTexture s, int w, int h) {}
        @Override public boolean onSurfaceTextureDestroyed(SurfaceTexture s) { return true; }
        @Override public void onSurfaceTextureUpdated(SurfaceTexture s) {}
      });
    }
  }

  /**
   * Opens Camera1 and starts the preview.  Must run on the background thread
   * so that preview callbacks are also delivered on the background thread
   * (Camera1 delivers callbacks on the thread that called open()).
   */
  private void startCameraOnBackground(SurfaceTexture texture) {
    backgroundHandler.post(() -> {
      try {
        /* Find the back-facing camera index. */
        int backId = 0;
        Camera.CameraInfo info = new Camera.CameraInfo();
        for (int i = 0; i < Camera.getNumberOfCameras(); i++) {
          Camera.getCameraInfo(i, info);
          if (info.facing == Camera.CameraInfo.CAMERA_FACING_BACK) {
            backId = i;
            break;
          }
        }

        camera = Camera.open(backId);
        camera.setPreviewTexture(texture);

        Camera.Parameters params = camera.getParameters();

        /* Choose a preview size close to 1280×720 (stays within guaranteed
           stream limits and is enough resolution for QR code detection). */
        Camera.Size previewSize =
            chooseCameraSize(params.getSupportedPreviewSizes(), 1280, 720);
        params.setPreviewSize(previewSize.width, previewSize.height);

        /* Enable continuous-picture autofocus if supported. */
        List<String> focusModes = params.getSupportedFocusModes();
        if (focusModes.contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE))
          params.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
        else if (focusModes.contains(Camera.Parameters.FOCUS_MODE_AUTO))
          params.setFocusMode(Camera.Parameters.FOCUS_MODE_AUTO);

        camera.setParameters(params);

        /* Two pre-allocated NV21 buffers for zero-copy preview delivery. */
        int bufSize = previewSize.width * previewSize.height * 3 / 2;
        camera.addCallbackBuffer(new byte[bufSize]);
        camera.addCallbackBuffer(new byte[bufSize]);
        camera.setPreviewCallbackWithBuffer(this::onPreviewFrame);

        camera.startPreview();

      } catch (IOException | RuntimeException e) {
        Log.e(TAG, "Camera open failed", e);
      }
    });
  }

  /**
   * Called on the background thread for each preview frame (NV21 format).
   * Returns the buffer immediately so the camera can reuse it, then decodes
   * only when the throttle interval allows.
   */
  private void onPreviewFrame(byte[] data, Camera cam) {
    /* Return the buffer first so the camera never runs out of buffers. */
    cam.addCallbackBuffer(data);

    if (!scanning)
      return;

    long now = System.currentTimeMillis();
    if (now - lastAnalysisTime < ANALYSIS_INTERVAL_MS)
      return;
    lastAnalysisTime = now;

    Camera.Size size = cam.getParameters().getPreviewSize();
    /* NV21: the first width×height bytes are the Y (luminance) plane,
       which is exactly what PlanarYUVLuminanceSource needs. */
    PlanarYUVLuminanceSource source = new PlanarYUVLuminanceSource(
        data, size.width, size.height, 0, 0, size.width, size.height, false);
    BinaryBitmap bitmap = new BinaryBitmap(new HybridBinarizer(source));
    try {
      Result result = qrReader.decode(bitmap);
      String text = result.getText();
      if (text != null && !text.isEmpty()) {
        scanning = false;
        Intent intent = new Intent();
        intent.putExtra(EXTRA_RESULT, text);
        runOnUiThread(() -> {
          setResult(RESULT_OK, intent);
          finish();
        });
      }
    } catch (ReaderException ignored) {
      /* No QR code in this frame — normal. */
    } finally {
      qrReader.reset();
    }
  }

  private synchronized void closeCamera() {
    cameraStarted = false;
    scanning = false;
    if (backgroundHandler != null) {
      backgroundHandler.post(() -> {
        if (camera != null) {
          try {
            camera.stopPreview();
            camera.setPreviewCallbackWithBuffer(null);
            camera.release();
          } catch (RuntimeException ignored) {}
          camera = null;
        }
      });
    }
  }

  // ── Background thread ──────────────────────────────────────────────────────

  private void startBackgroundThread() {
    if (backgroundThread != null)
      return;
    backgroundThread = new HandlerThread("QRCameraBackground");
    backgroundThread.start();
    backgroundHandler = new Handler(backgroundThread.getLooper());
  }

  private void stopBackgroundThread() {
    if (backgroundThread == null)
      return;
    /* quitSafely processes all already-queued messages (incl. closeCamera post)
       before stopping the thread. */
    backgroundThread.quitSafely();
    try { backgroundThread.join(); } catch (InterruptedException ignored) {}
    backgroundThread = null;
    backgroundHandler = null;
  }

  // ── Helpers ────────────────────────────────────────────────────────────────

  private static Camera.Size chooseCameraSize(List<Camera.Size> choices,
                                               int targetW, int targetH) {
    if (choices == null || choices.isEmpty())
      return null;
    Camera.Size best = choices.get(0);
    long bestDiff = Long.MAX_VALUE;
    for (Camera.Size s : choices) {
      long diff = Math.abs((long) s.width * s.height - (long) targetW * targetH);
      if (diff < bestDiff) {
        bestDiff = diff;
        best = s;
      }
    }
    return best;
  }
}
