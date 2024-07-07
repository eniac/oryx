## End-to-end evaluation

### How to build
```
cmake . -B build
cd build
make -j
```

### How to run
+ You need three servers for running the experiment.
+ Simply run `./server CONFIG_FILE` on each machine.
+ [config](./configs/) directory provides templates for the config files.
+ [m5.16xlarge.configs](./m5.16xlarge.configs) provides the config files we used
  when experimenting on `m5.16xlarge` instances on AWS.

### Notes
Please update the ip addresses in each config file and the location of graph file correspondingly.