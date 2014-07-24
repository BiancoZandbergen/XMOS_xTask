xTask Distributed Operating System
==================================

xTask is a distributed Operating System for the XMOS XS1 multi-core microprocessor architecture.
Each processor core of the XMOS XS1 architecture supports up to eight hardware threads.
Multiple processor cores can be interconnected by high speed on-chip or off-chip processor buses.
The xTask Operating System allows to run multiple tasks within a single hardware thread under
supervision of a kernel that manages the resources available to a hardware thread such as the processor time.
Multiple hardware threads can be configured to run multiple tasks on each of them.
These hardware threads can be located on the same processor core or another.
Each hardware thread that runs multiple tasks under supervision of a kernel is connected to a Communication Server.
Each processor core has a Communication Server and all Communication Servers are connected with one another by means of a ring bus,
enabling communication throughout the system.
Tasks can create dedicated hardware threads and communicate with it indirectly through virtual channels managed by the Communication Server.
Tasks can also communicate with one another, it does not matter whether the two tasks are managed by the same kernel,
different kernels on the same processor core or on different kernels on different processor cores. 

see /doc for more info or www.xtask.org

<p align="center">
  <img src="http://biancozandbergen.github.io/images/xmos_xtask_1.png"/>
</p>