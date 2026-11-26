package com.strecomp.st;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.MessageDigest;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * The start screen: bring in your own Spirit Tracks ROM (and, if you like, a save from another emulator), then
 * play. The ROM and the save are copied into the app's own storage (files/rom/bkie.nds, files/saves/bkie.sav); the
 * originals are only read. A save is never overwritten without a dated backup next to it.
 */
public final class LauncherActivity extends Activity {
    private static final int PICK_ROM = 1, PICK_SAVE = 2, EXPORT_SAVE = 3, PICK_MODULE = 4;
    // The dumps that share one recompiled game (common/src/known_roms.cpp). USA: the clean dump and the D-pad and
    // D-pad + QoL patches made from it. Europe: Revision 0, or Revision 1 (several dumps); the two revisions differ in
    // code, so each is its own build. The app takes the group of the ROM it was built from (BuildConfig.ROM_SHA1).
    private static final String[] USA = {
            "1f0921b5e99db9d92d24710f1b004e4f3c997b73",  // clean (USA, Australia) (En,Fr,Es)
            "6a63d88702c34da626e273dfca4931d52ac4551b",  // D-pad patch v0.9c
            "6896c1db2b3a725cb991f711a875fc2228b7620e",  // D-pad + QoL patch v0.9c
    };
    private static final String[] EUROPE = {
            "9e99cc803a14ce038eb908db585431f8254f09ee",  // (Europe) (En,Fr,De,Es,It)
    };
    private static final String[] EUROPE_REV1 = {
            "dcb221a89acb894503a461c2c7d4cfdad9a06b68",  // (Europe) (En,Fr,De,Es,It) (Rev 1), the dumps zeldaret/st lists
            "b9096f46e9163750a9e3f6c2fc0758e6094b6135",
            "b0288a880498c12508e53f4373085eb88955e14b",
            "4efbff9a263fe622f3f7e7cfb7bea8046b86e57d",
            "52d7f22255d5d86fc40d3a43c7de969e0365ff7d",
    };
    private static final String[] KNOWN = known();

    private static String[] known() {
        for (String[] group : new String[][] {EUROPE, EUROPE_REV1})
            for (String k : group)
                if (k.equals(BuildConfig.ROM_SHA1)) return group;
        return USA;
    }

    /** "USA" or "Europe": the release this APK plays. */
    private static String release() { return KNOWN == USA ? "USA: clean, D-pad patch, or D-pad + QoL patch v0.9c"
            : KNOWN == EUROPE ? "Europe, Revision 0" : "Europe, Revision 1"; }
    private static final int SAVE_SIZE = 1024 * 1024;  // the cartridge's 8 Mbit flash


    private File rom() { return new File(getFilesDir(), "rom/bkie.nds"); }

    /** The recompiled game, for a shell APK (BuildConfig.SHELL): frontend main.cpp loads it from here when present. */
    private File module() { return new File(getFilesDir(), "game/libst_game.so"); }

    private static final String MODULE_HELP = "The game is built on this phone from your ROM: nothing from the game is "
            + "downloaded. It takes about a minute, once.";

    /** Which app install the game module was built by (lastUpdateTime:versionCode); a newer app rebuilds it. */
    private File moduleStamp() { return new File(getFilesDir(), "game/built-for.txt"); }

    private String appStamp() {
        try {
            return getPackageManager().getPackageInfo(getPackageName(), 0).lastUpdateTime + ":" + BuildConfig.VERSION_CODE;
        } catch (android.content.pm.PackageManager.NameNotFoundException e) {
            return "";
        }
    }

    private void stampModule() {
        try (FileOutputStream out = new FileOutputStream(moduleStamp())) {
            out.write(appStamp().getBytes("UTF-8"));
        } catch (IOException ignored) {
            // (no stamp: the module is taken as current)
        }
    }

