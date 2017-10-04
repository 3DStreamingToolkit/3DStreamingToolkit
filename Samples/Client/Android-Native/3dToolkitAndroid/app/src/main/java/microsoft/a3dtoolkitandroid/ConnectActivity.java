package microsoft.a3dtoolkitandroid;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.EditText;

import com.android.volley.AuthFailureError;
import com.android.volley.Request;
import com.android.volley.RequestQueue;
import com.android.volley.toolbox.StringRequest;
import com.android.volley.toolbox.Volley;

import java.util.HashMap;
import java.util.Map;

import microsoft.a3dtoolkitandroid.util.Config;

public class ConnectActivity extends AppCompatActivity {
    public static final String SERVER_LIST = "com.microsoft.a3dtoolkitandroid.LIST";
    public static final String SERVER_SERVER = "com.microsoft.a3dtoolkitandroid.SERVER";
    public static final String NAME = "com.microsoft.a3dtoolkitandroid.PORT";

    private String server;
    private String name;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_connect);
        EditText editText = (EditText)findViewById(R.id.server);
        editText.setText(Config.signalingServer);
    }

    /**
     * Connects to remote server with provided info
     * @param view
     */
    public void connect(View view) {
        final Intent intent = new Intent(this, ServerListActivity.class);
        server = ((EditText) findViewById(R.id.server)).getText().toString().toLowerCase();
        name = ((EditText) findViewById(R.id.port)).getText().toString().toLowerCase();

        //alert dialog
        final AlertDialog.Builder builder;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            builder = new AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert);
        } else {
            builder = new AlertDialog.Builder(this);
        }

        //check for empty
        if (server.length() == 0 || name.length() == 0) {
            builder.setTitle("Missing Info")
                    .setMessage(server.length() == 0 ? "Please fill server" : "Please fill port");
        } else {
            // Instantiate the RequestQueue.
            RequestQueue queue = Volley.newRequestQueue(this);
            String url = server + "/sign_in?peer_name=" + name;
            intent.putExtra(SERVER_SERVER, server);
            intent.putExtra(NAME, name);

            // Request a string response from the server
            StringRequest getRequest = new StringRequest(Request.Method.GET, url,
                    response -> {
                        // response
                        intent.putExtra(SERVER_LIST, response);
                        startActivity(intent);
                    },
                    error -> builder.setTitle("Error")
                            .setMessage("Sorry request did not work!")
            ) {
                @Override
                public Map<String, String> getHeaders() throws AuthFailureError {
                    Map<String, String>  params = new HashMap<>();
                    params.put("Peer-Type", "Client");
                    return params;
                }
            };
            // Add the request to the RequestQueue.
            queue.add(getRequest);

            //show the alert
            AlertDialog alertDialog = builder.create();
            alertDialog.show();
        }
    }
}
