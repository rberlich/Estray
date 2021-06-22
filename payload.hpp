/**
 * @file payload.hpp
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
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <random>
#include <algorithm>

// Boost headers go here
#include <boost/function.hpp>
#include <boost/bind/bind.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/tracking.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/export.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/cstdint.hpp>
#include <boost/filesystem.hpp>

// Our own headers go here
#include "misc.hpp"

/******************************************************************************************/
////////////////////////////////////////////////////////////////////////////////////////////
/******************************************************************************************/

class payload_base {
    ///////////////////////////////////////////////////////////////
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive &ar, const unsigned int) { /* nothing */ }

    ///////////////////////////////////////////////////////////////

public:
    payload_base() = default;

    virtual ~payload_base() = default;

    payload_base(const payload_base &) = default;

    void process() {
        this->process_();
    }

    bool is_processed() {
        return this->is_processed_();
    }

private:
    virtual void process_() = 0;

    virtual bool is_processed_() = 0;
};

/******************************************************************************************/
////////////////////////////////////////////////////////////////////////////////////////////
/******************************************************************************************/

// Saving and loading of payload_base-derivatives through the base pointer
std::string to_string(const payload_base *payload_ptr);
payload_base *from_string(const std::string &descr);

// For debugging purposes: Direct output in XML and binary format
std::string to_xml(const payload_base *payload_ptr);

std::string to_binary(const payload_base *payload_ptr);
payload_base *from_binary(const std::string &descr);

/******************************************************************************************/
////////////////////////////////////////////////////////////////////////////////////////////
/******************************************************************************************/

class command_container {
    ///////////////////////////////////////////////////////////////
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive &ar, const unsigned int) {
        ar
        & BOOST_SERIALIZATION_NVP(m_command)
        & BOOST_SERIALIZATION_NVP(m_payload_ptr);
    }
    ///////////////////////////////////////////////////////////////

public:
    explicit command_container(payload_command command)
            : m_command(command) { /* nothing */ }

    command_container(
            payload_command command, payload_base *payload_ptr
    )
            : m_command(command), m_payload_ptr(payload_ptr) { /* nothing */ }


    ~command_container() {
        delete m_payload_ptr;
    }

    command_container &operator=(command_container &&cp) noexcept {
        m_command = cp.m_command;
        cp.m_command = payload_command::NONE;
        delete m_payload_ptr;
        m_payload_ptr = cp.m_payload_ptr;
        cp.m_payload_ptr = nullptr;

        return *this;
    }

    // Deleted copy-constructors and assignment operator -- the class is non-copyable
    command_container(const command_container &) = delete;

    command_container &operator=(const command_container &) = delete;

    // Reset to a new command and payload or clear the object
    const command_container &reset(
            payload_command command, payload_base *payload_ptr = nullptr
    ) {
        m_command = command;

        delete m_payload_ptr;

        m_payload_ptr = payload_ptr;

        return *this;
    }

    // Access to the command
    void set_command(payload_command command) {
        m_command = command;
    }

    payload_command get_command() const noexcept {
        return m_command;
    }

    // Processing of the payload (if any)
    void process() {
        if (m_payload_ptr) {
            m_payload_ptr->process();
        } else {
            throw std::runtime_error("command_container::process(): No processing possible as m_payload_ptr is empty.");
        }
    }

    bool is_processed() {
        if (m_payload_ptr) {
            return m_payload_ptr->is_processed();
        } else {
            return false;
        }
    }

    std::string to_string() const {
        // Reset the internal stream
#ifdef BINARYARCHIVE
        std::stringstream(std::ios::out | std::ios::binary).swap(m_stringstream);
#else
        std::stringstream(std::ios::out).swap(m_stringstream);
#endif

        {
#if defined(BINARYARCHIVE)
            boost::archive::binary_oarchive oa(m_stringstream);
#elif defined(XMLARCHIVE)
            boost::archive::xml_oarchive oa(m_stringstream);
#elif defined(TEXTARCHIVE)
        boost::archive::text_oarchive oa(m_stringstream);
#else
        boost::archive::xml_oarchive oa(m_stringstream);
#endif
            oa << boost::serialization::make_nvp("command_container", *this);
        } // archive and stream closed at end of scope

        return m_stringstream.str();
    }

    void from_string(const std::string &descr) {
        command_container local_command_container{payload_command::NONE};

        // Reset the internal stream
#ifdef BINARYARCHIVE
        std::stringstream(descr, std::ios::in | std::ios::binary).swap(m_stringstream);
#else
        std::stringstream(descr, std::ios::in).swap(m_stringstream);
#endif

        {
#if defined(BINARYARCHIVE)
            boost::archive::binary_iarchive ia(m_stringstream);
#elif defined(XMLARCHIVE)
            boost::archive::xml_iarchive ia(m_stringstream);
#elif defined(TEXTARCHIVE)
        boost::archive::text_iarchive ia(m_stringstream);
#else
        boost::archive::xml_iarchive ia(m_stringstream);
#endif
            ia >> boost::serialization::make_nvp("command_container", local_command_container);
        } // archive and stream closed at end of scope

        // Move the data from local_command_container
        *this = std::move(local_command_container);
    }

    std::string to_xml() const {
        // Reset the internal stream
        std::stringstream(std::ios::out).swap(m_stringstream);

        {
            boost::archive::xml_oarchive oa(m_stringstream);
            oa << boost::serialization::make_nvp("command_container", *this);
        } // archive and stream closed at end of scope

        return m_stringstream.str();
    }

