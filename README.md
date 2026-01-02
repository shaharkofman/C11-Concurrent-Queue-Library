# C11 Concurrent Queue Library

A lightweight, high-performance, thread-safe queue implementation written in C (C11 standard). This library provides a robust **Producer-Consumer** mechanism designed for multi-threaded applications, utilizing fine-grained locking and condition variables to eliminate busy waiting.

## 🚀 Key Features

* **Zero Busy Waiting:** Uses `cnd_wait` and `cnd_signal` to efficiently put consumer threads to sleep when the queue is empty.
* **Direct Handoff Architecture:** Implements an optimized path where producers hand data *directly* to waiting consumers, bypassing the internal data buffer entirely to reduce latency.
* **Dynamic Memory Management:** Fully dynamic linked-list storage (unbounded capacity) with strict memory cleanup.
* **Standard C11:** Built using `<threads.h>` for modern standard compliance (portable across POSIX systems).
* **Thread-Safe:** Full synchronization using mutexes ensures data integrity under high concurrency.

## 🛠 Architecture

The queue consists of two internal linked lists and a single synchronization lock:

1.  **Data Queue:** Holds items waiting to be processed.
2.  **Waiter Queue:** Holds threads waiting for data.

### The "Direct Handoff" Optimization
Unlike naive implementations that always push to a buffer, this queue checks the **Waiter Queue** first.

* **If a Consumer is waiting:** The Producer removes the Consumer from the wait list, places the data directly into the Consumer's stack-allocated struct, and signals it. This is O(1) and skips the overhead of `malloc/free` for the data node.
* **If no Consumer is waiting:** The Producer allocates a new node and appends it to the **Data Queue**.

## 📦 Installation & Usage

### 1. Integration
Simply include `queue.c` in your build process. There is no header file required as the interface is self-contained, but you can declare the prototypes in your own header.

### 2. Compilation
Compile with the `-pthread` flag (required for POSIX threads).

```bash
gcc -O3 -std=c11 -pthread main.c queue.c -o my_app
