package com.w9wdx.qk4phone;

import android.Manifest;
import android.app.Activity;
import android.content.ClipData;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.os.Build;
import android.provider.MediaStore;
import android.net.Uri;
import android.view.WindowManager;
import android.view.OrientationEventListener;

import androidx.core.content.FileProvider;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

import org.qtproject.qt.android.bindings.QtActivity;

/** Keeps the display awake only while the QK4 activity is in the foreground. */
public class Qk4Activity extends QtActivity {
    private static final int SSTV_GALLERY_REQUEST = 7401;
    private static final int SSTV_CAMERA_REQUEST = 7402;
    private static final int SSTV_CAMERA_PERMISSION_REQUEST = 7403;
    private static Qk4Activity instance;
    private String sstvCameraPath;
    private String sstvImportedImagePath;
    private boolean sstvCameraPermissionPending;
    private boolean sstvMediaOperationActive;
    private String sstvMediaError;
    private OrientationEventListener logbookOrientationListener;
    private boolean radioLogbookActive;
    private boolean radioLogbookRotationStarted;

    /** Keep the radio's initial landscape presentation until the operator
     * turns the phone, then allow both orientations for the logbook. */
    public static void setRadioLogbookRotation(boolean enabled) {
        Qk4Activity activity = instance;
        if (activity == null) return;
        activity.runOnUiThread(() -> {
            if (activity.logbookOrientationListener != null) {
                activity.logbookOrientationListener.disable();
                activity.logbookOrientationListener = null;
            }
            activity.radioLogbookActive = enabled;
            activity.radioLogbookRotationStarted = false;
            activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
            if (!enabled) return;
            activity.logbookOrientationListener = new OrientationEventListener(activity) {
                private int initialQuadrant = -1;
                @Override public void onOrientationChanged(int angle) {
                    if (angle == ORIENTATION_UNKNOWN) return;
                    int quadrant = ((angle + 45) / 90) % 4;
                    int distance = Math.abs(angle - quadrant * 90);
                    distance = Math.min(distance, 360 - distance);
                    if (distance > 25) return; // Ignore diagonal jitter.
                    if (initialQuadrant < 0) initialQuadrant = quadrant;
                    if (quadrant != initialQuadrant) {
                        activity.radioLogbookRotationStarted = true;
                        activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR);
                        disable();
                    }
                }
            };
            activity.logbookOrientationListener.enable();
        });
    }

    @Override public void setRequestedOrientation(int requested) {
        if (radioLogbookActive) requested = radioLogbookRotationStarted
            ? ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR : ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
        super.setRequestedOrientation(requested);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        instance = this;
        if (savedInstanceState != null) {
            sstvCameraPath = savedInstanceState.getString("sstvCameraPath");
            sstvImportedImagePath = savedInstanceState.getString("sstvImportedImagePath");
            sstvCameraPermissionPending = savedInstanceState.getBoolean("sstvCameraPermissionPending", false);
            sstvMediaOperationActive = savedInstanceState.getBoolean("sstvMediaOperationActive", false);
            sstvMediaError = savedInstanceState.getString("sstvMediaError");
        }
        cleanupAbandonedSstvMedia();
        requestKeepScreenOn();
    }

    @Override
    protected void onSaveInstanceState(Bundle state) {
        synchronized (this) {
            state.putString("sstvCameraPath", sstvCameraPath);
            state.putString("sstvImportedImagePath", sstvImportedImagePath);
            state.putBoolean("sstvCameraPermissionPending", sstvCameraPermissionPending);
            state.putBoolean("sstvMediaOperationActive", sstvMediaOperationActive);
            state.putString("sstvMediaError", sstvMediaError);
        }
        super.onSaveInstanceState(state);
    }

    @Override
    protected void onResume() {
        super.onResume();
        requestKeepScreenOn();
    }

    @Override
    protected void onPause() {
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        if (logbookOrientationListener != null) logbookOrientationListener.disable();
        if (instance == this) {
            instance = null;
        }
        super.onDestroy();
    }

    /** Opens Android's scoped image picker; the selected source is copied into
     * app-private cache so C++ never has to retain a content URI permission. */
    public static boolean openSstvGallery() {
        Qk4Activity activity = instance;
        if (activity == null || !activity.beginSstvMediaOperation()) return false;
        activity.runOnUiThread(activity::launchSstvGallery);
        return true;
    }

    /** Opens the installed camera with a full-resolution FileProvider output. */
    public static boolean openSstvCamera() {
        Qk4Activity activity = instance;
        if (activity == null || !activity.beginSstvMediaOperation()) return false;
        activity.runOnUiThread(activity::launchSstvCamera);
        return true;
    }

    /** Shares only a completed app-private SSTV RX PNG through a temporary,
     * read-only FileProvider grant. No radio/profile data is exposed. */
    public static boolean shareSstvImage(String path, String title) {
        Qk4Activity activity = instance;
        if (activity == null || path == null || path.isEmpty()) return false;
        try {
            File image = new File(path).getCanonicalFile();
            File rxRoot = new File(activity.getFilesDir(), "sstv/rx").getCanonicalFile();
            String rootPrefix = rxRoot.getPath() + File.separator;
            if (!image.isFile() || !image.getPath().startsWith(rootPrefix)) return false;
            Uri uri = FileProvider.getUriForFile(activity,
                    activity.getPackageName() + ".qtprovider", image);
            Intent share = new Intent(Intent.ACTION_SEND);
            share.setType("image/png");
            share.putExtra(Intent.EXTRA_STREAM, uri);
            share.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            share.setClipData(ClipData.newRawUri("SSTV image", uri));
            Intent chooser = Intent.createChooser(share,
                    title == null || title.isEmpty() ? "Share SSTV image" : title);
            activity.runOnUiThread(() -> activity.startActivity(chooser));
            return true;
        } catch (Exception error) {
            return false;
        }
    }

    /** Consumes the next imported image. Empty means the operation is pending
     * or cancelled; the previous prepared SSTV image remains untouched. */
    public static String takeSstvImportedImagePath() {
        Qk4Activity activity = instance;
        if (activity == null) return "";
        synchronized (activity) {
            String path = activity.sstvImportedImagePath;
            activity.sstvImportedImagePath = null;
            return path == null ? "" : path;
        }
    }

    public static boolean isSstvMediaOperationActive() {
        Qk4Activity activity = instance;
        if (activity == null) return false;
        synchronized (activity) {
            return activity.sstvMediaOperationActive;
        }
    }

    public static String takeSstvMediaError() {
        Qk4Activity activity = instance;
        if (activity == null) return "";
        synchronized (activity) {
            String error = activity.sstvMediaError;
            activity.sstvMediaError = null;
            return error == null ? "" : error;
        }
    }

    private void launchSstvGallery() {
        Intent intent;
        if (Build.VERSION.SDK_INT >= 33) {
            intent = new Intent(MediaStore.ACTION_PICK_IMAGES);
        } else {
            intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        }
        intent.setType("image/*");
        try {
            startActivityForResult(intent, SSTV_GALLERY_REQUEST);
        } catch (RuntimeException error) {
            finishSstvMediaOperation("No Android photo picker is available.");
        }
    }

    private void launchSstvCamera() {
        if (Build.VERSION.SDK_INT >= 23
                && checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            sstvCameraPermissionPending = true;
            requestPermissions(new String[]{Manifest.permission.CAMERA}, SSTV_CAMERA_PERMISSION_REQUEST);
            return;
        }
        File destination = new File(getCacheDir(), "sstv-camera-" + System.currentTimeMillis() + ".jpg");
        Uri outputUri = FileProvider.getUriForFile(this, getPackageName() + ".qtprovider", destination);
        Intent intent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
        intent.putExtra(MediaStore.EXTRA_OUTPUT, outputUri);
        intent.addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_READ_URI_PERMISSION);
        intent.setClipData(ClipData.newRawUri("SSTV image", outputUri));
        sstvCameraPath = destination.getAbsolutePath();
        try {
            startActivityForResult(intent, SSTV_CAMERA_REQUEST);
        } catch (RuntimeException error) {
            sstvCameraPath = null;
            //noinspection ResultOfMethodCallIgnored
            destination.delete();
            finishSstvMediaOperation("No Android camera is available.");
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == SSTV_CAMERA_PERMISSION_REQUEST && sstvCameraPermissionPending) {
            sstvCameraPermissionPending = false;
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                launchSstvCamera();
            } else {
                finishSstvMediaOperation("Camera permission was not granted.");
            }
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == SSTV_GALLERY_REQUEST) {
            if (resultCode == Activity.RESULT_OK && data != null && data.getData() != null) {
                importSstvUri(data.getData());
            } else {
                finishSstvMediaOperation("");
            }
        } else if (requestCode == SSTV_CAMERA_REQUEST) {
            File image = sstvCameraPath == null ? null : new File(sstvCameraPath);
            if (resultCode == Activity.RESULT_OK && image != null && image.isFile() && image.length() > 0) {
                setSstvImportedImagePath(image.getAbsolutePath());
            } else if (image != null) {
                //noinspection ResultOfMethodCallIgnored
                image.delete();
                finishSstvMediaOperation("");
            } else {
                finishSstvMediaOperation("The camera did not return an image.");
            }
            sstvCameraPath = null;
        }
    }

    private void importSstvUri(Uri uri) {
        File destination = new File(getCacheDir(), "sstv-import-" + System.currentTimeMillis());
        try (InputStream input = getContentResolver().openInputStream(uri);
             OutputStream output = new FileOutputStream(destination)) {
            if (input == null) throw new IllegalStateException("No image stream");
            byte[] buffer = new byte[32 * 1024];
            int count;
            while ((count = input.read(buffer)) >= 0) {
                output.write(buffer, 0, count);
            }
            setSstvImportedImagePath(destination.getAbsolutePath());
        } catch (Exception error) {
            //noinspection ResultOfMethodCallIgnored
            destination.delete();
            finishSstvMediaOperation("The selected image could not be imported.");
        }
    }

    private synchronized void setSstvImportedImagePath(String path) {
        sstvImportedImagePath = path;
        sstvMediaOperationActive = false;
        sstvMediaError = null;
    }

    private synchronized boolean beginSstvMediaOperation() {
        if (sstvMediaOperationActive) return false;
        sstvMediaOperationActive = true;
        sstvMediaError = null;
        return true;
    }

    private synchronized void finishSstvMediaOperation(String error) {
        sstvMediaOperationActive = false;
        sstvMediaError = error == null ? "" : error;
    }

    private void requestKeepScreenOn() {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    private void cleanupAbandonedSstvMedia() {
        File[] files = getCacheDir().listFiles();
        if (files == null) return;
        for (File file : files) {
            String name = file.getName();
            if (!name.startsWith("sstv-camera-") && !name.startsWith("sstv-import-")) continue;
            String path = file.getAbsolutePath();
            if (path.equals(sstvCameraPath) || path.equals(sstvImportedImagePath)) continue;
            //noinspection ResultOfMethodCallIgnored
            file.delete();
        }
    }
}
