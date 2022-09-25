package com.nipuream.n_player;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import android.Manifest;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.ByteBuffer;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "MainActivity";
    private static final int PERMISSION_REQUEST_CODE = 1024;
    private NPlayer player;

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        //remove title bar.
        supportRequestWindowFeature(Window.FEATURE_NO_TITLE);
        //full screen.
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        //set display landscape.
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        setContentView(R.layout.activity_main);
        player = findViewById(R.id.n_player);
        checkPermission();
    }

    private void checkPermission(){
        if(ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED
           || ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED
           || ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(this, new String[]{
                        Manifest.permission.CAMERA,
                        Manifest.permission.WRITE_EXTERNAL_STORAGE,
                        Manifest.permission.READ_EXTERNAL_STORAGE
                }, PERMISSION_REQUEST_CODE);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if(requestCode == PERMISSION_REQUEST_CODE) {
            Log.i(TAG, "permission resultCode : " + resultCode);
        }
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();

    public native void readByteBuffer(ByteBuffer buf);
    

    public void test(View view) {

//        new Thread(){
//            @Override
//            public void run() {
//                super.run();
//                stringFromJNI();
//            }
//        }.start();

//        File file = new File("/sdcard/1080.mp4");
//        if(file.exists()){
//            Log.i(TAG,"file exits.");
//
//            // Example of a call to a native method
//            TextView tv = findViewById(R.id.sample_text);
//            tv.setText(stringFromJNI());
//        }

        new Thread(){
            @Override
            public void run() {
                super.run();
                Surface surface = player.getHolder().getSurface();
                Log.i(TAG, " surface address : "+ surface);
                player.open("/sdcard/out.yuv", surface);
            }
        }.start();
    }

    public void test1(View view) {

        ByteBuffer buf = ByteBuffer.allocateDirect(100);
        buf.put("nipuream".getBytes());
        readByteBuffer(buf);
    }

}
