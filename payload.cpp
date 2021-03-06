/**
 * @file payload.cpp
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

#include "payload.hpp"

/******************************************************************************************/

BOOST_CLASS_EXPORT_IMPLEMENT(command_container) // NOLINT
BOOST_CLASS_EXPORT_IMPLEMENT(stored_number) // NOLINT
BOOST_CLASS_EXPORT_IMPLEMENT(random_container_payload) // NOLINT
BOOST_CLASS_EXPORT_IMPLEMENT(sleep_payload) // NOLINT

/******************************************************************************************/

// Saving and loading of payload_base-derivatives through the base pointer
std::string to_string(const payload_base *payload_ptr) {
    if (!payload_ptr) {
        throw std::runtime_error("to_string: payload_ptr is empty");
    }

    std::ostringstream
    oss(std::ios::out
#if defined(BINARYARCHIVE)
        | std::ios::binary
#endif
    );

    {
#if defined(BINARYARCHIVE)
        boost::archive::binary_oarchive oa(oss);
#elif defined(XMLARCHIVE)
        boost::archive::xml_oarchive oa(oss);
#elif defined(TEXTARCHIVE)
        boost::archive::text_oarchive oa(oss);
#else
        boost::archive::xml_oarchive oa(oss);
#endif
        oa << boost::serialization::make_nvp("payload_base", payload_ptr);
    } // archive and stream closed at end of scope

    return oss.str();
}

payload_base *from_string(const std::string &descr) {
    payload_base *payload_ptr = nullptr;

    std::istringstream iss(descr, std::ios::in
#if defined(BINARYARCHIVE)
      | std::ios::binary // de-serialize
#endif
    );

    {
#if defined(BINARYARCHIVE)
        boost::archive::binary_iarchive ia(iss);
#elif defined(XMLARCHIVE)
        boost::archive::xml_iarchive ia(iss);
#elif defined(TEXTARCHIVE)
        boost::archive::text_iarchive ia(iss);
#else
        boost::archive::xml_iarchive ia(iss);
#endif
        ia >> boost::serialization::make_nvp("payload_base", payload_ptr);
    } // archive and stream closed at end of scope

    return payload_ptr;
}

// For debugging purposes: Direct output in XML and binary format
std::string to_xml(const payload_base *payload_ptr) {
    if (!payload_ptr) {
        throw std::runtime_error("to_xml: payload_ptr is empty");
    }

    std::ostringstream oss; // serialize
    {
        boost::archive::xml_oarchive oa(oss);
        oa << boost::serialization::make_nvp("payload_base", payload_ptr);
    } // archive and stream closed at end of scope

    return oss.str();
}

std::string to_binary(const payload_base *payload_ptr) {
    if (!payload_ptr) {
        throw std::runtime_error("to_string: payload_ptr is empty");
    }

    std::ostringstream oss; // serialize
    {
        boost::archive::binary_oarchive oa(oss);
        oa << boost::serialization::make_nvp("payload_base", payload_ptr);
    } // archive and stream closed at end of scope

    return oss.str();
}

payload_base *from_binary(const std::string &descr) {
    payload_base *payload_ptr = nullptr;

    std::istringstream iss(descr); // de-serialize
    {
        boost::archive::binary_iarchive ia(iss);
        ia >> boost::serialization::make_nvp("payload_base", payload_ptr);
    } // archive and stream closed at end of scope

    return payload_ptr;
}

/******************************************************************************************/