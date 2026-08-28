# network programming with go

## general info

written by Adam Woodbeck and Jerremy Bowers, both senior lvl SWE's at [barracuda](https://www.barracuda.com/) a software company specializing in networking and security.

the book claims it will teach you how to write idiomatic go code to share data in a secure and reliable way.

## ch.1, an overview of networked systems

### topologies and nodes

- node -> some device on the network. usually a computer.
- topology -> a connection of nodes. has six types.
  - point-to-point, being a direct connection
    - a daisy chain is a bunch of point to points in a chain. Rare nowadays.
  - bus, where one link is used to send traffic to every other device. Common in wireless networks but the traffic is encrypted inw ireless networks so its not that big of an issue.
  - ring, where the traffic needs to flow through all the other intermediary nodes before going to the desired node
  - and star, which is where one controller gets the traffic and sends it to the other node. common in wired networks.
  - mesh: expensive but most reliable. untennable in large scale networks.
  - hybrid: most networks are hybrids nowadays. check the image.
    ![imgs/screenshot-471ba0bf14b9148de6bcf18edec9fe54.png](imgs/screenshot-471ba0bf14b9148de6bcf18edec9fe54.png)

### banwidth vs latency

- bandwidth is the raw capacity of the pipe. Throughput is the amount of water flowing through that pipe (the book doesnt talk about throughput).
- the book states that _latency must not be overlooked_. And that latency can come from a bunch of different sources, including database lookups, serverside rendering, the physical distance from the server to the client, or blocking on the server due to a lack of a well designed concurrency model (shameless go shill).

### The OSI (Open Systems Interconnection) _Reference_ Model

- Everybody knows the OSI layer. It's more educational than a strict representation of reality. Developed in the 1970s.
- The layers
  - L7 -> mostly for resources. applications use this.
  - L6 -> presentation layer. _Encryption, decryption, and data encoding are examples of l6 functions._
  - L5 -> session layer. supposed to be a protocol for keeping sessions alive. Outdated imo.
  - L4 -> everybodies favourite! the transport layer. Is used for the transport of data between two nodes. It controls the (un)reliability of the data transmitted through a network.

![imgs/screenshot-9f10e00de667d96cf28674f77e984f26.png](imgs/screenshot-9f10e00de667d96cf28674f77e984f26.png)

