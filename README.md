# Fine-grained and Powerful Equivalences

Subgraph matching, a cornerstone of graph analytics, critically suffers from redundant computations during the search process.
Existing methods primarily target identical computations—redundant operations that are localized to individual query vertices—but fail to address similar redundancies that recur across multiple query vertices. In this paper, we present a novel algorithm, called FiPE, that accelerates subgraph matching through __Fine-grained and Powerful Equivalences__. FiPE redefines redundancy elimination by shifting the optimization granularity from isolated vertices to vertex pairs and multiple vertex patterns. It introduces _vertex-pair equivalence_ to cluster candidate pairs with isomorphic neighbor structures, even if their individual vertices differ, enabling pruning of similar computations between adjacent query vertices. FiPE proposes _group equivalence_ to defer equivalence checks to later search depths, capturing potential redundancies incrementally. To fully exploit the advantages of the equivalence, we introduce two optimization techniques: a matching order generation method to reduce the overall search space and an efficient conflict resolution mechanism to avoid two query vertices being mapped to the same data vertex. Experiments on real-world graphs highlight the superiority of FiPE. FiPE achieves a speedup of 2 to 3 orders of magnitude on various graphs under the EPS (embeddings per second) metric.

## Compile

Under the root directory of the project, execute the following commands to compile the source code.

```zsh
mkdir build
cd build
cmake ..
make -j16
```

## Execute

After compiling the source code, you can find the binary file 'BS' under the 'build/bin' directory.
Execute the binary with the following command ./BS -d data_graph -q query_graph 
-num max_number_of_embeddings -time_limit max_execute_time -o output_file -conf configuration_of_methods.
|Parameter of Command Line (-filter) | Description |
| :-----------------------------------: | :-------------: |
|-d| the data graph |
|-q| the query graph |
|-num| the maximum number of embeddings, set -num as 'MAX' to find all results |
|-time_limit| the maximum execution time |
|-conf| the path to your configuration files which specify methods to run |
|-o| the output file |


Example (Use the filtering method of CFL and order method of GraphQL to generate the candidate vertex sets and the matching order respectively.
Enumerate results with the set-intersection based local candidate computation method):

```zsh
./build/bin/BS -d ./valid/sample_dataset/test_case_1.graph -q ./valid/sample_dataset/query1_positive.graph -num MAX -o ./output_file.csv -conf ./valid/conf -time_limit 1
```

## Input

Both the input query graph and data graph are vertex-labeled.
Each graph starts with 't N M' where N is the number of vertices and M is the number of edges. A vertex and an edge are formatted
as 'v VertexID LabelId Degree' and 'e VertexId VertexId' respectively. Note that we require that the vertex
id is started from 0 and the range is [0,N - 1] where V is the vertex set. The following
is an input sample. You can also find sample data sets and query sets under the test folder.

Example:

```zsh
t 5 6
v 0 0 2
v 1 1 3
v 2 2 3
v 3 1 2
v 4 2 2
e 0 1
e 0 2
e 1 2
e 1 3
e 2 4
e 3 4
```

## Techniques Supported

The filtering methods that generate candidate vertex sets.

|Supported filtering methods | Description |
| :-----------------------------------: | :-------------: |
|LDF| the label degree filter |
|NLF| the neighborhood label frequency filter |
|CFL| the filtering method of CFL|
|DPiso| the filtering method of DP-iso |
|GQL| the filtering method of GQL |
|TSO| the filtering method of TurboIso |
|CaLiG| the filtering method of CaLiG |
|RM| the filtering method of RapidMatch |

The ordering methods that generate matching order.

|Supported ordering methods | Description |
| :-----------------------------------: | :-------------: |
|GQL| the ordering method of GraphQL |
|DPiso| the ordering method of DP-iso |
|RM| the ordering method of RapidMatch |

The enumeration methods that find all results.

|Supported engine methods | Description |
| :-----------------------------------: | :-------------: |
|BS1| Naive-Backtracking Search |
|BSX| Batch-Backtracking Search |
|RM| RapidMatch |
|KSS| Kernel and Shell |
|FiPE| Fine-grained and Powerful Equivalences |

Besides, we also integrate VEQ to this framework.
## Experiment Datasets

We have placed all the datasets used for testing in the paper at this link: [dataset_FiPE](https://github.com/Lu-Yujie/FiPE_dataset).

```bash
7z x FiPE_dataset.7z
```
