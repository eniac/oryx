#!/bin/bash
ulimit -n 1048576

configs_file="$1"
bin_path="$2"
logs_dir="$3"
another_server_ip="$4"

# Loop through the new values and update the configuration file
for p in 18 20 22 24
do
    # for d in 5 10 15
    # do
        for k in 3 4 5
        do
            for r in 0.05 0.1 0.15
            do
                pn=$((2**$p))
                # sed -i "/^max_degree$/{n;s/.*/$d/}" "$configs_file"
                sed -i "/^detect_length_k$/{n;s/.*/$k/}" "$configs_file"
                sed -i "/^path_num$/{n;s/.*/$pn/}" "$configs_file"
                sed -i "/^valid_ratio$/{n;s/.*/$r/}" "$configs_file"
                echo "sudo tcpdump -i any -nn host "$another_server_ip" > "$logs_dir"/filter_p"$p"_k"$k"_r"$r"_network_raw.log &"
                sudo tcpdump -i any -nn host "$another_server_ip" > "$logs_dir"/filter_p"$p"_k"$k"_r"$r"_network_raw.log &
                echo "$d" "$k" "$p" "$pn"
                echo "$bin_path $configs_file > "$logs_dir"/filter_p"$p"_k"$k"_r"$r".log"
                start_timestamp=$(date "+%Y-%m-%d %H:%M:%S")
                echo "Start Timestamp: $start_timestamp" >> "$logs_dir"/filter_p"$p"_k"$k"_r"$r"_network.log
                $bin_path $configs_file > "$logs_dir"/filter_p"$p"_k"$k"_r"$r".log
                sleep 10
                sudo pkill -SIGINT tcpdump
                end_timestamp=$(date "+%Y-%m-%d %H:%M:%S")
                echo "End Timestamp: $end_timestamp" >> "$logs_dir"/filter_p"$p"_k"$k"_r"$r"_network.log
                python3 network_analysis.py "$logs_dir"/filter_p"$p"_k"$k"_r"$r"_network_raw.log >> "$logs_dir"/filter_p"$p"_k"$k"_r"$r"_network.log
                rm "$logs_dir"/filter_p"$p"_k"$k"_r"$r"_network_raw.log
            done
        done
    # done
done
