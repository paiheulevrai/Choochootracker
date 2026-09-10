package com.paiheulevrai.choochootracker;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import org.libsdl.app.SDLActivity;

public final class ChooChooTrackerActivity extends SDLActivity {
    private static final int OPEN_DOCUMENT = 4101;
    private static final int CREATE_DOCUMENT = 4102;
    private String importDirectory;
    private String exportPath;

    @Override protected String[] getLibraries() {
        return new String[] { "SDL2", "chipnomad" };
    }

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        hideSystemBars();
        // AssetManager lists nested directories reliably, unlike the native
        // asset API on a few Android builds. Never overwrite user files.
        seedWorkspace("choochootracker_data", getWorkspacePath());
    }

    private void seedWorkspace(String assetPath, String destinationPath) {
        try {
            String[] children = getAssets().list(assetPath);
            if (children != null && children.length > 0) {
                new File(destinationPath).mkdirs();
                for (String child : children) seedWorkspace(assetPath + "/" + child,
                    destinationPath + "/" + child);
                return;
            }
            File destination = new File(destinationPath);
            if (destination.exists()) return;
            File parent = destination.getParentFile();
            if (parent != null) parent.mkdirs();
            try (InputStream input = getAssets().open(assetPath);
                 FileOutputStream output = new FileOutputStream(destination)) {
                byte[] buffer = new byte[32768];
                for (int read; (read = input.read(buffer)) != -1;) output.write(buffer, 0, read);
            }
        } catch (Exception ignored) { }
    }

    private void hideSystemBars() {
        getWindow().getDecorView().setSystemUiVisibility(
            android.view.View.SYSTEM_UI_FLAG_FULLSCREEN |
            android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
            android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
            android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
            android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
            android.view.View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemBars();
    }

    public void saveDocument(String path, String mimeType, String suggestedName) {
        runOnUiThread(() -> {
            exportPath = path;
            Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType(mimeType);
            intent.putExtra(Intent.EXTRA_TITLE, suggestedName);
            startActivityForResult(intent, CREATE_DOCUMENT);
        });
    }

    // Called by the native layer. Files stay private to the app: no storage permission.
    public String getWorkspacePath() {
        File workspace = new File(getFilesDir(), "workspace");
        workspace.mkdirs();
        return workspace.getAbsolutePath();
    }

    public void openDocument(String mimeType, String relativeDirectory) {
        runOnUiThread(() -> {
            importDirectory = relativeDirectory;
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType(mimeType);
            startActivityForResult(intent, OPEN_DOCUMENT);
        });
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
        if (requestCode == CREATE_DOCUMENT) {
            try (InputStream input = new java.io.FileInputStream(exportPath);
                 java.io.OutputStream output = getContentResolver().openOutputStream(data.getData())) {
                byte[] buffer = new byte[32768];
                for (int read; output != null && (read = input.read(buffer)) != -1;) output.write(buffer, 0, read);
            } catch (Exception ignored) { }
            return;
        }
        if (requestCode != OPEN_DOCUMENT) return;
        Uri uri = data.getData();
        String name = "import";
        try (android.database.Cursor cursor = getContentResolver().query(uri, null, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int column = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (column >= 0) name = cursor.getString(column);
            }
        } catch (Exception ignored) { }
        name = name.replaceAll("[^A-Za-z0-9._ -]", "_");
        File destination = new File(new File(getWorkspacePath(), importDirectory), name);
        destination.getParentFile().mkdirs();
        try (InputStream input = getContentResolver().openInputStream(uri);
             FileOutputStream output = new FileOutputStream(destination)) {
            byte[] buffer = new byte[32768];
            for (int read; input != null && (read = input.read(buffer)) != -1;) output.write(buffer, 0, read);
        } catch (Exception ignored) { }
    }
}
