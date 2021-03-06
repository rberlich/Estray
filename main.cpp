/**
 * @file main.cpp
 */

/*
 * The following license applies to the code in this file:
 *
 * **************************************************************************
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * **************************************************************************
 *
 * Author: Dr. Rüdiger Berlich of Gemfony scientific UG (haftungsbeschraenkt)
 * See http://www.gemfony.eu for further information.
 *
 * This code is based on the Beast Websocket library by Vinnie Falco.
 */

// Standard headers go here
#include <iostream>
#include <thread>
#include <fstream>
#include <vector>

// Boost headers go here
#include <boost/program_options.hpp>

// Application headers go here
#include "async_websocket_server.hpp"

namespace po = boost::program_options;

const payload_type   DEFAULTPAYLOADTYPE = payload_type::container;
const double         DEFAULTSLEEPTIME = 1.;
const std::size_t    DEFAULTCONTAINERSIZE = 1000;
const std::size_t    DEFAULTNACCEPT = 10000;
const unsigned short DEFAULTPORT = 10000;
const std::size_t    DEFAULTFULLQUEUESLEEPMS = 5;
const std::size_t    DEFAULTMAXQUEUESIZE = 5000;
const std::string    DEFAULTHOST = "127.0.0.1"; // localhost // NOLINT

/******************************************************************************************/

int main(int argc, char **argv) {
	bool           is_client = false;
	payload_type   pType = DEFAULTPAYLOADTYPE;
	double         payload_sleep_time = DEFAULTSLEEPTIME;
	std::size_t    container_size = DEFAULTCONTAINERSIZE;
	std::size_t    max_n_served = DEFAULTNACCEPT;
	std::size_t    n_producer_threads = 0;
	std::size_t    n_context_threads = 0;
	std::size_t    full_queue_sleep_ms = DEFAULTFULLQUEUESLEEPMS;
	std::size_t    max_queue_size = DEFAULTMAXQUEUESIZE;
	unsigned short port = DEFAULTPORT;
	std::string    host = DEFAULTHOST;
	std::size_t    client_id = 0;

	try {
		po::options_description desc("Available options");
		desc.add_options()
			("help,h", "This message")
			(
				"client", po::value<bool>(&is_client)->default_value(false)->implicit_value(true)
				, "Determine whether this is a client or server (the default)")
			(  "payload_type,p", po::value<payload_type>(&pType)->default_value(DEFAULTPAYLOADTYPE)
			   , R"(The type of payload to be used for the measurements. 0: "container_payload", 1: "sleep_payload".)")
			(
				"container_size,s", po::value<std::size_t>(&container_size)->default_value(DEFAULTCONTAINERSIZE)
				, "The desired size of each container_payload object")
			(  "payload_sleep_time,t", po::value<double>(&payload_sleep_time)->default_value(DEFAULTSLEEPTIME),
			   "The amount of time in seconds that each client with a sleep_payload should sleep")
			(
				"n_producer_threads,n", po::value<std::size_t>(&n_producer_threads)->default_value(std::thread::hardware_concurrency())
				, R"(The number of threads that will simultaneously produce payload objects. 0 uses "hardware_concurrency".)")
			(
				"n_context_threads,l", po::value<std::size_t>(&n_context_threads)->default_value(std::thread::hardware_concurrency())
				, R"(The number of threads used for the io_context. 0 uses "hardware_concurrency".)")
			(
				"max_n_served,m", po::value<std::size_t>(&max_n_served)->default_value(DEFAULTNACCEPT)
				, "The total number of packages served by the server")
			(  "full_queue_sleep_ms,f", po::value<std::size_t>(&full_queue_sleep_ms)->default_value(DEFAULTFULLQUEUESLEEPMS)
			   , "The amount of milliseconds a payload producer should pause when the queue is full")
			(  "max_queue_size,q", po::value<std::size_t>(&max_queue_size)->default_value(DEFAULTMAXQUEUESIZE)
			   , "The maximum size of the payload queue")
			(  "port", po::value<unsigned short>(&port)->default_value(DEFAULTPORT)
				, "The port to which a client should connect or on which the server should listen")
			(  "host", po::value<std::string>(&host)->default_value(DEFAULTHOST)
				, "IP or name of the host running the server")
            (  "client_id" , po::value<std::size_t>(&client_id)->default_value(0)
                , "A unique id to be assigned to the client to make it distinguishable in the output"
            )
			;

		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			return 0;
		}

		if (is_client) { // We are a client
		    std::cout << "Client with id " << client_id << " is starting up" << std::endl;

			// Use std::make_shared so shared_from_this works
			std::make_shared<async_websocket_client>(host, port)->run();

            std::cout << "Client with id " << client_id << " has terminated" << std::endl;
		} else { // We are a server
			auto start = std::chrono::system_clock::now();
			// Start the actual server and measure its runtime in milliseconds
			std::make_shared<async_websocket_server>(
				host
				, port
				, n_context_threads
				, n_producer_threads
				, max_n_served
				, pType
				, container_size
				, payload_sleep_time
				, full_queue_sleep_ms
				, max_queue_size
			)->run();
			auto end = std::chrono::system_clock::now();

			auto nMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();

			std::cout
			    << "Used " << nMilliseconds << " ms" << std::endl
			    << "This amounts to " << 1000*double(max_n_served)/double(std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count()) << " packages/s" << std::endl;
		}
	} catch (std::exception &e) {
		std::cerr << "Exception in main(): " << e.what() << std::endl;
		return 1;
	}

	return 0;
}

/******************************************************************************************/
