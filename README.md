# PriceLevel

> Experimental playground for building and benchmarking the **PriceLevel** component of a High-Frequency Trading system.

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Status: Experimental](https://img.shields.io/badge/status-experimental-orange.svg)](#)

---

## 📌 Overview

`PriceLevel` is a core building block of any limit order book (LOB). It represents all resting orders
at a single price point and must support **extremely fast** insert, cancel, match, and iteration
operations — often under strict latency budgets measured in nanoseconds.

This repository is a **research & development sandbox** for exploring different designs of the
`PriceLevel` component: from intrusive linked lists and custom memory pools to cache-friendly
layouts and lock-free approaches.

The goal is not to ship a production-ready library, but to **measure, compare, and learn**.

---

## 🎯 Goals

- Explore **intrusive data structures** for order storage (intrusive doubly-linked lists, skip lists, etc.)
- Benchmark **order pools** and custom allocators (slab, arena, freelist)
- Measure **cache locality** and branch-prediction impact on hot paths
- Compare **FIFO vs. pro-rata** matching semantics
- Evaluate trade-offs between **latency**, **throughput**, and **memory footprint**
- Keep the hot path **allocation-free** and **branch-predictable**

---

## 🧩 What's Inside (planned / WIP)

| Component | Description | Status |
|-----------|-------------|--------|
| `IntrusiveOrderList` | Doubly-linked list with intrusive hooks inside `Order` | 🚧 WIP |
| `OrderPool` | Fixed-size object pool for `Order` objects | 🚧 WIP |
| `PriceLevel` | Aggregates orders at a single price, supports add/cancel/match | 🚧 WIP |
| `MatchingPolicy` | FIFO / Pro-rata strategies | 📝 Planned |
| `Benchmarks` | Google Benchmark / custom microbenchmarks | 📝 Planned |

---

## 🏗️ Design Notes

Some of the ideas being explored:

- **Intrusive lists** avoid extra allocations and improve cache locality — the `Order` *is* the list node.
- **Object pools** eliminate `malloc`/`free` from the hot path.
- **`PriceLevel`** keeps only a head/tail pointer and a running total quantity, so matching is O(1) per fill.
- **Cache-line alignment** and **padding** to avoid false sharing in multi-threaded scenarios.

> ⚠️ These are experiments. APIs will change. Nothing here is battle-tested.

---

## 🚀 Building

```bash
git clone https://github.com/<your-username>/price-level.git
cd price-level
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j