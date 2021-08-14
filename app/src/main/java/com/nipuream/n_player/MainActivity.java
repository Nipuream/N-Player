package com.nipuream.n_player;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.TextView;
import java.io.File;
import java.nio.ByteBuffer;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "MainActivity";

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();

    public native void readByteBuffer(ByteBuffer buf);


    public void test(View view) {
        File file = new File("/sdcard/1080.mp4");
        if(file.exists()){
            Log.i(TAG,"file exits.");

            // Example of a call to a native method
            TextView tv = findViewById(R.id.sample_text);
            tv.setText(stringFromJNI());
        }
    }

    public void test1(View view) {

        ByteBuffer buf = ByteBuffer.allocateDirect(100);
        buf.put("nipuream".getBytes());
        readByteBuffer(buf);
    }
}
