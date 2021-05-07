/**
 * @file async_webscket_server.hpp
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

#pragma once

// Standard headers go here
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <sstream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <string>
#include <thread>
#include <vector>
#include <cassert>

// Boost headers go here
#include <boost/spirit/include/qi.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/random.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/cstdint.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/policies.hpp>

// Our own headers go here
#include "payload.hpp"

/******************************************************************************************/
////////////////////////////////////////////////////////////////////////////////////////////
/******************************************************************************************/

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>

using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using error_code = boost::system::error_code;
using resolver = net::ip::tcp::resolver;
using close_code = websocket::close_code;
using frame_type = websocket::frame_type;
using string_view = beast::string_view;

/******************************************************************************************/
////////////////////////////////////////////////////////////////////////////////////////////
/******************************************************************************************/
// Report a failure
void
fail(beast::error_code ec, char const *what) {
    std::cerr << what << ": " << ec.message() << "\n";
}

/******************************************************************************************/
////////////////////////////////////////////////////////////////////////////////////////////
/******************************************************************************************/
/**
 * This client aims to always keep a read-operation active, so that it may react properly to
 * ping- and close-frames. User-initiated reads, writes and processing are NOT protected by
 * a strand, as the protocol between client and server-session is still serial, in the sense
 * that operations on command containers are always triggered first by the client. So a
 * user-initiated write on the client side is followed by a user-initiated read on the server
 * side, then a user-initiated write on the server side and a user-initiated read on the client
 * side. Control-frames (in particular pings and pongs) are sent back and forth in the background
 * and are protected by the implementation. It is crucial for this lack of protection that
 * the server-session side of the implementation remains serial. This is achieved by starting
 * a write only from the read-completion handler and vice versa.
 */
