# Fine-grained and Powerful Equivalences

Subgraph matching, a cornerstone of graph analytics, critically suffers from redundant computations during the search process.
Existing methods primarily target identical computations—redundant operations that are localized to individual query vertices—but fail to address similar redundancies that recur across multiple query vertices. In this paper, we present a novel algorithm, called FiPE, that accelerates subgraph matching through __Fine-grained and Powerful Equivalences__. FiPE redefines redundancy elimination by shifting the optimization granularity from isolated vertices to vertex pairs and multiple vertex patterns. It introduces _vertex-pair equivalence_ to cluster candidate pairs with isomorphic neighbor structures, even if their individual vertices differ, enabling pruning of similar computations between adjacent query vertices. FiPE proposes _group equivalence_ to defer equivalence checks to later search depths, capturing potential redundancies incrementally. To fully exploit the advantages of the equivalence, we introduce two optimization techniques: a matching order generation method to reduce the overall search space and an efficient conflict resolution mechanism to avoid two query vertices being mapped to the same data vertex. Experiments on real-world graphs highlight the superiority of FiPE. FiPE achieves a speedup of 2 to 3 orders of magnitude on various graphs under the EPS (embeddings per second) metric.

## Compile

Under the root directory of the project, execute the following commands to compile the source code.

```zsh
mkdir build
cd build
cmake ..
make
```

## Correctness Verification

We provide 200 test cases along with the corresponding test script. The usage is as follows:

```bash
python test.py ../build/matching/BS
```

If all 200 cases pass correctly, the following text will be displayed:

```bash
{your_method_name} engine passed the correctness check.
```

Additionally, we provide a script(_check_result.py_) for comparing results between different methods as an auxiliary tool for verifying the correctness of the code. Please refer to the script comments for specific usage instructions.

## Execute

After compiling the source code, you can find the binary file 'BS' under the 'build/matching' directory.
Execute the binary with the following command ./BS -d data_graph -q query_graph
-filter filter_technique -order order_technique -engine engine_technique -num max_number_of_embeddings -time_limit max_execute_time,
in which -d specifies the input of the data graphs and -q specifies the input of the query graphs.
The -filter parameter gives the filtering method, the -order specifies the ordering method, and the -engine
sets the enumeration method. The -num parameter sets the maximum number of embeddings that you would like to find. The time_time constrains the maximum execution time. If the number of embeddings enumerated reaches the limit or all the results have been found or the time limit is reached, then the program will terminate.
Set -num as 'MAX' to find all results.

Example (Use the filtering method of CFL and order method of GraphQL to generate the candidate vertex sets and the matching order respectively.
Enumerate results with the set-intersection based local candidate computation method):

```zsh
./BS -d ../../test/sample_dataset/test_case_1.graph -q ../../test/sample_dataset/query1_positive.graph -filter CFL -order GQL -engine General -num MAX
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

|Parameter of Command Line (-filter) | Description |
| :-----------------------------------: | :-------------: |
|LDF| the label degree filter |
|NLF| the neighborhood label frequency filter |
|CFL| the filtering method of CFL|
|DPiso| the filtering method of DP-iso |

The ordering methods that generate matching order.

|Parameter of Command Line (-order) | Description |
| :-----------------------------------: | :-------------: |
|GQL| the ordering method of GraphQL |

The enumeration methods that find all results.

|Parameter of Command Line (-engine) | Description |
| :-----------------------------------: | :-------------: |
|General| Naive-Backtracking Search |
|FiPE| Fine-grained and Powerful Equivalences |

## Experiment Datasets

We have placed all the datasets used for testing in the paper at this link: [dataset_FiPE](https://anonymous.4open.science/status/FiPE_dataset-D5CD).

```bash
7z x FiPE_dataset.7z
```
