package microsoft.a3dtoolkitandroid;

import android.content.Intent;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.EditText;
import android.widget.Toast;

import com.android.volley.Request;
import com.facebook.stetho.Stetho;

import microsoft.a3dtoolkitandroid.util.Config;

import static microsoft.a3dtoolkitandroid.util.HelperFunctions.addRequest;

public class SignInActivity extends AppCompatActivity {
    public static final String SERVER_LIST = "com.microsoft.a3dtoolkitandroid.LIST";
    public static final String SERVER_SERVER = "com.microsoft.a3dtoolkitandroid.SERVER";
    public static final String NAME = "com.microsoft.a3dtoolkitandroid.NAME";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Stetho.initializeWithDefaults(this);
        setContentView(R.layout.activity_connect);
        EditText editText = findViewById(R.id.serverInput);
        editText.setText(Config.signalingServer);
    }

    /**
     * Connects to remote server with provided info
     * @param view
     */
    public void signIn(View view) {
        final Intent intent = new Intent(this, ConnectActivity.class);
        String server = ((EditText) findViewById(R.id.serverInput)).getText().toString().toLowerCase();
        String name = ((EditText) findViewById(R.id.port)).getText().toString().toLowerCase();

        //check for empty
        if (server.length() == 0 || name.length() == 0) {
            Toast.makeText(this, server.length() == 0 ? "Please fill server" : "Please fill name", Toast.LENGTH_SHORT).show();
        } else {
            String url = server + "/sign_in?peer_name=" + name.replaceAll(" ", "_");
            intent.putExtra(SERVER_SERVER, server);
            intent.putExtra(NAME, name);

            addRequest(url, Request.Method.GET, this, response -> {
                intent.putExtra(SERVER_LIST, response);
                startActivity(intent);
            });

        }
    }
}