class async_websocket_client final
    : public std::enable_shared_from_this<async_websocket_client>
{
public:
    //--------------------------------------------------------------------------
    async_websocket_client() = delete;
    async_websocket_client(const async_websocket_client &) = delete;
    async_websocket_client(async_websocket_client &&) = delete;
    async_websocket_client &operator=(const async_websocket_client &) = delete;
    async_websocket_client &operator=(async_websocket_client &&) = delete;

    //--------------------------------------------------------------------------

    async_websocket_client(
        std::string address, unsigned short port
    )
        : m_io_context{2}
        , m_address{std::move(address)}
        , m_port{port}
    {
        // Set the auto_fragment option, so control frames are delivered timely
        m_ws.auto_fragment(true);
        m_ws.write_buffer_bytes(16384);

        // Set the transfer mode according to the defines in CMakeLists.txt
        set_transfer_mode(m_ws);
    }

    //--------------------------------------------------------------------------
    /**
     * Start the asynchronous operation
     */
    void
    run() {
        // Look up the domain name
        m_resolver.async_resolve(
                m_address,
                std::to_string(m_port),
                beast::bind_front_handler(
                        &async_websocket_client::when_resolved,
                        shared_from_this()));

        // We need an additional thread for the processing of incoming work items
        std::thread processing_thread(
                [this]() {
                    m_io_context.run();
                }
        );

        // This call will block until no more work remains in the ASIO work queue
        m_io_context.run();

        // When run() has finished, close all outstanding connections
        std::cout << "async_websocket_client::run(): Closing down remaining connections" << std::endl;
        processing_thread.join();

        // Close the websocket
        m_ws.close(websocket::close_code::normal);
    }

private:
    //--------------------------------------------------------------
    // Communication and processing

    void
    when_resolved(
        beast::error_code ec,
        const tcp::resolver::results_type &results
    ) {
        if (ec)
            return fail(ec, "resolve");

        // Set the timeout for the operation
        beast::get_lowest_layer(m_ws).expires_after(std::chrono::seconds(30));

        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(m_ws).async_connect(
                results,
                beast::bind_front_handler(
                        &async_websocket_client::when_connected,
                        shared_from_this()));
    }

    //--------------------------------------------------------------------------
    void
    when_connected(beast::error_code ec, const tcp::resolver::results_type::endpoint_type &ep) {
        if (ec)
            return fail(ec, "connect");

        // Turn off the timeout on the tcp_stream, because
        // the websocket stream has its own timeout system.
        beast::get_lowest_layer(m_ws).expires_never();

        // Set suggested timeout settings for the websocket
        m_ws.set_option(
                websocket::stream_base::timeout::suggested(
                        beast::role_type::client));

        // Set a decorator to change the User-Agent of the handshake
        m_ws.set_option(websocket::stream_base::decorator(
                [](websocket::request_type &req) {
                    req.set(http::field::user_agent,
                            std::string(BOOST_BEAST_VERSION_STRING) +
                            " async_websocket_client ");
                }));

        // Update the m_address string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        m_address += (':' + std::to_string(m_port));

        // Perform the websocket handshake
        m_ws.async_handshake(m_address, "/",
                             beast::bind_front_handler(
                                     &async_websocket_client::when_handshake_succeeded,
                                     shared_from_this()));
    }

    //--------------------------------------------------------------------------
    void
    async_start_write(const std::string &message) {
        // Prepare the buffer for the next iteration
        m_out_buffer.clear();

        // Fill it with the message
        beast::ostream(m_out_buffer) << message;

        // Send the message
        m_ws.async_write(
                m_out_buffer.data(),
                beast::bind_front_handler(
                        &async_websocket_client::when_written,
                        shared_from_this())
        );
    }

    //--------------------------------------------------------------------------
    void
    async_start_read() {
        // Do the next read
        m_ws.async_read(
                m_in_buffer,
                beast::bind_front_handler(
                        &async_websocket_client::when_read,
                        shared_from_this())
        );
    }

    //--------------------------------------------------------------------------
    void
    when_handshake_succeeded(beast::error_code ec) {
        if (ec)
            return fail(ec, "handshake");

        // Ask the server for data
        async_start_write(
                m_command_container.reset(
                        payload_command::GETDATA
                ).to_string()
        );

        // Start the read cycle -- it will keep itself alive
        // Beast and ASIO allow reads and writes to happen concurrently to each other.
        // However, care must be taken that no two reads (or writes) may run in parallel.
        async_start_read();
    }

    //--------------------------------------------------------------------------
    void
    when_written(
        beast::error_code ec,
        std::size_t bytes_transferred
    ) {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "when_written");

        // We are done. Further writing is triggered by the task processing
        m_out_buffer.clear();
    }

    //--------------------------------------------------------------------------
    void
    when_read(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "when_read");

        // Start asynchronous processing of the work item.
        // The next write-operation is initiated from process_request().
        m_io_context.post(
                beast::bind_front_handler(
                        &async_websocket_client::process_request,
                        shared_from_this(),
                        std::move(beast::buffers_to_string(m_in_buffer.data()))
                        )
        );

        m_in_buffer.clear(); // We can clear the old message

        // Start a new read cycle so we may react to control frames
        // (in particular ping and close)
        async_start_read();
    }

    //--------------------------------------------------------------------------
    void
    process_request(std::string in_data) {
        // De-serialize the object
        m_command_container.from_string(in_data);

        // Extract the command
        auto inboundCommand = m_command_container.get_command();

        // Act on the command received
        switch (inboundCommand) {
            case payload_command::COMPUTE: {
                // Process the work item
                m_command_container.process();

                // Set the command for the way back to the server
                m_command_container.set_command(payload_command::RESULT);
            }
                break;

            case payload_command::NODATA: // This must be a command payload
            case payload_command::ERROR: { // We simply ask for new work
                // sleep for a short while (between 10 and 50 milliseconds, randomly),
                // before we ask for new work, so the server is not bombarded with requests.
                std::uniform_int_distribution<> dist(10, 50);
                std::this_thread::sleep_for(std::chrono::milliseconds(dist(m_rng_engine)));

                // Tell the server again we need work
                m_command_container.reset(payload_command::GETDATA);
            }
                break;

            default: {
                throw std::runtime_error(
                        "async_websocket_client::process_request(): Got unknown or invalid command " +
                        boost::lexical_cast<std::string>(inboundCommand)
                );
            }
        }

        // Serialize the object again and return the result
        async_start_write(m_command_container.to_string());
    }

    //--------------------------------------------------------------------------
    // Data

    net::io_context m_io_context; ///< The io_context is required for all I/O

    tcp::resolver m_resolver{net::make_strand(m_io_context)};
    websocket::stream<beast::tcp_stream> m_ws{net::make_strand(m_io_context)};

    beast::flat_buffer m_out_buffer;
    beast::flat_buffer m_in_buffer;

    std::string m_address;
    unsigned short m_port;

    std::random_device m_nondet_rng; ///< Source of non-deterministic random numbers
    std::mt19937 m_rng_engine{m_nondet_rng()}; ///< The actual random number engine, seeded my m_nondet_rng

    command_container m_command_container{payload_command::NONE,
                                          nullptr}; ///< Holds the current command and payload (if any)

    //--------------------------------------------------------------------------
};

