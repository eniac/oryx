#!/bin/bash
ulimit -n 1048576

configs_file="$1"
bin_path="$2"
logs_dir="$3"
another_server_ip="$4"

# Loop through the new values and update the configuration file
for p in 18 20 22 24
do
    for d in 10
    do
        for k in 4
        do
            # We replace the nodes_num, total_path num is 2*nodes_num
            pn=$((2**($p-1)))
            sed -i "/^max_degree$/{n;s/.*/$d/}" "$configs_file"
            sed -i "/^detect_length_k$/{n;s/.*/$k/}" "$configs_file"
            sed -i "/^nodes_num$/{n;s/.*/$pn/}" "$configs_file"
            echo "sudo tcpdump -i any -nn host "$another_server_ip" > "$logs_dir"/extract_p"$p"_d"$d"_k"$k"_network_raw.log &"
            sudo tcpdump -i any -nn host "$another_server_ip" > "$logs_dir"/extract_p"$p"_d"$d"_k"$k"_network_raw.log &
            echo "$d" "$k" "$p" "$pn"
            echo "$bin_path $configs_file > "$logs_dir"/extract_p"$p"_d"$d"_k"$k".log"
            start_timestamp=$(date "+%Y-%m-%d %H:%M:%S")
            echo "Start Timestamp: $start_timestamp" >> "$logs_dir"/extract_p"$p"_d"$d"_k"$k"_network.log
            $bin_path $configs_file > "$logs_dir"/extract_p"$p"_d"$d"_k"$k".log
            sleep 10
            sudo pkill -SIGINT tcpdump
            end_timestamp=$(date "+%Y-%m-%d %H:%M:%S")
            echo "End Timestamp: $end_timestamp" >> "$logs_dir"/extract_p"$p"_d"$d"_k"$k"_network.log
            python3 network_analysis.py "$logs_dir"/extract_p"$p"_d"$d"_k"$k"_network_raw.log >> "$logs_dir"/extract_p"$p"_d"$d"_k"$k"_network.log
            rm "$logs_dir"/extract_p"$p"_d"$d"_k"$k"_network_raw.log
        done
    done
done
