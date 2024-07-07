#!/bin/bash
ulimit -n 1048576

configs_file="$1"
bin_path="$2"
logs_dir="$3"
s1="$4"
s2="$5"

# Loop through the new values and update the configuration file
for p in 18 20 22 24
do
    for d in 2 4 8
    do
        for k in 3 6
        do
            pn=$((2**$p))
            sed -i "/^max_degree$/{n;s/.*/$d/}" "$configs_file"
            sed -i "/^detect_length_k$/{n;s/.*/$k/}" "$configs_file"
            sed -i "/^path_num$/{n;s/.*/$pn/}" "$configs_file"
            echo "sudo tcpdump -i any -nn host "$s1" or host "$s2" > "$logs_dir"/shuffle_p"$p"_d"$d"_k"$k"_network_raw.log &"
            sudo tcpdump -i any -nn host "$s1" or host "$s2" > "$logs_dir"/shuffle_p"$p"_d"$d"_k"$k"_network_raw.log &
            echo "$d" "$k" "$p" "$pn"
            echo "$bin_path $configs_file > "$logs_dir"/shuffle_p"$p"_d"$d"_k"$k".log"
            start_timestamp=$(date "+%Y-%m-%d %H:%M:%S")
            echo "Start Timestamp: $start_timestamp" >> "$logs_dir"/shuffle_p"$p"_d"$d"_k"$k"_network.log
            $bin_path $configs_file > "$logs_dir"/shuffle_p"$p"_d"$d"_k"$k".log
            sleep 10
            sudo pkill -SIGINT tcpdump
            end_timestamp=$(date "+%Y-%m-%d %H:%M:%S")
            echo "End Timestamp: $end_timestamp" >> "$logs_dir"/shuffle_p"$p"_d"$d"_k"$k"_network.log
            python3 network_analysis_shuffle.py "$logs_dir"/shuffle_p"$p"_d"$d"_k"$k"_network_raw.log >> "$logs_dir"/shuffle_p"$p"_d"$d"_k"$k"_network.log
            rm "$logs_dir"/shuffle_p"$p"_d"$d"_k"$k"_network_raw.log
        done
    done
done
