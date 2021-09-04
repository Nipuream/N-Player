package com.nipuream.n_player;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class NPlayer extends SurfaceView implements Runnable, SurfaceHolder.Callback {

    private static final String TAG = "NPlayer";

    public NPlayer(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void run() {
        open("/sdcard/1080.mp4", getHolder().getSurface());
    }


    @Override
    public void surfaceCreated(SurfaceHolder holder){
        Log.i(TAG, "surface create ...");
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width,
                               int height){
        Log.i(TAG, "surface changed...");
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder){
        Log.i(TAG, "surface destroy ...");
    }


    public native void open(String url, Object surface);

}
