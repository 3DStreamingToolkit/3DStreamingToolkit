package microsoft.a3dtoolkitandroid;

import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import static java.lang.Integer.parseInt;

/**
 * A fragment representing a list of Items.
 * <p/>
 * Activities containing this fragment MUST implement the {@link OnListFragmentInteractionListener}
 * interface.
 */
public class ServerListFragment extends Fragment {
    public OnListFragmentInteractionListener mListener;
    public ServerListRecyclerViewAdapter adapter;
    public RecyclerView recyclerView;
    public List<ServerItem> servers = new ArrayList<>();
    public Map<Integer, ServerItem> serverMap = new HashMap<>();

    public void initializeList(String originalList){
        // split the string into a list of servers
        List<String> peers = new ArrayList<>(Arrays.asList(originalList.split("\n")));

        // remove the first one and add it as myID
        mListener.addMyID(parseInt(peers.remove(0).split(",")[1]));

        // add the rest of the servers to the adapter for display
        for (int i = 0; i < peers.size(); ++i) {
            if (peers.get(i).length() > 0) {
                String[] parsed = peers.get(i).split(",");
                addServer(parseInt(parsed[1]), parsed[0]);
            }
        }
        if (adapter == null){
            adapter = new ServerListRecyclerViewAdapter(getActivity());
            recyclerView.setAdapter(adapter);
        }

    }

    /**
     * Updates the peer list adapter with any new entries
     */
    public void addPeerList(int serverID, String serverName) {
        addServer(serverID, serverName);
        adapter.notifyDataSetChanged();
    }

    /**
     * Adds a server to the list of servers and the map of server names to server id's
     */
    public void addServer(int serverID, String serverName) {
        ServerItem item = new ServerItem(serverID, serverName);
        servers.add(item);
        serverMap.put(item.id, item);
    }

    /**
     * Mandatory empty constructor for the fragment manager to instantiate the
     * fragment (e.g. upon screen orientation changes).
     */
    public ServerListFragment() {
    }

    public static ServerListFragment newInstance() {
        ServerListFragment fragment = new ServerListFragment();
        Bundle args = new Bundle();
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getActivity().getIntent();
    }



    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_server_list, container, false);

        // Set the adapter
        if (view instanceof RecyclerView) {
            Context context = view.getContext();
            recyclerView = (RecyclerView) view;
            recyclerView.setLayoutManager(new LinearLayoutManager(context));
        }
        return view;
    }


    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        if (context instanceof OnListFragmentInteractionListener) {
            mListener = (OnListFragmentInteractionListener) context;
        } else {
            throw new RuntimeException(context.toString()
                    + " must implement OnListFragmentInteractionListener");
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();
        mListener = null;
    }

    class ServerListRecyclerViewAdapter extends RecyclerView.Adapter<ViewHolder> {

        private LayoutInflater mLayoutInflater;

        ServerListRecyclerViewAdapter(Context context) {
            mLayoutInflater = LayoutInflater.from(context);
        }

        @Override
        public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            return new ViewHolder(mLayoutInflater
                    .inflate(R.layout.fragment_server_item, parent, false));
        }

        @Override
        public void onBindViewHolder(final ViewHolder holder, int position) {
            holder.mItem = servers.get(position);
            holder.mTextView.setText(holder.mItem.serverName);

            holder.itemView.setOnClickListener(v -> {
                if (mListener != null) {
                    // Notify the active callbacks interface (the activity, if the
                    // fragment is attached to one) that an item has been selected.
                    mListener.onServerClicked(holder.mItem.id);
                }
            });
        }

        @Override
        public int getItemCount() {
            return servers.size();
        }

    }

    class ViewHolder extends RecyclerView.ViewHolder {
        private final TextView mTextView;
        private ServerItem mItem;

        private ViewHolder(View itemView) {
            super(itemView);
            mTextView = itemView.findViewById(R.id.name);
        }

        @Override
        public String toString() {
            return super.toString() + " '" + mTextView.getText() + "'";
        }
    }

    /**
     * A item representing a server and its ID.
     */
    public class ServerItem {
        public final int id;
        public final String serverName;

        public ServerItem(int id, String serverName) {
            this.id = id;
            this.serverName = serverName;
        }

        @Override
        public String toString() {
            return serverName;
        }
    }

    /**
     * This interface must be implemented by activities that contain this
     * fragment to allow an interaction in this fragment to be communicated
     * to the activity and potentially other fragments contained in that
     * activity.
     * <p/>
     * See the Android Training lesson <a href=
     * "http://developer.android.com/training/basics/fragments/communicating.html"
     * >Communicating with Other Fragments</a> for more information.
     */
    public interface OnListFragmentInteractionListener {
        void onServerClicked(int server);
        void addMyID(int myID);
    }
}
