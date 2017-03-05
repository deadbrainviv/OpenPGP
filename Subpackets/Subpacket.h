/*
Subpacket.h
Base class for OpenPGP subpackets to inherit from

Copyright (c) 2013 - 2017 Jason Lee @ calccrypto at gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef __SUBPACKET__
#define __SUBPACKET__

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "../common/includes.h"

// 5.2.3.1. Signature Subpacket Specification
//
//    A subpacket data set consists of zero or more Signature subpackets.
//    In Signature packets, the subpacket data set is preceded by a two-
//    octet scalar count of the length in octets of all the subpackets. A
//    pointer incremented by this number will skip over the subpacket data
//    set.
//
//    Each subpacket consists of a subpacket header and a body. The header
//    consists of:
//
//      - the subpacket length (1, 2, or 5 octets),
//
//      - the subpacket type (1 octet),
//
//    and is followed by the subpacket-specific data.
//
//    The length includes the type octet but not this length. Its format
//    is similar to the "new" format packet header lengths, but cannot have
//    Partial Body Lengths. That is:
//
//        if the 1st octet < 192, then
//            lengthOfLength = 1
//            subpacketLen = 1st_octet
//
//        if the 1st octet >= 192 and < 255, then
//            lengthOfLength = 2
//            subpacketLen = ((1st_octet - 192) << 8) + (2nd_octet) + 192
//
//        if the 1st octet = 255, then
//            lengthOfLength = 5
//            subpacket length = [four-octet scalar starting at 2nd_octet]
//
//    ...
//
//    An implementation SHOULD ignore any subpacket of a type that it does
//    not recognize.
//
//    Bit 7 of the subpacket type is the "critical" bit. If set, it
//    denotes that the subpacket is one that is critical for the evaluator
//    of the signature to recognize. If a subpacket is encountered that is
//    marked critical but is unknown to the evaluating software, the
//    evaluator SHOULD consider the signature to be in error.
//
//    An evaluator may "recognize" a subpacket, but not implement it. The
//    purpose of the critical bit is to allow the signer to tell an
//    evaluator that it would prefer a new, unknown feature to generate an
//    error than be ignored.
//
//    Implementations SHOULD implement the three preferred algorithm
//    subpackets (11, 21, and 22), as well as the "Reason for Revocation"
//    subpacket. Note, however, that if an implementation chooses not to
//    implement some of the preferences, it is required to behave in a
//    polite manner to respect the wishes of those users who do implement
//    these preferences.
//
class Subpacket{
    protected:
        uint8_t type;
        unsigned int size; // only used for displaying. recalculated when writing

        std::string write_subpacket(const std::string & data) const;

        // returns first line of show functions (no tab or newline)
        virtual std::string show_title() const = 0;

        Subpacket(uint8_t type = 0, unsigned int size = 0);
        Subpacket(const Subpacket & copy);
        Subpacket & operator=(const Subpacket & copy);

    public:
        typedef std::shared_ptr <Subpacket> Ptr;

        virtual ~Subpacket();
        virtual void read(const std::string & data) = 0;
        virtual std::string show(const uint8_t indents = 0, const uint8_t indent_size = 4) const = 0;
        virtual std::string raw() const = 0; // returns raw subpacket data, with no header
        std::string write() const;

        uint8_t get_type() const;
        unsigned int get_size() const;

        void set_type(uint8_t t);
        void set_size(unsigned int s);
};

#endif
