package com.android.antidebug;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity implements IAntiDebugCallback{

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        AntiDebug.setAntiDebugCallback(this);
    }


    @Override
    public void beInjectedDebug() {
        Log.e("weikaizhi", "beInjectedDebug");
    }
}
