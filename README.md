# C++ implementation of OCPP

![Github Actions](https://github.com/EVerest/libocpp/actions/workflows/build_and_test.yaml/badge.svg)

Libocpp is a comprehensive C++ library implementation of the Open Charge Point Protocol (OCPP) for versions 1.6 and 2.0.1. 

OCPP is a standardized communication protocol for electric vehicle charging stations and central management systems, developed by the Open Charge Alliance (OCA). For more information on OCPP, visit the [OCA website](https://openchargealliance.org/protocols/open-charge-point-protocol/).

### Development Status:
| OCPP 2.0.1 | OCPP 1.6 |
|:---|---|
|<ul><li>Implementation is under active development</li><li>Core functionality is complete</li><li>Smart Charging and ISO 15118 support are in progress</li><li>Ongoing testing with various CSMS and against OCTT2 (OCPP Compliance Test Tool)</li></ul> | <ul><li>Complete implementation available</li><li>Supports all feature profiles: Core, Firmware Management, Local Auth List Management, Reservation, Smart Charging, and Remote Trigger</li><li>Fully tested with numerous Charging Station Management Systems (CSMS)</li><li>Enables communication for one charging station and multiple Electric Vehicle Supply Equipment (EVSE) using a single WebSocket connection</li></ul> |

## Table of Contents

| <a href="OCPP-2-0-1.md"  target="_blank">OCPP 2.0.1</a> | [OCPP 1.6](OCPP-1-6.md) |
|:---|:---|
| <ol><li>Quickstart for OCPP 2.0.1</li><li>Run OCPP 2.0.1 with EVerest</li><li>Integrate OCPP 2.0.1 library with charging station</li><li>Certification Profile Support</li><li>CSMS Compatibility</li><li>Database Initialization</li></ol> | <ol><li>Quickstart for OCPP 1.6</li><li>Run OCPP 1.6 with EVerest</li><li>Integrate OCPP 1.6 library with charging station</li><li>Feature Profile Support</li><li>CSMS Compatibility</li><li>Database Initialization</li></ol> |

For detailed information on each OCPP version, please refer to their respective README files linked above.


## Get Involved

We welcome contributions and feedback! To get involved with the EVerest project and this OCPP implementation, please refer to our [COMMUNITY.md](https://github.com/EVerest/EVerest/blob/main/COMMUNITY.md) and [CONTRIBUTING.md](https://github.com/EVerest/EVerest/blob/main/CONTRIBUTING.md) guides.

Your involvement helps drive the future of open-source EV charging infrastructure!
