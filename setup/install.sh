sudo apt update
echo "Y" | sudo apt install g++ tcpdump cmake build-essential libgmp-dev libboost-all-dev python3-pip
pip install gdown absl-py pandas numpy matplotlib

### install emp
cd
wget https://raw.githubusercontent.com/emp-toolkit/emp-readme/master/scripts/install.py
python3 install.py --deps 

### install emp-tool and enable multi-thread
cd
git clone https://github.com/emp-toolkit/emp-tool.git
cd emp-tool
git checkout 6e75f6d03e622ca6a2a23ba0c1c82fdd93f2c733
cp ../cycle-codes/emp-tool.diff .
git apply emp-tool.diff
cmake -B build
cd build
make -j
sudo make install
### Check out to home directory
cd

wget https://raw.githubusercontent.com/emp-toolkit/emp-readme/master/scripts/install.py
python3 install.py --ot --sh2pc
cd

### refresh shared library cache
sudo ldconfig

echo "ulimit -n 1048576" >> ~/.bashrc