/******************************************************************************************/
////////////////////////////////////////////////////////////////////////////////////////////
/******************************************************************************************/
/**
 * Instances of this class are started for each incoming client connection. They handle
 * all communication with the respective client. Note that no seperate in- and out-buffers
 * are needed, as read- and write-operations happen sequentially.
 */
class async_websocket_server_session final
    : public std::enable_shared_from_this<async_websocket_server_session>
{
public:
    //--------------------------------------------------------------------------

    async_websocket_server_session(tcp::socket &&socket, // Take ownership of the socket
                                   std::function<bool(payload_base *&plb_ptr)> &&get_next_payload_item,
                                   std::function<bool()> &&check_server_stopped,
                                   std::function<void(bool)> &&server_sign_on
    )
        : m_ws(std::move(socket))
        , f_get_next_payload_item(std::move(get_next_payload_item))
        , f_check_server_stopped(std::move(check_server_stopped))
        , f_server_sign_on(std::move(server_sign_on))
    {
        // Set the auto_fragment option, so control frames are delivered timely
        m_ws.auto_fragment(true);
        m_ws.write_buffer_bytes(16384);

        // Set the transfer mode according to the defines in CMakeLists.txt
        set_transfer_mode(m_ws);
    }

    //--------------------------------------------------------------------------
    // Start the asynchronous operation
    void
    async_start_run() {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(m_ws.get_executor(),
                      beast::bind_front_handler(
                              &async_websocket_server_session::when_run_started,
                              shared_from_this()));
    }

private:
    //--------------------------------------------------------------------------

    void
    when_run_started() {
        // Set suggested timeout settings for the websocket
        m_ws.set_option(
                websocket::stream_base::timeout::suggested(
                        beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        m_ws.set_option(websocket::stream_base::decorator(
                [](websocket::response_type &res) {
                    res.set(http::field::server,
                            std::string(BOOST_BEAST_VERSION_STRING) +
                            " async_websocket_server_session");
                }));


        // Accept the websocket handshake
        m_ws.async_accept(
                beast::bind_front_handler(
                        &async_websocket_server_session::when_connection_accepted,
                        shared_from_this()));
    }

    //--------------------------------------------------------------------------

    void
    when_connection_accepted(beast::error_code ec) {
        if (ec) return do_close(ec, "when_connection_accepted");

        // Make it known to the server that a new session is alive
        f_server_sign_on(true);

        // Read a message into our buffer
        async_start_read();
    }

    //--------------------------------------------------------------------------

    void
    async_start_read() {
        // Read a message into our buffer
        m_ws.async_read(
                m_buffer,
                beast::bind_front_handler(
                        &async_websocket_server_session::when_read,
                        shared_from_this()));
    }

    //--------------------------------------------------------------------------

    void
    async_start_write() {
        m_ws.async_write(
                m_buffer.data(),
                beast::bind_front_handler(
                        &async_websocket_server_session::when_written,
                        shared_from_this()));
    }

    //--------------------------------------------------------------------------

    void
    when_read(
            beast::error_code ec,
            std::size_t /* nothing */
    ) {
        // This indicates that the session was closed
        if (ec == websocket::error::closed) return;

        // Act on errors
        if (ec) return do_close(ec, "when_read");

        // process the request. This will read out
        // m_buffer and fill it with new data
        process_request();

        // Send the next buffer back
        async_start_write();
    }

    //--------------------------------------------------------------------------

    void
    when_written(
            beast::error_code ec,
            std::size_t /* nothing */) {
        if (ec)
            return fail(ec, "when_written");

        // Clear the buffer
        m_buffer.clear();

        if (this->f_check_server_stopped()) {
            std::cout << "Server is stopped" << std::endl;
            // Do not continue if a stop criterion was reached
            return;
        } else {
            // Start another read cycle
            async_start_read();
        }
    }

    //--------------------------------------------------------------------------

    std::string getAndSerializeWorkItem() {
        // Obtain a container_payload object from the queue, serialize it and send it off
        payload_base *plb_ptr = nullptr;
        if (this->f_get_next_payload_item(plb_ptr) && plb_ptr != nullptr) {
            m_command_container.reset(payload_command::COMPUTE, plb_ptr);
        } else {
            // Let the remote side know whe don't have work
            m_command_container.reset(payload_command::NODATA);
        }

        return m_command_container.to_string();
    }

    //--------------------------------------------------------------------------

    void process_request() {
        // De-serialize the object
        try {
            m_command_container.from_string(beast::buffers_to_string(m_buffer.data()));
            m_buffer.clear(); // Clear the buffer, so we may later fill it with data to be sent
        } catch (...) {
            throw std::runtime_error(
                    "async_websocket_server_session::process_request(): Caught exception while de-serializing");
        }

        // Extract the command
        auto inboundCommand = m_command_container.get_command();

        // Act on the command received
        switch (inboundCommand) {
            case payload_command::GETDATA:
            case payload_command::ERROR: {
                boost::beast::ostream(m_buffer) << getAndSerializeWorkItem();
            }
                return;

            case payload_command::RESULT: {
                // Check that work was indeed done
                if (!m_command_container.is_processed()) {
                    throw std::runtime_error(
                            "async_websocket_server_session::process_request(): Returned payload is unprocessed");
                }
                boost::beast::ostream(m_buffer) << getAndSerializeWorkItem();
            }
                return;

            default: {
                throw std::runtime_error(
                        "async_websocket_server_session::process_request(): Got unknown or invalid command "
                        + boost::lexical_cast<std::string>(inboundCommand)
                );
            }
        }
    }

    //--------------------------------------------------------------------------

    void do_close(
            beast::error_code ec, const std::string &where
    ) {
        std::cout
                << "async_websocket_server_session:" << std::endl
                << "Closing down session from " << where << std::endl
                << "with error code " << ec.message() << std::endl;

        if (m_ws.is_open()) {
            // Close the connection
            m_ws.close(beast::websocket::close_code::protocol_error);
        }

        // Make it known to the server that a session is leaving
        f_server_sign_on(false);
    }

    //--------------------------------------------------------------------------
    // Data

    websocket::stream<beast::tcp_stream> m_ws;

    std::function<bool(payload_base *&plb_ptr)> f_get_next_payload_item;
    std::function<bool()> f_check_server_stopped;
    std::function<void(bool)> f_server_sign_on;

    command_container m_command_container{payload_command::NONE,
                                          nullptr}; ///< Holds the current command and payload (if any)

    beast::flat_buffer m_buffer;

    //--------------------------------------------------------------------------
};

/******************************************************************************************/
////////////////////////////////////////////////////////////////////////////////////////////
/******************************************************************************************/

class async_websocket_server final
    : public std::enable_shared_from_this<async_websocket_server>
{
    // --------------------------------------------------------------
    // Simplify usage of namespaces
    using error_code = boost::system::error_code;

public:
    // --------------------------------------------------------------
    // Deleted default constructor, copy-/move-constructors and assignment operators.
    // We want to enforce the usage of a single, specialized constructor.

    async_websocket_server() = delete;
    async_websocket_server(const async_websocket_server &) = delete;
    async_websocket_server(async_websocket_server &&) = delete;
    async_websocket_server &operator=(const async_websocket_server &) = delete;
    async_websocket_server &operator=(async_websocket_server &&) = delete;

    // --------------------------------------------------------------
    // External "API"

    async_websocket_server(
        const std::string &address
        , unsigned short port
        , std::size_t n_context_threads
        , std::size_t n_producer_threads
        , std::size_t n_max_packages_served
        , payload_type payload_type
        , std::size_t container_size
        , double sleep_time
        , std::size_t full_queue_sleep_ms
        , std::size_t max_queue_size
    )
        : m_endpoint(net::ip::make_address(address), port)
        , m_n_listener_threads(n_context_threads > 0 ? n_context_threads : std::thread::hardware_concurrency())
        , m_n_max_packages_served(n_max_packages_served)
        , m_payload_type(payload_type)
        , m_n_producer_threads(n_producer_threads > 0 ? n_producer_threads : std::thread::hardware_concurrency())
        , m_container_size(container_size)
        , m_sleep_time(sleep_time)
        , m_full_queue_sleep_ms(full_queue_sleep_ms)
        , m_max_queue_size(max_queue_size)
        , m_payload_queue{m_max_queue_size}
    { /* nothing */ }

    void run() {
        beast::error_code ec;

        // Reset the package counter
        m_n_packages_served = 0;

        // Indicate that the server is entering the run-state
        m_server_stopped = false;

        // Open the acceptor
        m_acceptor.open(m_endpoint.protocol(), ec);
        if (ec) return fail(ec, "run() / m_acceptor.open()");

        // Bind to the server address
        m_acceptor.bind(m_endpoint, ec);
        if (ec) return fail(ec, "run() / m_acceptor.bind()");

        // Start listening for connections
        m_acceptor.listen(net::socket_base::max_listen_connections, ec);
        if (ec) return fail(ec, "run() / m_acceptor.listen()");

        // Start producers
        m_producer_threads_vec.reserve(m_n_producer_threads);
        switch (m_payload_type) {
            //------------------------------------------------
            case payload_type::container: {
                for (std::size_t i = 0; i < m_n_producer_threads; i++) {
                    m_producer_threads_vec.emplace_back(
                            std::thread(
                                    [this](std::size_t container_size, std::size_t full_queue_sleep_ms) {
                                        this->container_payload_producer(container_size, full_queue_sleep_ms);
                                    }, m_container_size, m_full_queue_sleep_ms
                            )
                    );
                }
            }
                break;

                //------------------------------------------------
            case payload_type::sleep: {
                for (std::size_t i = 0; i < m_n_producer_threads; i++) {
                    m_producer_threads_vec.emplace_back(
                            std::thread(
                                    [this](double sleep_time, std::size_t full_queue_sleep_ms) {
                                        this->sleep_payload_producer(sleep_time, full_queue_sleep_ms);
                                    }, m_sleep_time, m_full_queue_sleep_ms
                            )
                    );
                }
            }
                break;

                //------------------------------------------------
            case payload_type::command: { // This is a severe error
                throw std::runtime_error(R"(async_websocket_server::run(): Got invalid payload_type "command")");
            }
                break;

                //------------------------------------------------
        }

        //---------------------------------------------------------------------------
        // And ... action!

        // Will return immediately
        async_start_accept();

        // Allow to serve requests from multiple threads
        m_context_thread_vec.reserve(m_n_listener_threads - 1);
        for (std::size_t t_cnt = 0; t_cnt < (m_n_listener_threads - 1); t_cnt++) {
            m_context_thread_vec.emplace_back(
                    std::thread(
                            [this]() {
                                this->m_io_context.run();
                            }
                    )
            );
        }

        // Block until all work is done
        m_io_context.run();

        //---------------------------------------------------------------------------
        // Wait for the server to shut down

        // Make sure the stop flag has really been set
        assert(true == this->m_server_stopped);

        // Wait for context threads to finish
        for (auto &t: m_context_thread_vec) { t.join(); }
        m_context_thread_vec.clear();

        // Wait for producer threads to finish
        for (auto &t: m_producer_threads_vec) { t.join(); }
        m_producer_threads_vec.clear();
    }

private:
    // --------------------------------------------------------------
    // Communication and data retrieval

    void async_start_accept() {
        // The new connection gets its own strand
        m_acceptor.async_accept(
                net::make_strand(m_io_context),
                beast::bind_front_handler(
                        &async_websocket_server::when_accepted,
                        shared_from_this()));
    }

    void when_accepted(beast::error_code ec, tcp::socket socket) {
        if (m_server_stopped) return;

        if (ec) {
            fail(ec, "when accepted");
        } else {
            // Create the async_websocket_server_session and async_start_run it. This call will return immediately.
            std::make_shared<async_websocket_server_session>(
                    std::move(socket),
                    [this](payload_base *&plb_ptr) -> bool { return this->getNextPayloadItem(plb_ptr); },
                    [this]() -> bool { return this->server_stopped(); }, [this](bool sign_on) {
                        if (sign_on) {
                            this->m_n_active_sessions++;
                        } else {
                            if (0 == this->m_n_active_sessions) {
                                throw std::runtime_error(
                                        "In async_websocket_server::when_accepted(): Tried to decrement #sessions which is already 0");
                            } else {
                                // This won't help, though, if m_n_active_sessions becomes 0 after the if-check
                                this->m_n_active_sessions--;
                            }
                        }

                        std::cout << this->m_n_active_sessions << " active sessions" << std::endl;
                    }
            )->async_start_run();
        }

        // Accept another connection
        if (!this->m_server_stopped) async_start_accept();
    }

    bool getNextPayloadItem(payload_base *&plb_ptr) {
        // Retrieve a new item, then update counters and the stop flag
        if (m_payload_queue.pop(plb_ptr)) {
            if (m_n_packages_served++ < m_n_max_packages_served) {
                if (m_n_packages_served % 10 == 0) {
                    std::cout << "async_websocket_server served " << m_n_packages_served << " packages" << std::endl;
                }
            } else { // Leave
                // Indicate to all parties that we want to stop
                m_server_stopped = true;
                // Stop accepting new connections
                m_acceptor.close();
            }

            return true;
        }

        // Let the audience know
        return false;
    }

    void container_payload_producer(
        std::size_t containerSize
        , std::size_t full_queue_sleep_ms
    ) {
        std::random_device nondet_rng;
        std::mt19937 mersenne(nondet_rng());
        std::normal_distribution<double> normalDist(0., 1.);

        bool produce_new_container = true;
        random_container_payload *sc_ptr = nullptr;
        while (true) {
            using namespace std::literals;

            // Only create a new container if the old one was
            // successfully added to the queue
            if (produce_new_container) {
                sc_ptr = new random_container_payload(containerSize, normalDist, mersenne);
            }

            if (!m_payload_queue.push(sc_ptr)) { // Container could not be added to the queue
                if (this->m_server_stopped) break;
                produce_new_container = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(full_queue_sleep_ms));
            } else {
                produce_new_container = true;
            }
        }
    }

    void sleep_payload_producer(
        double sleep_time
        , std::size_t full_queue_sleep_ms
    ) {
        bool produce_new_container = true;
        sleep_payload *sp_ptr = nullptr;
        while (!this->m_server_stopped) {
            using namespace std::literals;

            // Only create a new container if the old one was
            // successfully added to the queue
            if (produce_new_container) {
                sp_ptr = new sleep_payload(sleep_time);
            }

            if (!m_payload_queue.push(sp_ptr)) { // Container could not be added to the queue
                if (this->m_server_stopped) break;
                produce_new_container = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(full_queue_sleep_ms));
            } else {
                produce_new_container = true;
            }
        }
    }

    bool server_stopped() const {
        return this->m_server_stopped.load();
    }

    // --------------------------------------------------------------
    // Data and Queues

    net::ip::tcp::endpoint m_endpoint;
    std::size_t m_n_listener_threads;
    net::io_context m_io_context{boost::numeric_cast<int>(m_n_listener_threads)};
    net::ip::tcp::acceptor m_acceptor{m_io_context};
    std::vector<std::thread> m_context_thread_vec;
    std::atomic<std::size_t> m_n_active_sessions{0};
    std::atomic<std::size_t> m_n_packages_served{0};
    const std::size_t m_n_max_packages_served;
    const std::size_t m_full_queue_sleep_ms = 5;
    const std::size_t m_max_queue_size = 5000;

    std::atomic<bool> m_server_stopped{false};

    payload_type m_payload_type = payload_type::container; ///< Indicates which sort of payload should be produced

    std::size_t m_n_producer_threads = 4;
    std::vector<std::thread> m_producer_threads_vec; ///< Holds threads used to produce payload packages

    std::size_t m_container_size = 1000; ///< The size of container_payload objects
    double m_sleep_time = 1.; ///< The sleep time of sleep_payload objects

    // Holds payloads to be passed to the sessions
    boost::lockfree::queue<payload_base *, boost::lockfree::fixed_sized<true>> m_payload_queue;

    // --------------------------------------------------------------
};

/******************************************************************************************/
////////////////////////////////////////////////////////////////////////////////////////////
/******************************************************************************************/
