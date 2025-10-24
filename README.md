# [Pearl: Performance-Aware Wear Leveling for Nonvolatile FPGAs](https://ieeexplore.ieee.org/document/9104706)

## Table of Contents

- [Introduction](#introduction)
- [Implementation Details](#implementation-details)
- [Usage](#usage)
- [Evaluation](#evaluation)
- [Results](#results)
- [Cite](#Cite)
- [License](#license)


## Introduction 

This implementation addresses the endurance problems in block random access memory (BRAM) of nonvolatile FPGAs. It proposes two wear leveling strategies - Coarse-Grained Wear Leveling (C-Pearl) and Fine-Grained Wear Leveling (F-Pearl) to enhance the FPGA's lifetime.


## Implementation Details

The implementation includes the following components:

- Coarse-Grained Wear Leveling (C-Pearl) algorithm
- Fine-Grained Wear Leveling (F-Pearl) algorithm
- Static analysis
- Wear leveling-guided placement
- Reconfiguration procedures
- Supportive circuit design

## Usage

## Evaluation

The code includes an evaluation section that demonstrates the performance of C-Pearl and F-Pearl compared to traditional wear leveling (TWL). Detailed results are available in the [Results](#results) section.

## Results

The evaluation results show that C-Pearl and F-Pearl achieve 34% and 46% higher lifetime improvement, respectively, with 8% and 11% lower performance overhead compared to TWL.


## Cite

If you use this code in your research or project, please cite the following paper:

```bibtex
@ARTICLE{9104706,
author={Zhang, Hao and Liu, Ke and Zhao, Mengying and Shen, Zhaoyan and Cai, Xiaojun and Jia, Zhiping},
journal={IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems},
title={Pearl: Performance-Aware Wear Leveling for Nonvolatile FPGAs},
year={2021},
volume={40},
number={2},
pages={274-286},
doi={10.1109/TCAD.2020.2998779}}
```


## License

This project is licensed under the [MIT License](LICENSE).

