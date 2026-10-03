package fi.wekan.wena;

import android.content.Intent;

import org.libsdl.app.SDLActivity;

/**
 * Wena's desktop on Android. SDLActivity (from the pinned SDL2 source) runs
 * SDL_main, which is client/desktop.c's main, from libmain.so; SDL2 and SQLite
 * are linked into that one library, so there is no libSDL2.so to load.
 */
public class WenaActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }

    /**
     * There is no command line on Android. The one argument a launcher can
     * give is --smoke, for a test: render three frames with editor writes
     * disabled and exit (adb shell am start -n fi.wekan.wena/.WenaActivity
     * --ez smoke true; logcat then says "Wena desktop smoke passed").
     */
    @Override
    protected String[] getArguments() {
        Intent intent = getIntent();
        if (intent != null && intent.getBooleanExtra("smoke", false)) {
            return new String[] { "--smoke" };
        }
        return new String[0];
    }
}
