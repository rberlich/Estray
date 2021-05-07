# Estray

This program serves as a testbed for the integration of the websocket part of Boost.Beast with Boost.Serialization, as developed by Vinnie Falco and Robert Ramey. A server produces payloads and, upon request from one or more clients, serializes them and transfers them to the peer. Calling the `process()` function of the payload-object performs an operation depending on the payload, of which the client knows nothing (except that the `process()` call exists). (De-)serialization always happens through the base class, so that client and server do not need to know specifics of the payload (but do need access to a factory function, which has some specific information, such as the constructor signature). When work items have been processed by the client, they are serialized again and shipped back to the server.

Payload-items are constantly produced by a configurable number of threads and added to a Boost.Lockfree-queue in the server, from where the server-side websocket sessions may extract and ship them to the clients.

_Two payloads have been implemented:_

* `random_container_payload` wraps a `std::vector` of objects holding a random number. Calling `process()` sorts the vector according to the values in the random number objects. The default size of the vector is 1000 objects, so that serialization is sufficiently complex to simulate a real work load. Processing (sorting) on the client side will take in the range of milliseconds, though, so that this payload may be used to test the case of very short client runtimes combined with frequent, comparatively large network transfers. This may serve as an indication of the worst case performance.
* `sleep_payload` does the opposite: The `process()` call sleeps for a configurable number of seconds before returning. Except for the sleep duration, the payload objects are empty, so that network transfers are short and very little effort is needed for serialization of the work item. This can be used to test the best case, i.e. long "processing" times on the client side with inexpensive transfers. While the `random_container_payload` performance may be dominated by the available CPU-power and network speed, the `sleep_payload` will likely be dominated by the performance of the websocket implementation.

Varying the vector size or the sleep time may help to calculate possible speedups under different scenarios, and might be useful for finding more efficient ways of using Boost.Beast and Boost.Serialization as well as exchanging larger workloads.

The code has been tested with a number of client machines jointly running up to 500 Websocket clients, each accessing the same octacore Ryzen system representing the server. The machines were connected by a fast network, but were located in different data centers.

The code is made available under the terms of the Boost Software License in version 1.0 in the hope that it

* will be useful to people exploring Boost.Beast, e.g. by serving as a more comprehensive starting point for your own work
* may evolve over time to show best practices when using Beast. Note that I DO NOT CLAIM that the code made available here represents in any way best practices. Rather, it currently reflects my own, personal and certainly limited understanding of how Boost.Beast may be used best to implement client-server systems for work-distribution on top of websockets. If you stumble upon areas where the code may be improved, please feel free to make suggestions, and I will aim to make improvements.
* may serve as the basis for further profiling of the simultaneous use of Boost.Beast and Boost.Serialization for more complex workloads.

See each file for the licensing information.

To get started, on a single Linux machine, call `Estray` without arguments (this will be the server) and with the argument `--client` in another shell. This will create a client-server communication using the `random_container_payload` and a single client. See the `scripts/startClients.sh` script for information on how to start multiple clients. The command-line option `--help` will show you additional options.

_Open Questions and Work Items:_

* The server-sessions need to interact with the server-object (e.g. check for stop-conditions, get payload objects from the queue held in the server object, ...). The necessary callbacks are handed to the async_websocket_client-constructors and are stored in the async_websocket_client object. This works o.k., but I wonder whether there are cleaner ways to do this (e.g. Boost.Signal2 ?)
* There is a lot of probably unnecessary copying of strings, which slows things down. It would be great if this could be replaced with string_view
* Sometimes, when a server is terminated, starting it again on the same port results in the error message "port is already in use"
 - The demo should use ASIOs means to deal with signals portably

_Caveats:_

* As the focus of this testbed is on Boost.Beast and Boost.Serialization and it is not possible to submit std::shared_ptr-objects to a Boost.Lockfree queue, payload items are accessed via raw pointers, so Estray has some "raw" new and delete-calls.
* The code has so far only been tested on Ubuntu 17.10, using g++ 7.2.0 and Boost 1.66 (which is the first version to ship with Boost.Beast). It compiles with Clang on MacOS X 10.13.2 but has not been tested there.
* No attempt has been made to test the code with C++11. It is possible that it will compile only with C++14 and above.
* No attempt has been made to compile the code on MS Windows so far.

*PLEASE DO NOTE THAT THE CODE WILL CONTAIN BUGS. USE AT YOUR OWN RISK.*

_Thanks:_

The author would like to thank Vinnie Falco and Robert Ramey for the hard work they put into Boost.Beast and Boost.Serialization!
