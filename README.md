# Analysis, implementation, and optimization (x86-32 SSE, x86-64 AVX) of the CFS algorithm.

---

## 1. Introduction and Objectives
In data analysis, **Feature Selection** is crucial for reducing noise and improving model effectiveness by identifying the most relevant variables. This project focuses on the **Correlation Feature Selection (CFS)** algorithm, a powerful tool based on statistical correlation.

The goal is to explore implementation and optimization challenges, starting from high-level code (C) down to assembly instruction level optimization for x86 architectures.

---

## 2. Software Structure

### 2.1 Input and Parameters
The program is structured to accept input from binary files. The call must include:
* **Architecture:** Specifies whether 32-bit or 64-bit (`32c`, `64c`).
* **Parameter k:** Number of features to select.
* **File:** Dataset and labels file.
* **Options:** Flags for printing results.

### 2.2 C Implementation
The algorithm calculates a heuristic ("merit") to evaluate feature subsets. The main functions implemented in C include:

* **`calcola_rcf`**: Calculates the correlation between features and classes (labels). It uses the mean and standard deviation to determine how predictive a feature is.
* **`calcola_rff`**: Calculates the correlation between two features ($fx$ and $fy$). This function is computationally expensive as it iterates over the entire dataset to calculate the summations necessary for the Pearson formula:
    * `sommatoria1`: Sum of the products of the differences from the means.
    * `sommatoria2` and `sommatoria3`: Sum of the squared differences.
    * Returns: `sommatoria1 / sqrt(sommatoria2 * sommatoria3)`.
* **`calcola_merit`**: Combines $rcf$ and $rff$ to evaluate the merit of the current subset.

---

## 3. Optimization

### 3.1 Matrix Vectorization (Data Layout)
During the analysis, it emerged that data access could be optimized. The original code read the matrix by rows.
* **Modification:** The `load_data` function was rewritten to vectorize data by **columns**.
* **Advantage:** This layout favors linear memory access during the calculation of correlations between features, which is essential for subsequent Assembly optimizations.

### 3.2 x86-32 Assembly Implementation (SSE)
The critical function `calcola_rff` was rewritten in x86-32 Assembly using SSE extensions.

**Implementation Logic:**
1.  Loading pointers and means ($mux$, $muy$) into XMM registers (`xmm0`, `xmm1`).
2.  Main loop iterating over $N$ elements:
    * Scalar loading (`movss`) of the values of the two current features.
    * Subtraction of means (`subss`).
    * Accumulation of products and squares (`mulss`, `addss`) in accumulator registers (`xmm2`, `xmm3`, `xmm4`).
3.  Final calculation of square root (`sqrtss`) and division (`divss`).

**Note on Parallelization:**
An attempt at parallelization (processing 4 floats simultaneously) was made. Although it reduced the time to **0.001s**, it produced incorrect accuracy results (wrong score) and was discarded in the final version to preserve correctness.

### 3.3 x86-64 Assembly Implementation (AVX)
The 64-bit version uses AVX extensions (`calcola_rff_new`).

**Main Differences:**
* Use of 64-bit registers (`RDI`, `RSI`, `RDX`) for parameter passing.
* Use of VEX-encoded instructions (e.g., `vmovss`, `vsubss`, `vmulss`) which support three operands, reducing the need for explicit data movement instructions.
* The algorithmic logic remains identical to the 32-bit version, operating in scalar mode.

---

## 4. Benchmarks and Results

Tests were performed on a dataset of 5000 rows and 50 columns, extracting $k=5$ features.

| Implementation | Architecture | Execution Time | Notes |
| :--- | :--- | :--- | :--- |
| **Pure C** | x86-32 | 0.011 secs | Initial baseline. |
| **Assembly + SSE** | x86-32 | **0.005 secs** | Scalar optimization and column layout. |
| *Parallel Assembly* | *x86-32* | *0.001 secs* | openMP optimization. |
| **Assembly + AVX** | x86-64 | **0.006 secs** | 64-bit scalar version. |

### Example Output (Optimized x86-32)
```text
Dataset row number: 5000
Dataset column number: 50
Number of features to extract: 5
CFS time: 0.005 secs
sc: 0.053388, out: [45, 25, 7, 33, 47]
