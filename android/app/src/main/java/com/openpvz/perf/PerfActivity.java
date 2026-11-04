package com.openpvz.perf;

import android.app.NativeActivity;
import android.app.Dialog;
import android.os.Bundle;
import android.os.Build;
import android.content.Context;
import android.content.res.Configuration;
import android.hardware.input.InputManager;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import java.nio.charset.StandardCharsets;

/** Keeps NativeActivity's input view eligible for explicit keyboard requests. */
public final class PerfActivity extends NativeActivity implements InputManager.InputDeviceListener {
    // NativeActivity's native dlopen does not register JNI lookup with ART's
    // application ClassLoader. Associate this library before Java callbacks run.
    static { System.loadLibrary("pvz_perf"); }
    private LinearLayout inputBar;
    private Dialog inputDialog;
    private EditText textInput;
    private View nativeInput;
    private final int[] surfaceOrigin = new int[2];
    private long inputSession;
    private boolean replacingText;
    // The game editor and its bitmap fonts currently accept printable ASCII.
    private final InputFilter gameCharacters = (source, start, end, dest, dstart, dend) -> {
        StringBuilder accepted = null;
        for (int i = start; i < end; ++i) {
            char ch = source.charAt(i);
            if (ch < 32 || ch > 126) {
                if (accepted == null) accepted = new StringBuilder(source.subSequence(start, i));
            } else if (accepted != null) accepted.append(ch);
        }
        return accepted == null ? null : accepted.toString();
    };
    private static native void nativeTextChanged(long session, byte[] text, boolean done);
    private static native void nativeSurfaceOriginChanged(int left, int top);

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        if (BuildConfig.HDR_DEVICE_TEST && Build.VERSION.SDK_INT >= 26) {
            getWindow().setColorMode(android.content.pm.ActivityInfo.COLOR_MODE_HDR);
        }
        // Leave the Window surface to Android so its caption can be drawn.
        // NativeActivity's public holder callbacks still feed native_app_glue.
        getWindow().takeSurface(null);
        getWindow().setFormat(PixelFormat.OPAQUE);
        // NativeActivity overwrites the manifest's soft-input mode in onCreate.
        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_HIDDEN
                | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING);
        ViewGroup content = findViewById(android.R.id.content);
        content.getChildAt(0).getViewTreeObserver().removeOnGlobalLayoutListener(this);
        SurfaceView gameSurface = new SurfaceView(this);
        gameSurface.getHolder().addCallback(this);
        setContentView(gameSurface);
        nativeInput = gameSurface;
        gameSurface.getViewTreeObserver().addOnGlobalLayoutListener(() -> {
            gameSurface.getLocationInWindow(surfaceOrigin);
            nativeSurfaceOriginChanged(surfaceOrigin[0], surfaceOrigin[1]);
        });
        nativeInput.setFocusableInTouchMode(true);
        nativeInput.requestFocus();
        updateWindowDecor();
        ((InputManager)getSystemService(Context.INPUT_SERVICE)).registerInputDeviceListener(this, null);

        inputBar = new LinearLayout(this);
        inputBar.setGravity(Gravity.CENTER_VERTICAL);
        inputBar.setPadding(dp(12), dp(6), dp(12), dp(6));
        inputBar.setBackgroundColor(Color.rgb(245, 245, 245));
        textInput = new EditText(this);
        textInput.setSingleLine(true);
        textInput.setTextColor(Color.BLACK);
        textInput.setImeOptions(EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        inputBar.addView(textInput, new LinearLayout.LayoutParams(0, dp(48), 1));
        Button done = new Button(this);
        done.setText(android.R.string.ok);
        done.setOnClickListener(view -> finishInput());
        inputBar.addView(done);
        // Resize the editor's own window for the IME without changing the
        // game's SurfaceView or its pointer mapping.
        inputDialog = new Dialog(this);
        inputDialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        inputDialog.setContentView(inputBar);
        inputDialog.setCanceledOnTouchOutside(true);
        inputDialog.getWindow().setGravity(Gravity.BOTTOM);
        inputDialog.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        textInput.addTextChangedListener(new TextWatcher() {
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                if (!replacingText && inputDialog.isShowing())
                    nativeTextChanged(inputSession, s.toString().getBytes(StandardCharsets.UTF_8), false);
            }
            public void afterTextChanged(Editable text) {}
        });
        textInput.setOnEditorActionListener((view, action, event) -> {
            if (action == EditorInfo.IME_ACTION_DONE || (event != null
                    && event.getKeyCode() == android.view.KeyEvent.KEYCODE_ENTER)) {
                if (event == null || event.getAction() == android.view.KeyEvent.ACTION_UP) finishInput();
                return true;
            }
            return false;
        });
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private void finishInput() {
        nativeTextChanged(inputSession, textInput.getText().toString().getBytes(StandardCharsets.UTF_8), true);
        dismissInput();
    }

    private void dismissInput() {
        ((InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE))
                .hideSoftInputFromWindow(textInput.getWindowToken(), 0);
        inputDialog.dismiss();
        nativeInput.requestFocus();
    }

    // Called from the native game thread; all Android View work stays on the UI thread.
    public void showTextInput(long session, byte[] utf8, int cursor, int hilite, int maxChars, boolean password) {
        final String text = new String(utf8, StandardCharsets.UTF_8);
        runOnUiThread(() -> {
            if (session < inputSession || isFinishing() || isDestroyed()) return;
            boolean opening = !inputDialog.isShowing() || inputSession != session;
            inputSession = session;
            replacingText = true;
            textInput.setInputType(InputType.TYPE_CLASS_TEXT | (password
                    ? InputType.TYPE_TEXT_VARIATION_PASSWORD : InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS));
            textInput.setFilters(maxChars >= 0
                    ? new InputFilter[]{gameCharacters, new InputFilter.LengthFilter(maxChars)}
                    : new InputFilter[]{gameCharacters});
            if (!textInput.getText().toString().equals(text)) textInput.setText(text);
            if (opening) {
                int end = Math.min(Math.max(cursor, 0), textInput.length());
                int start = hilite < 0 ? end : Math.min(hilite, textInput.length());
                textInput.setSelection(Math.min(start, end), Math.max(start, end));
            }
            replacingText = false;
            textInput.setShowSoftInputOnFocus(!hasPhysicalKeyboard());
            inputDialog.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE
                    | WindowManager.LayoutParams.SOFT_INPUT_STATE_HIDDEN);
            if (!inputDialog.isShowing()) inputDialog.show();
            inputDialog.getWindow().setLayout(ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT);
            textInput.requestFocus();
            textInput.post(() -> { if (inputSession == session) updateKeyboard(); });
        });
    }

    public void hideTextInput(long session) {
        runOnUiThread(() -> {
            if (session < inputSession) return;
            inputSession = session;
            dismissInput();
        });
    }

    private boolean hasPhysicalKeyboard() {
        Configuration config = getResources().getConfiguration();
        if (config.keyboard == Configuration.KEYBOARD_QWERTY
                && config.hardKeyboardHidden == Configuration.HARDKEYBOARDHIDDEN_NO) return true;
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice device = InputDevice.getDevice(id);
            if (device != null && !device.isVirtual()
                    && device.getKeyboardType() == InputDevice.KEYBOARD_TYPE_ALPHABETIC
                    && device.supportsSource(InputDevice.SOURCE_KEYBOARD)) return true;
        }
        return false;
    }

    private void updateKeyboard() {
        if (textInput == null) return;
        boolean physical = hasPhysicalKeyboard();
        textInput.setShowSoftInputOnFocus(!physical);
        if (!inputDialog.isShowing()) return;
        InputMethodManager ime = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
        if (physical) ime.hideSoftInputFromWindow(textInput.getWindowToken(), 0);
        else ime.showSoftInput(textInput, 0);
    }

    public void onInputDeviceAdded(int id) { updateKeyboard(); }
    public void onInputDeviceRemoved(int id) { updateKeyboard(); }
    public void onInputDeviceChanged(int id) { updateKeyboard(); }

    @Override
    public void onWindowFocusChanged(boolean focused) {
        // The owned editor temporarily owns window focus, not another app.
        if (!focused && inputDialog != null && inputDialog.isShowing()) return;
        super.onWindowFocusChanged(focused);
    }

    @Override
    protected void onPause() {
        if (inputDialog != null && inputDialog.isShowing()) {
            dismissInput();
            super.onWindowFocusChanged(false);
        }
        super.onPause();
    }

    @Override
    public void onConfigurationChanged(Configuration config) {
        super.onConfigurationChanged(config);
        updateWindowDecor();
        updateKeyboard();
    }

    private void updateWindowDecor() {
        final boolean multiWindow = isInMultiWindowMode();
        getWindow().setNavigationBarColor(Color.TRANSPARENT);
        if (Build.VERSION.SDK_INT >= 29) getWindow().setNavigationBarContrastEnforced(false);
        // Fullscreen draws beneath the transparent navigation buttons. A freeform
        // window keeps the system's content/caption layout instead.
        if (Build.VERSION.SDK_INT >= 30) getWindow().setDecorFitsSystemWindows(multiWindow);
        if (multiWindow) {
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            View decor = getWindow().getDecorView();
            decor.setSystemUiVisibility(decor.getSystemUiVisibility() & ~(View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE));
        } else {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            View decor = getWindow().getDecorView();
            decor.setSystemUiVisibility((decor.getSystemUiVisibility() & ~(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_IMMERSIVE | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY))
                    | View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        }
    }

    @Override
    public void onMultiWindowModeChanged(boolean multiWindow) {
        super.onMultiWindowModeChanged(multiWindow);
        updateWindowDecor();
    }

    @Override
    public void onMultiWindowModeChanged(boolean multiWindow, Configuration config) {
        if (Build.VERSION.SDK_INT >= 26) super.onMultiWindowModeChanged(multiWindow, config);
        updateWindowDecor();
    }

    @Override
    protected void onDestroy() {
        ((InputManager)getSystemService(Context.INPUT_SERVICE)).unregisterInputDeviceListener(this);
        super.onDestroy();
        // NativeActivity.onDestroy waits for native_app_glue to finish. At
        // this point the game has saved and released audio/GPU resources.
        // Explicit Quit must not leave an empty cached game process behind.
        if (isFinishing() && !isChangingConfigurations()) {
            android.os.Process.killProcess(android.os.Process.myPid());
        }
    }
}
