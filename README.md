# Spatially-Constrained Top-𝑘 Meta Path Similarity Search

## About

Extension of [PathSim](https://dl.acm.org/doi/abs/10.14778/3402707.3402736) to incorporate (spatial) proximity to allow inexact matches based.


## Setting Up for Local Development

Follow the steps below to set up and run the project on your local machine:

1. Install an IDE like [CLion](https://learn.microsoft.com/en-us/windows/wsl/install) or [VSCode](https://code.visualstudio.com/) (optional; if you plan to develop).
    If you work on Windows, you might want to install [WSL](https://learn.microsoft.com/en-us/windows/wsl/install) for simpler compilation in Linux.
    
2. Install Essential Libraries:

     Install the [Eigen](https://robots.uc3m.es/installation-guides/install-eigen.html) C++ library.
     Depending on your operating system, the installation process might vary.
     This is the process on Ubuntu:
   
      ```bash
      sudo apt install libeigen3-dev
      ```

3. Install CMake:
    CMake is a cross-platform build system.
    Depending on your operating system, the installation process might vary:
    This is the process on Ubuntu:
   
    ```bash
    sudo apt install cmake
    ```

4. Build the project:
    ```bash
    ~/.../spatially-constrained-pathsim/build$ cmake .. -DBUILD_TESTING=ON
    ~/.../spatially-constrained-pathsim/build$ make
    ```

5. Prepare and preprocess the datasets:

   GeoDBLP is already provided in HIN format under `Datasets/GeoDBLP`.
   Foursquare and YFCC can be built from online-available data with the dataset scripts:
   ```bash
   ~/.../spatially-constrained-pathsim$ python Datasets/Foursquare/createHINFilesFromFoursquare.py
   ~/.../spatially-constrained-pathsim$ python Datasets/YFCC/createHINFilesFromYFCC.py
   ```
   Both scripts write their generated files into their own dataset directory, i.e., `Datasets/Foursquare` and `Datasets/YFCC`.

   After that, run the C++ preprocessing executables on the common datasets directory:
    ```bash
    ~/.../spatially-constrained-pathsim$ ./build/Experiments/Analysis/Preprocess/preprocessGeoDBLP ./Datasets
    ~/.../spatially-constrained-pathsim$ ./build/Experiments/Analysis/Preprocess/preprocessFoursquare ./Datasets
    ~/.../spatially-constrained-pathsim$ ./build/Experiments/Analysis/Preprocess/preprocessYFCC ./Datasets
    ```
   
6. Verify the Installation:
    - After setting up, you can run the test to verify the installation.
    - To run the test, execute the test executable in the `build/tests` directory, i.e.:
    ```bash
    ~/.../spatially-constrained-pathsim$ ./build/tests/EdgeDecompositionUnweightedValidationTest
    ```

## Experiments

The experiment executables expect one directory that contains the dataset folders, e.g., `Datasets/Foursquare`, `Datasets/GeoDBLP`, and `Datasets/YFCC`.
The experiments create the required base matrices with `preprocessDataset` when needed, so the original CSV files can be used directly.

### Approximation Quality

The approximation-quality experiment compares the grid-based spatial PathSim against an edge-decomposition spatial PathSim result.
For each dataset, the reference is unweighted Edge Decomposition with epsilon `1000`.
The candidates are unweighted H3 Grid Range Query variants with resolution `5-9` and epsilon `500`  for the under-approximation and resolution-dependent epsilon for the over-approximation.
The script computes PathSim scores per query and reports macro-averaged quality metrics such as precision, recall, MAE, MSE, Overlap@k, and nDCG@k.
1000 query objects are sampled with seed `42`.

Run the comparison for the Foursquare dataset with grid resolution 9:

```bash
~/.../spatially-constrained-pathsim$ ./build/Experiments/ApproximationQuality/compareApproximationQualityFoursquareRes9 ./Datasets ./Experiments/ApproximationQuality/approximation_quality_foursquare.csv
```

The second argument is optional. If omitted, the CSV summary is written to `Experiments/ApproximationQuality/approximation_quality_foursquare.csv`.

### Compute Max Distance

Contains standalone scripts for computing the global maximum distance from an H3 cell center to its boundary, used to approximate ρ in the paper.

The executable writes CSV with the relevant upper-bound column:

```text
rounded_upper_bound_m
```

Resolution 0 through 5 are run single-threaded. Resolution 6 through 15 use OpenMP through `OMP_NUM_THREADS`, normally set from `SLURM_CPUS_PER_TASK`.

Run the script locally using:

```bash
bash Experiments/ComputeMaxDistance/run_compute_max_distance.sh 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
```

The per-resolution CSV files are written to:

```text
Experiments/ComputeMaxDistance/results/
```

### Experimental Analysis

The experimental analysis compares two Foursquare user nodes with regular PathSim on the `User-Venue-User` meta path and Edge Decomposition PathSim with a fixed epsilon of `100` meters.
There are separate executables for the unweighted and weighted Edge Decomposition approaches.
Both print the deltas, row dot product, and PathSim score for the regular and Edge Decomposition matrices.

Run the unweighted comparison for two Foursquare user node IDs:

```bash
~/.../spatially-constrained-pathsim$ ./build/Experiments/ExperimentalAnalysis/computeFoursquareUserPathSimScoresUnweighted ./Datasets 794 398
```

Run the weighted comparison for the same user node IDs:

```bash
~/.../spatially-constrained-pathsim$ ./build/Experiments/ExperimentalAnalysis/computeFoursquareUserPathSimScoresWeighted ./Datasets 794 398
```

An optional fourth argument writes the same output to a file, for example:

```bash
~/.../spatially-constrained-pathsim$ ./build/Experiments/ExperimentalAnalysis/computeFoursquareUserPathSimScoresUnweighted ./Datasets 794 398 ./Experiments/ExperimentalAnalysis/pathsim_scores_794_398_unweighted.txt
```


### Track Runtimes

The TrackRuntimes executables reproduce the scalability experiments from the paper. 

They measure commuting-matrix creation and PathSim top-k query runtimes for Edge Decomposition and H3 Grid variants while sweeping the spatial range parameters over `{0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 75}` km. 

The grid experiments cover H3 resolutions `5`, `7`, and `9`; weighted and unweighted variants are tracked separately. 

Equal numeric values for the edge-decomposition and grid range parameters are intended as a scalability stress test and do not represent equivalent induced spatial relations.

Run the unweighted Foursquare H3 Grid query-runtime tracking for resolution `9`, epsilon `1000`, `k = 10`, and `10` measured runs:

```bash
~/.../spatially-constrained-pathsim$ ./build/Experiments/TrackRuntimes/Foursquare/trackPathSimQueryGridResolution9Epsilon1000Unweighted ./Datasets 10 10
```


## Format of Datasets

All datasets are contained in their respectively named directories and should all contain the files `nodes.csv`, `node_types.csv`, `edges.csv`, `edge_types.csv` and `schema.csv` as defined in the [HIN Datasets Git repository](https://git.informatik.uni-kiel.de/ag-ai/hin-datasets).
The format and contents of these files is described in greater detail below.

### nodes.csv

Each line contains a node type ID as integers.
The node IDs start implicitly with 0, are sorted in ascending order, and are continuous.
For example, if line 42 (assuming the fist line is numbered with 0) contains `5\n`, this signifies that node 42 has the fifth node type, which is explained in node_types.csv.

### node_types.csv

Each line contains a node type name.
The type IDs implicitly start with 0, are sorted in ascending order, and are continuous.
For example, the 5th line `author\` (index 6 as line numbers start with 0) signifies that the fifth node type has the name 'author'.
Hence, node 42 from the previous example is an author-type node.
In some files, the name of the node type is given by a number.
In this case the meaning of the node type is unknown.


### edges.csv

Each line contains a source node ID, an edge type ID, and destination node IDs, all encoded as integers and separated by comma.
The edge IDs are implicitly assumed in ascending, continuous fashion, startting with 0, and sorted by source node ID primarily and destination node ID secondly.
For example, the 108th line  `69,2,420\n` signifies that the 108th edge of the second edge type points from node 69 to node 420. Nice.

### edge_type.csv

Each line contains an edge type name.
The type IDs implicitly start with 0, are sorted in ascending order, and are continuous.
For example, the line 2 `write\n` signifies that the second edge type has the name 'write'.
Hence, edge 108 from the previous example is a write-type node that connects author 69 to paper 420.
In some files, the name of the edge type is given by a number.
In this case the meaning of the edge type is unknown.

### schema.csv

Each line contains a source node type ID, and a destination type ID, separated by comma.
The lines are sorted by the edge type ID primarily, and secondly by source node type (in case of symmetric relations split into two edge types).
For example, the first line (i.e. line 0) `1,3\n` signifies that the 0th allowed edge type points from nodes of type 1 to nodes of type 3.
