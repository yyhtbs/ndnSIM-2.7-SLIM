# ndnSIM-2.7-SLIM
Size Reduced ndnSIM 2.7, [38.4MB] [1291 files]

This ndnSIM version does not support wireless and visualisation simulations. 

# Why this version

The official ndnSIM is heavy but not rich. It contains lots of un-used and redundant modules/functions. Usually, it takes quite long time to compile the whole simulator. 

Removing unused ns3 modules will reduce the storage cost and accelerate its compiling speed.

However, it seems very difficult to only remove the unused modules because some of them are highly coupled. For example, the WIFI module heavily relies on the Internet module which is however not used by ndnSIM.

In this case, I removed all the heavily coupled modules (including WIFI, CSMA) and only remain the essential modules to support the fundmental functions of ndnSIM.

In the future, I plan to add back some useful wireless module by combing the dependencies between different modules. For example, to maintain only the minimum amount of codes for the unused but dependent modules. 

# Simulation Scope

This version is mainly for simulating congestion control algorithms and caching strategies.The wireless scenario can be mimic by changing the ppp link error rate. 

# Installation

The installation steps are the same as the official ndnSIM. 


# Acknowledgement
This code is a part of the owners' project "Monetized Information-Centric Transmission Control" [GOIPD/2019/874] funded by Irish Research Council.


