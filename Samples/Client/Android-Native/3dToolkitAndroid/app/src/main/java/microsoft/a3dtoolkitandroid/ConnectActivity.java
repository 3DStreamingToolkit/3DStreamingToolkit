package microsoft.a3dtoolkitandroid;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.EditText;

import com.android.volley.Request;
import com.android.volley.RequestQueue;
import com.android.volley.Response;
import com.android.volley.VolleyError;
import com.android.volley.toolbox.StringRequest;
import com.android.volley.toolbox.Volley;

public class ConnectActivity extends AppCompatActivity {
    public static final String SERVER_LIST = "com.microsoft.a3dtoolkitandroid.LIST";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_connect);
    }

    /**
     * Connects to remote server with provided info
     * @param view
     */
    public void connect(View view) {
        final Intent intent = new Intent(this, ServerListActivity.class);
        final String server = ((EditText) findViewById(R.id.server)).getText().toString().toLowerCase();
        String port = ((EditText) findViewById(R.id.port)).getText().toString().toLowerCase();

        //alert dialog
        final AlertDialog.Builder builder;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            builder = new AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert);
        } else {
            builder = new AlertDialog.Builder(this);
        }

        //check for empty
        if (server.length() == 0 || port.length() == 0) {
            builder.setTitle("Missing Info")
                    .setMessage(server.length() == 0 ? "Please fill server" : "Please fill port");
        } else {
            // Instantiate the RequestQueue.
            RequestQueue queue = Volley.newRequestQueue(this);
            String url = server + "/sign_in?peer_name=" + port;

            // Request a string response from the server
            StringRequest stringRequest = new StringRequest(Request.Method.GET, url,
                    new Response.Listener<String>() {
                        @Override
                        public void onResponse(String response) {
                            intent.putExtra(SERVER_LIST, response);
                            startActivity(intent);
                        }
                    }, new Response.ErrorListener() {
                @Override
                public void onErrorResponse(VolleyError error) {
                    builder.setTitle("Error")
                            .setMessage("Sorry request did not work!");
                }
            });
            // Add the request to the RequestQueue.
            queue.add(stringRequest);

            //show the alert
            AlertDialog alertDialog = builder.create();
            alertDialog.show();
        }
    }
}