    /** True when the app was updated since the game was built (the new engine needs the game built again). A module
     *  without a stamp (copied in by the PC scripts, or built before stamps) is taken as current. */
    private boolean moduleStale() {
        if (!BuildConfig.SHELL || !module().isFile() || !moduleStamp().isFile()) return false;
        try (BufferedReader r = new BufferedReader(new FileReader(moduleStamp()))) {
            final String line = r.readLine();
            return line == null || !line.trim().equals(appStamp());
        } catch (IOException e) {
            return false;
        }
    }

    private boolean building;

    private File save() { return new File(getFilesDir(), "saves/bkie.sav"); }

    /** The resume state the game writes when it quits or goes to the background (frontend main.cpp). */
    private File resume(String ext) { return new File(getFilesDir(), "states/resume." + ext); }

    /** Intent extra from the game (GameActivity.restartWithResume): continue it again at once, in a fresh process. */
    static final String EXTRA_RELAUNCH = "relaunch";

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
    }

    /** Continue, once the old game process has exited (SDL's native side starts once per process), or after 4 s. */
    private void relaunchWhenGameGone(long until) {
        boolean alive = false;
        final android.app.ActivityManager am = (android.app.ActivityManager) getSystemService(ACTIVITY_SERVICE);
        final java.util.List<android.app.ActivityManager.RunningAppProcessInfo> procs = am.getRunningAppProcesses();
        if (procs != null)
            for (android.app.ActivityManager.RunningAppProcessInfo p : procs)
                if (p.processName.equals(getPackageName() + ":game")) alive = true;
        if (alive && System.currentTimeMillis() < until) {
            getWindow().getDecorView().postDelayed(() -> relaunchWhenGameGone(until), 100);
            return;
        }
        startActivity(new Intent(this, GameActivity.class).putExtra(GameActivity.EXTRA_CONTINUE, true));
    }

    // ---- per game ----
    private static final String TITLE = "The Legend of Zelda\nSpirit Tracks";
    private static final String SUBTITLE = "Nintendo DS · native Android port";
    private static final String ROM_HELP = "Choose your Spirit Tracks ROM (" + release() + "), the release this app "
            + "was built for (the PC scripts copy it in for you). Nothing from the game is downloaded. A save from melonDS (.sav) "
            + "can be imported afterwards under Save file.";
    private static final String SAVE_NAME = "SpiritTracks.sav";
    private static final boolean GPU_DEFAULT = true;   // GameActivity: prefs.getBoolean("gpu", ...)

    private LauncherLook look;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        look = new LauncherLook(this);
        setContentView(look.root);
        refresh();
    }

    @Override
    protected void onResume() {
        super.onResume();
        look.hideSystemBars();
        refresh();
        UpdateCheck.maybeCheck(this, prefs(), this::refresh);
        if (BuildConfig.SHELL && !building && module().isFile()) OptimizeService.start(this);  // (cut short before)
        if (getIntent() != null && getIntent().getBooleanExtra(EXTRA_RELAUNCH, false)) {
            getIntent().removeExtra(EXTRA_RELAUNCH);
            if (resume("state").isFile()) relaunchWhenGameGone(System.currentTimeMillis() + 4000);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) look.hideSystemBars();
    }

    @Override
    public boolean dispatchKeyEvent(android.view.KeyEvent e) {
        return look.onKey(e) || super.dispatchKeyEvent(e);
    }

    private SharedPreferences prefs() { return getSharedPreferences(DsPad.PREFS, MODE_PRIVATE); }

    /** Builds the start screen (LauncherLook: the KH Birth by Sleep port's style). */
    private void refresh() {
        final LinearLayout column = look.begin(TITLE, SUBTITLE, new LauncherArt());
        if (!rom().isFile()) {
            column.addView(look.text("First-time setup", 20, Color.WHITE, true));
            final TextView info = look.text(ROM_HELP, 15, 0xFFDDDDDD, false);
            info.setPadding(0, look.dp(8), 0, look.dp(20));
            column.addView(info);
            look.button(column, "Choose game ROM (.nds)…", null, () -> pick(PICK_ROM)).requestFocus();
            return;
        }
        if (BuildConfig.SHELL && !module().isFile()) {
            column.addView(look.text("One more step: build the game", 20, Color.WHITE, true));
            final TextView info = look.text(MODULE_HELP, 15, 0xFFDDDDDD, false);
            info.setPadding(0, look.dp(8), 0, look.dp(20));
            column.addView(info);
            look.button(column, "Build the game", null, () -> buildGame(false)).requestFocus();
            look.button(column, "Use a game module built on a PC…", "libst_game.so", () -> pick(PICK_MODULE));
            return;
        }
        if (moduleStale() && !building) {
            // The app was updated: build the game again for the new version first (the ROM is kept; saves stay).
            column.post(() -> buildGame(true));
        }
        final View first;
        if (resume("state").isFile()) {
            first = look.button(column, "Continue", "Where you last quit · " + resumeTime(), () ->
                    startActivity(new Intent(this, GameActivity.class).putExtra(GameActivity.EXTRA_CONTINUE, true)));
            look.button(column, "Start game", "From the title screen (in-game saves)",
                    () -> startActivity(new Intent(this, GameActivity.class)));
        } else {
            first = look.button(column, "Start game", "From the title screen",
                    () -> startActivity(new Intent(this, GameActivity.class)));
        }
        final String update = UpdateCheck.available(prefs());
        if (update != null)
            look.button(column, "Update to " + update, "A new version is out: download it from GitHub (saves stay)",
                    () -> UpdateCheck.install(this, prefs()));
        final LinearLayout row = look.row(column);
        look.small(row, "Save file", this::saveFile);
        look.small(row, "Settings", () -> look.settings(prefs(), GPU_DEFAULT, this::refresh));
        look.small(row, "Game ROM", this::gameRom);
        look.note(column, LauncherLook.summary(prefs(), GPU_DEFAULT) + "  ·  Save states: in the game's ≡ menu");
        first.requestFocus();
    }

    private void saveFile() {
        final String when = save().isFile() ? "Last saved: " + new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US)
                .format(new Date(save().lastModified())) : "No save yet (the game makes one when you save in it).";
        look.dialog().setTitle("Save file").setMessage(when + "\n\nImport a save from melonDS (.sav): the save "
                        + "already here is kept as a dated backup. Export copies the save out.")
                .setPositiveButton("Import…", (d, w) -> pick(PICK_SAVE))
                .setNeutralButton("Export…", (d, w) -> {
                    final Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                    i.addCategory(Intent.CATEGORY_OPENABLE);
                    i.setType("application/octet-stream");
                    i.putExtra(Intent.EXTRA_TITLE, SAVE_NAME);
                    startActivityForResult(i, EXPORT_SAVE);
                })
                .setNegativeButton("Back", null).show();
    }

    private void gameRom() {
        final AlertDialog.Builder b = look.dialog().setTitle("Game ROM").setMessage("Ready: " + rom().getName() + " ("
                        + (rom().length() >> 20) + " MB)\n\nChoose the ROM again to replace it. Saves and save "
                        + "states stay." + (BuildConfig.SHELL ? "\n\nGame module: " + (module().isFile()
                        ? (module().length() >> 20) + " MB, " + new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US)
                        .format(new Date(module().lastModified())) : "none") + ". Rebuild it after an app update."
                        + optimisedText() : ""))
                .setPositiveButton("Choose ROM…", (d, w) -> pick(PICK_ROM))
                .setNegativeButton("Back", null);
        if (BuildConfig.SHELL) b.setNeutralButton("Rebuild game", (d, w) -> buildGame(false));
        b.show();
    }

    /** The optimised build's state, for the Game ROM dialog (GameOptimizer). */
    private String optimisedText() {
        if (!GameOptimizer.available(this)) return "";
        if (OptimizeService.running()) return "\n\nOptimising: " + Math.max(0, GameOptimizer.progress) + "%";
        final File done = new File(getFilesDir(), "game/optimised.txt");
        if (done.isFile()) {
            try (BufferedReader r = new BufferedReader(new FileReader(done))) {
                return "\n\nOptimised (" + r.readLine() + ")";
            } catch (IOException ignored) {
                // (fall through)
            }
        }
        final String err = OptimizeService.result;
        return err != null && !err.isEmpty() ? "\n\nOptimising failed: " + err : "\n\nNot optimised yet (quick build).";
    }

    /** When the resume state was written ("MM-DD HH:MM", from resume.txt). */
    private String resumeTime() {
        try (BufferedReader r = new BufferedReader(new FileReader(resume("txt")))) {
            final String line = r.readLine();
            if (line != null && !line.isEmpty()) return line.trim();
        } catch (IOException ignored) {
            // (no time: the file's date instead)
        }
        return new SimpleDateFormat("MM-dd HH:mm", Locale.US).format(new Date(resume("state").lastModified()));
    }


    private void pick(int request) {
        final Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        startActivityForResult(i, request);
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (result != RESULT_OK || data == null || data.getData() == null) return;
        final Uri uri = data.getData();
        if (request == LauncherLook.REQUEST_BACKGROUND) {
            final String error = look.importBackground(uri);
            refresh();
            if (error != null) look.dialog().setMessage(error).setPositiveButton("OK", null).show();
            return;
        }
        // Large copies off the UI thread.
        new Thread(() -> {
            String message;
            try {
                if (request == PICK_ROM) message = importRom(uri);
                else if (request == PICK_MODULE) message = importModule(uri);
                else if (request == PICK_SAVE) message = importSave(uri);
                else message = exportSave(uri);
            } catch (IOException e) {
                message = "That did not work: " + e.getMessage();
            }
            final String m = message;
            final boolean build = BuildConfig.SHELL && request == PICK_ROM && m.startsWith("Game ROM ready");
            runOnUiThread(() -> {
                refresh();
                if (build) buildGame(false);  // (a new ROM needs its own game)
                else new AlertDialog.Builder(this).setMessage(m).setPositiveButton("OK", null).show();
            });
        }).start();
    }

    /** Shell APK: builds files/game/libst_game.so from the ROM on the phone (GameBuilder), with a progress dialog. */
    private void buildGame(boolean afterUpdate) {
        if (building) return;
        building = true;
        final AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle(afterUpdate ? "App updated: building the game again" : "Building the game")
                .setMessage("Recompiling your ROM…").setCancelable(false).show();
        final long start = System.currentTimeMillis();
        final Runnable tick = new Runnable() {
            @Override
            public void run() {
                if (!dialog.isShowing()) return;
                final int p = GameBuilder.progress();
                dialog.setMessage((p < 20 ? "Recompiling your ROM…" : "Compiling the game…") + " " + p + "%");
                getWindow().getDecorView().postDelayed(this, 300);
            }
        };
        new Thread(() -> {
            //noinspection ResultOfMethodCallIgnored
            module().delete();
            //noinspection ResultOfMethodCallIgnored
            new File(getFilesDir(), "game/optimised.txt").delete();
            runOnUiThread(tick);
            final String error = GameBuilder.run(this, rom(), module());
            if (error == null) stampModule();
            final long secs = (System.currentTimeMillis() - start) / 1000;
            android.util.Log.i("STOpt", "quick build (TinyCC) " + (error == null ? "done" : "failed") + " in "
                    + (System.currentTimeMillis() - start) + " ms");
            runOnUiThread(() -> {
                building = false;
                dialog.dismiss();
                refresh();
                if (error == null) {
                    if (Build.VERSION.SDK_INT >= 33)
                        requestPermissions(new String[] {"android.permission.POST_NOTIFICATIONS"}, 3);
                    OptimizeService.start(this);
                }
                new AlertDialog.Builder(this).setMessage(error == null
                                ? "The game is ready (built in " + secs + " s). Press Start game." : error)
                        .setPositiveButton("OK", null).show();
            });
        }).start();
    }

    private String importRom(Uri uri) throws IOException {
        final File dest = rom(), tmp = new File(dest.getPath() + ".tmp");
        //noinspection ResultOfMethodCallIgnored
        dest.getParentFile().mkdirs();
        final String sha1 = copy(uri, tmp);
        for (String k : KNOWN) {
            if (k.equals(sha1)) {
                if (dest.exists() && !dest.delete()) throw new IOException("cannot replace the ROM");
                if (!tmp.renameTo(dest)) throw new IOException("cannot store the ROM");
                return "Game ROM ready. Press Play.";
            }
        }
        //noinspection ResultOfMethodCallIgnored
        tmp.delete();
        return "That file is not a Spirit Tracks dump this app was built for (" + release() + ")."
                + "\nThis file's SHA-1: " + sha1;
    }

    /** A shell APK's game: an arm64 ELF shared library. Which ROM it was built from is checked when the game starts. */
    private String importModule(Uri uri) throws IOException {
        final File dest = module(), tmp = new File(dest.getPath() + ".tmp");
        //noinspection ResultOfMethodCallIgnored
        dest.getParentFile().mkdirs();
        copy(uri, tmp);
        final byte[] h = new byte[20];
        try (InputStream in = new FileInputStream(tmp)) {
            if (in.read(h) != h.length) h[0] = 0;
        }
        // ELF magic, 64-bit (EI_CLASS 2), little-endian, shared object (e_type 3), AArch64 (e_machine 183).
        if (!(h[0] == 0x7f && h[1] == 'E' && h[2] == 'L' && h[3] == 'F' && h[4] == 2 && h[5] == 1 && h[16] == 3
                && (h[18] & 0xff) == 183 && h[19] == 0)) {
            //noinspection ResultOfMethodCallIgnored
            tmp.delete();
            return "That file is not a game module: choose the libst_game.so the PC build made for Android (arm64).";
        }
        if (dest.exists() && !dest.delete()) throw new IOException("cannot replace the game module");
        if (!tmp.renameTo(dest)) throw new IOException("cannot store the game module");
        stampModule();
        return "Game module ready (" + (dest.length() >> 20) + " MB). Press Start game.";
    }

    private String importSave(Uri uri) throws IOException {
        final File dest = save(), tmp = new File(dest.getPath() + ".tmp");
        //noinspection ResultOfMethodCallIgnored
        dest.getParentFile().mkdirs();
        copy(uri, tmp);
        if (tmp.length() != SAVE_SIZE) {
            final long n = tmp.length();
            //noinspection ResultOfMethodCallIgnored
            tmp.delete();
            return "That save is " + n + " bytes; a Spirit Tracks save is " + SAVE_SIZE + " bytes (melonDS's .sav).";
        }
        if (dest.isFile()) {
            final String stamp = new SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(new Date());
            if (!dest.renameTo(new File(dest.getPath() + ".bak-" + stamp))) throw new IOException("cannot back up the old save");
        }
        if (!tmp.renameTo(dest)) throw new IOException("cannot store the save");
        return "Save imported (the previous one, if any, is kept as a backup).";
    }

    private String exportSave(Uri uri) throws IOException {
        if (!save().isFile()) return "There is no save yet.";
        try (InputStream in = new FileInputStream(save()); OutputStream out = getContentResolver().openOutputStream(uri)) {
            if (out == null) throw new IOException("cannot write there");
            final byte[] buf = new byte[1 << 16];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        }
        return "Save exported.";
    }

    /** Copies a picked file to `to` and returns its SHA-1 (lowercase hex). */
    private String copy(Uri uri, File to) throws IOException {
        try (InputStream in = getContentResolver().openInputStream(uri); OutputStream out = new FileOutputStream(to)) {
            if (in == null) throw new IOException("cannot open the file");
            final MessageDigest sha = MessageDigest.getInstance("SHA-1");
            final byte[] buf = new byte[1 << 16];
            int n;
            while ((n = in.read(buf)) > 0) {
                out.write(buf, 0, n);
                sha.update(buf, 0, n);
            }
            final StringBuilder hex = new StringBuilder();
            for (byte b : sha.digest()) hex.append(String.format(Locale.US, "%02x", b));
            return hex.toString();
        } catch (java.security.NoSuchAlgorithmException e) {
            throw new IOException(e);
        }
    }
}
