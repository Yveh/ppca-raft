echo "building the raft cluster with 5 servers..."
gnome-terminal --title="Server1" --window -x bash -c "./main configs/1.json"
gnome-terminal --title="Server2" --window -x bash -c "./main configs/2.json"
gnome-terminal --title="Server3" --window -x bash -c "./main configs/3.json"
gnome-terminal --title="Server4" --window -x bash -c "./main configs/4.json"
gnome-terminal --title="Server5" --window -x bash -c "./main configs/5.json"
