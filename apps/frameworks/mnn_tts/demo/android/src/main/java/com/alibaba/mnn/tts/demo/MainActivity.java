package com.alibaba.mnn.tts.demo;

import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;

import com.alibaba.mnn.tts.MNNTTS;

public class MainActivity extends AppCompatActivity {
    private TextView resultText;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        resultText = findViewById(R.id.resultText);

        // Call the JNI method and display the result
        try {
            String helloFromJNI = MNNTTS.getHelloWorldFromJNI();
            String message = "Message from JNI: " + helloFromJNI;
            resultText.setText(message);
            Log.d("MNN_DEMO", message);
        } catch (Exception e) {
            String errorMessage = "JNI call failed: " + e.getMessage();
            resultText.setText(errorMessage);
            Log.e("MNN_DEMO", errorMessage, e);
        }
    }
} 