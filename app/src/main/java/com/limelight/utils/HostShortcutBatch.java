package com.limelight.utils;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.AlertDialog;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.widget.Toast;

import com.limelight.R;
import com.limelight.binding.PlatformBinding;
import com.limelight.nvstream.http.ComputerDetails;
import com.limelight.nvstream.http.NvApp;
import com.limelight.nvstream.http.NvHTTP;

import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Keeps only one launcher pin request pending at a time. Owned by the host activity. */
@TargetApi(Build.VERSION_CODES.O)
public final class HostShortcutBatch {
    private final Activity activity;
    private final ShortcutHelper shortcuts;
    private final ComputerDetails computer;
    private final String uniqueId;
    private NvHTTP http;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final List<NvApp> apps = new ArrayList<>();
    private AlertDialog dialog;
    private volatile boolean closed;
    private int index;
    private Bitmap icon;
    private boolean ready;
    private boolean requested;

    public HostShortcutBatch(Activity activity, ShortcutHelper shortcuts,
                             ComputerDetails computer, String uniqueId) {
        this.activity = activity;
        this.shortcuts = shortcuts;
        this.computer = computer;
        this.uniqueId = uniqueId;
    }

    public void start() {
        dialog = new AlertDialog.Builder(activity)
                .setTitle(R.string.pcview_menu_all_shortcuts)
                .setMessage(R.string.shortcuts_loading)
                .setPositiveButton(R.string.shortcuts_retry, null)
                .setNeutralButton(R.string.shortcuts_skip, null)
                .setNegativeButton(android.R.string.cancel, (d, which) -> close())
                .setOnCancelListener(d -> close())
                .create();
        dialog.show();
        dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v -> {
            requested = false;
            requestCurrent();
        });
        dialog.getButton(AlertDialog.BUTTON_NEUTRAL).setOnClickListener(v -> {
            index++;
            prepareNext();
        });
        setButtons(false);
        worker.execute(() -> {
            try {
                http = new NvHTTP(ServerHelper.getCurrentAddressFromComputer(computer),
                        computer.httpsPort, uniqueId, computer.serverCert,
                        PlatformBinding.getCryptoProvider(activity));
                // Fetch the complete host list, including apps hidden in Iris.
                List<NvApp> fetched = http.getAppList();
                handler.post(() -> {
                    if (closed) return;
                    apps.addAll(fetched);
                    prepareNext();
                    handler.post(checkProgress);
                });
            } catch (Exception e) {
                handler.post(() -> {
                    if (closed) return;
                    Toast.makeText(activity, R.string.shortcuts_load_failed, Toast.LENGTH_LONG).show();
                    close();
                });
            }
        });
    }

    private void setButtons(boolean enabled) {
        dialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(enabled);
        dialog.getButton(AlertDialog.BUTTON_NEUTRAL).setEnabled(enabled);
    }

    private void prepareNext() {
        ready = false;
        requested = false;
        icon = null;
        setButtons(false);
        while (index < apps.size() && shortcuts.isGameShortcutPinned(computer, apps.get(index))) {
            index++;
        }
        if (index == apps.size()) {
            Toast.makeText(activity, apps.isEmpty() ? R.string.shortcuts_no_apps :
                    R.string.shortcuts_finished, Toast.LENGTH_LONG).show();
            close();
            return;
        }
        dialog.setMessage(activity.getString(R.string.shortcuts_progress,
                index + 1, apps.size(), apps.get(index).getAppName()));
        NvApp app = apps.get(index);
        worker.execute(() -> {
            Bitmap artwork = null;
            try (InputStream stream = http.getBoxArt(app)) {
                BitmapFactory.Options options = new BitmapFactory.Options();
                options.inSampleSize = 4;
                artwork = BitmapFactory.decodeStream(stream, null, options);
            } catch (Exception ignored) {
                // A missing cover must not prevent creating a shortcut.
            }
            final Bitmap loadedIcon = artwork;
            handler.post(() -> {
                if (closed) return;
                icon = loadedIcon;
                ready = true;
                setButtons(true);
                requestCurrent();
            });
        });
    }

    private void requestCurrent() {
        if (closed || !ready || requested || !dialog.getWindow().getDecorView().hasWindowFocus()) return;
        try {
            requested = shortcuts.createPinnedGameShortcut(computer, apps.get(index), icon);
            if (!requested) {
                Toast.makeText(activity, R.string.unable_to_pin_shortcut, Toast.LENGTH_LONG).show();
                close();
            }
        } catch (IllegalStateException e) {
            // The launcher may still be closing its previous prompt. Retry when we regain focus.
        }
    }

    private final Runnable checkProgress = new Runnable() {
        @Override
        public void run() {
            if (closed) return;
            // Android sends no rejection callback. Keep Retry/Skip available after dismissal.
            if (ready && dialog.getWindow().getDecorView().hasWindowFocus()) {
                if (requested && shortcuts.isGameShortcutPinned(computer, apps.get(index))) {
                    index++;
                    prepareNext();
                } else {
                    requestCurrent();
                }
            }
            if (!closed) handler.postDelayed(this, 500);
        }
    };

    public void close() {
        closed = true;
        handler.removeCallbacksAndMessages(null);
        worker.shutdownNow();
        icon = null;
        if (dialog != null) dialog.dismiss();
    }
}
