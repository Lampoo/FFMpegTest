package org.lampoo.ffmpeg;

import androidx.annotation.RequiresApi;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {
    private final String TAG = "MainActivity";

    private final String FILE = "SD_3.mp4";

    private final int MY_PERMISSIONS_REQUEST_READ_EXTERNAL_STORAGE = 200;

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @RequiresApi(api = Build.VERSION_CODES.M)
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
        TextView tv = findViewById(R.id.sample_text);
        tv.setText(FILE);

        //if (Build.VERSION.SDK_INT > 22) {
            if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) !=
                    PackageManager.PERMISSION_GRANTED) {
                if (shouldShowRequestPermissionRationale(Manifest.permission.READ_EXTERNAL_STORAGE)) {
                    // Provide an additional rationale to the user if the permission was not granted
                    // and the user would benefit from additional context for the use of the permission.
                    // For example if the user has previously denied the permission.
                }
                requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE},
                        MY_PERMISSIONS_REQUEST_READ_EXTERNAL_STORAGE);
            }  else {
                // permission granted
                call_native_main(Environment.getExternalStorageDirectory() + "/MyLocalPlayer/" + FILE);
            }
        //}

    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        switch (requestCode) {
        case MY_PERMISSIONS_REQUEST_READ_EXTERNAL_STORAGE:
            if (grantResults.length == 1 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                call_native_main(Environment.getExternalStorageDirectory() + "/MyLocalPlayer/" + FILE);
            }
            break;
        }
    }

    private void call_native_main(final String path) {
        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                native_main(path);
            }
        });
        thread.start();
    }

    public native void native_main(String path);
}
