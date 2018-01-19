package com.max.openglffmpeg;

import android.opengl.GLSurfaceView;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * Created by geyu on 18-1-18.
 */

public class FFMpegRender implements GLSurfaceView.Renderer {

    private native void surfaceChange(int width, int height);
    private native void createSurface();
    private native void drawFrame();
    public native void native_play(String file);

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
//        surfaceChange(width, height);
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        createSurface();
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        drawFrame();
    }
}
