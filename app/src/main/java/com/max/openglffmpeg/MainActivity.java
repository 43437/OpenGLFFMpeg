package com.max.openglffmpeg;

import android.app.Activity;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.view.View;
import android.widget.EditText;

public class MainActivity extends Activity {

    private GLSurfaceView glSurfaceView;
    private FFMpegRender ffmpegRenderer;
    private EditText editText;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        glSurfaceView = (GLSurfaceView) findViewById(R.id.video_surface);
        editText = (EditText) findViewById(R.id.fileUrl);
        ffmpegRenderer = new FFMpegRender();
        glSurfaceView.setRenderer(ffmpegRenderer);
    }

    public void onClicked(View view){

        String filePath= editText.getEditableText().toString();
        ffmpegRenderer.native_play(filePath);
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
}
