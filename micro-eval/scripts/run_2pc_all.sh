ulimit -n 1048576

extract_config="$1"
extract_logs="$2"

filter_config="$3"
filter_logs="$4"

sort_config="$5"
sort_logs="$6"

build_dir="$7"

bash run_extract.sh $extract_config $build_dir/extract $extract_logs

bash run_filter.sh $filter_config $build_dir/filter $filter_logs

bash run_sort.sh $sort_config $build_dir/sort $sort_logs