private:
    command_container() = default;

    // Data
    payload_command m_command{payload_command::NONE};
    payload_base *m_payload_ptr{nullptr};

    mutable std::stringstream m_stringstream;
};

/******************************************************************************************/
////////////////////////////////////////////////////////////////////////////////////////////
/******************************************************************************************/

class stored_number {
    ///////////////////////////////////////////////////////////////
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive &ar, const unsigned int) {
        using boost::serialization::make_nvp;
        ar & BOOST_SERIALIZATION_NVP(m_secret);
    }
    ///////////////////////////////////////////////////////////////

public:
    stored_number() = default;

    explicit stored_number(double secret) : m_secret(secret) { /* nothing */ }

    stored_number(const stored_number &cp) = default;

    ~stored_number() = default;

    stored_number &operator=(const stored_number &cp) = default;

    std::shared_ptr<stored_number> clone() {
        return std::make_shared<stored_number>(*this);
    }

    [[nodiscard]] double value() const {
        return m_secret;
    }

private:
    double m_secret = 0.;
};

/******************************************************************************************/
////////////////////////////////////////////////////////////////////////////////////////////
/******************************************************************************************/

class random_container_payload : public payload_base {
    ///////////////////////////////////////////////////////////////
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive &ar, const unsigned int) {
        using boost::serialization::make_nvp;
        ar
        & make_nvp("payload_base", boost::serialization::base_object<payload_base>(*this))
        & BOOST_SERIALIZATION_NVP(m_data);
    }
    ///////////////////////////////////////////////////////////////

public:
    // Initialize container with random numbers
    template<typename dist_type, typename rng_type>
    random_container_payload(std::size_t size, dist_type &dist, rng_type &rng) {
        for (unsigned int i = 0; i < size; i++) {
            this->add(std::shared_ptr<stored_number>(new stored_number(dist(rng))));
        }
    }

    // Copy constructor
    random_container_payload(const random_container_payload &cp) : payload_base(cp) {
        m_data.clear();
        for (const auto &d: cp.m_data) {
            m_data.push_back(d->clone());
        }
    }

    // The destructor
    ~random_container_payload() override = default;

    // Assignment operator
    random_container_payload &operator=(const random_container_payload &cp) {
        m_data.clear();
        for (const auto &d: cp.m_data) {
            m_data.push_back(d->clone());
        }

        return *this;
    }

    void sort() {
        std::sort(
                m_data.begin(), m_data.end(),
                [](const std::shared_ptr<stored_number> &x, const std::shared_ptr<stored_number> &y) -> bool {
                    return x->value() < y->value();
                }
        );
    }

    [[nodiscard]] std::size_t size() const {
        return m_data.size();
    }

    [[nodiscard]] std::shared_ptr<stored_number> member(std::size_t pos) const {
        return m_data.at(pos);
    }

    void add(const std::shared_ptr<stored_number> &p) {
        m_data.push_back(p);
    }

private:
    // Only needed for de-serialization
    random_container_payload() = default;

    void process_() override {
        this->sort();
    }

    bool is_processed_() override {
        return std::is_sorted(
                m_data.begin(), m_data.end(),
                [](const std::shared_ptr<stored_number> &x_ptr, const std::shared_ptr<stored_number> &y_ptr) -> bool {
                    return (x_ptr->value() < y_ptr->value());
                }
        );
    };

    //-------------------------------------------------
    // Data

    std::vector<std::shared_ptr<stored_number>> m_data;
};

/******************************************************************************************/
////////////////////////////////////////////////////////////////////////////////////////////
/******************************************************************************************/

class sleep_payload : public payload_base {
    ///////////////////////////////////////////////////////////////
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive &ar, const unsigned int) {
        using boost::serialization::make_nvp;
        ar
        & make_nvp("payload_base", boost::serialization::base_object<payload_base>(*this))
        & BOOST_SERIALIZATION_NVP(m_sleep_time);
    }
    ///////////////////////////////////////////////////////////////

public:
    // Initialize with the sleep duration in seconds (a double number, i.e. you can say "sleep 1.5 seconds)
    explicit sleep_payload(double sleep_time) : m_sleep_time(sleep_time) { /* default */ }

    // Copy constructor
    sleep_payload(const sleep_payload &) = default;

    // The destructor
    ~sleep_payload() override = default;

    // Assignment operator
    sleep_payload &operator=(const sleep_payload &) = default;

private:
    // Only needed for de-serialization
    sleep_payload() = default;

    void process_() override {
        std::this_thread::sleep_for(std::chrono::duration<double>(m_sleep_time));
    }

    bool is_processed_() override {
        return true;
    }

    //-------------------------------------------------
    // Data
    double m_sleep_time = 0.;
};

/******************************************************************************************/
////////////////////////////////////////////////////////////////////////////////////////////
/******************************************************************************************/
// Used for the serialization of the classes in this file

BOOST_SERIALIZATION_ASSUME_ABSTRACT(payload_base)
BOOST_CLASS_EXPORT_KEY(command_container)
BOOST_CLASS_EXPORT_KEY(stored_number)
BOOST_CLASS_EXPORT_KEY(random_container_payload)
BOOST_CLASS_EXPORT_KEY(sleep_payload)

/******************************************************************************************/
////////////////////////////////////////////////////////////////////////////////////////////
/******************************************************************************************/
