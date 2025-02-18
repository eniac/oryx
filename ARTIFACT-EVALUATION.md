# Artifact Appendix

Paper title: **Oryx: Private detection of cycles in federated graphs**

Artifacts HotCRP Id: **#Enter your HotCRP Id here** (not your paper Id, but the artifacts id)

Requested Badge: **Reproduced**

## Description
This repo includes the codebase and instructions to reproduce results in our paper, which runs a MPC protocol of detecting cycles over federated graphs where the graphs are held by different parties.

Our evaluation has three different parts:
+ Micro-benchmarks.
+ End-to-end evaluation on small synthetic datasets.
+ End-to-end evaluation on a public financial dataset.

The last experiment requires running on AWS m5.16xlarge instances (32-core Intel Xeon and 256 GB RAM) and takes about 5 hours to reproduce the entire end-to-end run.
Due to limited funding, we don't provide the AWS instances to run the last section.
However, the previous experiments should work on generic machines with all dependencies installed correctly.

### Security/Privacy Issues and Ethical Concerns (All badges)
None.

## Basic Requirements (Only for Functional and Reproduced badges)

### Hardware Requirements
At least 128 GB memmory.

### Software Requirements
Ubuntu 20.02 with all dependencies installed correctly.
Detailed instructios 

### Estimated Time and Storage Consumption
Estimated time: 1 hour.
Storage consumption: less than 1GB.

## Environment 
**It's recommended to run all experiments in an isolated environment such as a virtual machine, as it may change some system setting.**

### Accessibility (All badges)
It's available at https://github.com/eniac/oryx.

### Set up the environment (Only for Functional and Reproduced badges)
The full details of dependencies and descriptions of how to set up the environment can be found in [setup/install.md](./setup/install.md).

We have also provided a script for the reviewers to use and install one dependencies easily. The script comes as it is. 

```
bash ./setup/install.sh
```

The dependencies will be installed at your home directory.

### Testing the Environment (Only for Functional and Reproduced badges)
Run some unit tests under [secret-share](./secret-share/) directory.

```
cd secret-share
cmake -B build
cd build
make
./secret_share_test [choose from 1-6 to run different unit tests]
```

## Artifact Evaluation (Only for Functional and Reproduced badges)

### Main Results and Claims
List all your paper's results and claims that are supported by your submitted artifacts.

#### Main Result 1: Micro-benchmarks of each subroutine
The first main result is presented in section 9.1 **Costs of each subroutine** in our paper. It presents how the (1) latency and (2) network traffic of each subroutine changes with different parameters.

#### Main Result 2: End-to-end execution
The result is presented in section 9.2 **End-to-end evaluation**.
Here we give instructions to reproduce results in section 9.2.1 **Evaluation on small synthetic dataset**.
Results in section 9.2.2 have to be reproduced in a powerful instance such as 
AWS m5.16xlarge, which we don't provide in our evaluation.

### Experiments 
List each experiment the reviewer has to execute. Describe:
 - How to execute it in detailed steps.
 - What the expected result is.
 - How long it takes and how much space it consumes on disk. (approximately)
 - Which claim and results does it support, and how.

#### Experiment 1: Micro-benchmarks
To run the micro-benchmarks, first run the following scripts to build.
```
cd oryx/micro-eval
cmake -B build
cd build
make -j
```

The output binaries are stored in `./build/bin`

#### Experiment 1.1: Micro-benchmarks of shuffle subroutine
Steps to run:
```
cd oryx/micro-eval/scripts
cmake -B build
cd build
make -j
```

Expected 

#### Experiment 2: Name
...

#### Experiment 3: Name 
...

## Limitations (Only for Functional and Reproduced badges)
Results in section 9.2.2 are not reproduced here due to that the instances are expensive for us to afford (can take hundreds of dollars for one day).

## Notes on Reusability (Only for Functional and Reproduced badges)
First, this section might not apply to your artifacts.
Use it to share information on how your artifact can be used beyond your research paper, e.g., as a general framework.
The overall goal of artifact evaluation is not only to reproduce and verify your research but also to help other researchers to re-use and improve on your artifacts.
Please describe how your artifacts can be adapted to other settings, e.g., more input dimensions, other datasets, and other behavior, through replacing individual modules and functionality or running more iterations of a specific part.