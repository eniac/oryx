#### Environments

We experiment on AWS instances with Ubuntu 20.04. Please refer to our paper for
more details.

#### Dependencies
To compile and run the codes, please install the following dependencies:
+ `sudo apt install g++ tcpdump cmake build-essential libgmp-dev libboost-all-dev python3-pip`.
+ `pip install gdown absl-py pandas numpy matplotlib`.
+ Emp-tool, we tried out with the
  [commit](https://github.com/emp-toolkit/emp-tool/tree/6e75f6d03e622ca6a2a23ba0c1c82fdd93f2c733).
  + Please apply the patch [emp-tool.diff](./emp-tool.diff) to the `emp-tool`
    repo by running `git apply emp-tool.diff` to enable multi-threading.
  + For your reference you can run the following script for installing
      `emp-tool`.
      ```
        git clone https://github.com/emp-toolkit/emp-tool.git
        cd emp-tool
        git checkout 6e75f6d03e622ca6a2a23ba0c1c82fdd93f2c733
        cp ../cycle-codes/emp-tool.diff .
        git apply emp-tool.diff
        cmake -B build
        cd build
        make -j
        sudo make install
      ```    
  + Then install the related `emp-tool` dependencies by running
    ```
        wget https://raw.githubusercontent.com/emp-toolkit/emp-readme/master/scripts/install.py
        python3 install.py --ot --sh2pc
    ```