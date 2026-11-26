package com.strecomp.st;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;

import java.io.File;

/**
 * Runs the optimised game build (GameOptimizer) as a foreground service with a notification, so it keeps going while
 * the player is already playing the quick (TinyCC) build, or has left the app. The new module replaces the old one
 * when done; the next game start uses it.
 */
public class OptimizeService extends Service {
    private static final String CHANNEL = "optimise";
    private static final int ID = 7;
    private static volatile boolean running;
    /** The last result: null while none, "" when done, else the error. */
    static volatile String result;

    static void start(Context c) {
        if (running || !GameOptimizer.pending(c)) return;
        final Intent i = new Intent(c, OptimizeService.class);
        if (Build.VERSION.SDK_INT >= 26) c.startForegroundService(i);
        else c.startService(i);
    }

    static boolean running() { return running; }

    @Override
    public IBinder onBind(Intent intent) { return null; }

    private Notification note(String text, boolean ongoing, int progress) {
        final Notification.Builder b = Build.VERSION.SDK_INT >= 26 ? new Notification.Builder(this, CHANNEL)
                : new Notification.Builder(this);
        b.setSmallIcon(android.R.drawable.stat_sys_download).setContentTitle("Spirit Tracks").setContentText(text)
                .setOngoing(ongoing).setOnlyAlertOnce(true);
        if (progress >= 0) b.setProgress(100, progress, false);
        return b.build();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        final NotificationManager nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        if (Build.VERSION.SDK_INT >= 26)
            nm.createNotificationChannel(new NotificationChannel(CHANNEL, "Game build", NotificationManager.IMPORTANCE_LOW));
        final Notification first = note("Optimising the game for this phone…", true, 0);
        if (Build.VERSION.SDK_INT >= 29) startForeground(ID, first, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        else startForeground(ID, first);
        if (running) return START_NOT_STICKY;
        running = true;
        result = null;
        final Handler ui = new Handler(Looper.getMainLooper());
        final Runnable tick = new Runnable() {
            @Override
            public void run() {
                if (!running) return;
                final int p = Math.max(0, GameOptimizer.progress);
                nm.notify(ID, note("Optimising the game for this phone… " + p + "% (you can play meanwhile)", true, p));
                ui.postDelayed(this, 2000);
            }
        };
        ui.postDelayed(tick, 2000);
        new Thread(() -> {
            final String error = GameOptimizer.run(this, new File(getFilesDir(), "game/libst_game.so"));
            ui.post(() -> {
                running = false;
                result = error == null ? "" : error;
                stopForeground(true);
                nm.notify(ID, note(error == null ? "The optimised game is ready: it runs from the next game start."
                        : "Optimising failed: " + error, false, -1));
                stopSelf();
            });
        }).start();
        return START_NOT_STICKY;
    }
}
