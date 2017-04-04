#include "packet.h"

const uint8_t Packet::RESERVED                                     = 0;
const uint8_t Packet::PUBLIC_KEY_ENCRYPTED_SESSION_KEY             = 1;
const uint8_t Packet::SIGNATURE                                    = 2;
const uint8_t Packet::SYMMETRIC_KEY_ENCRYPTED_SESSION_KEY          = 3;
const uint8_t Packet::ONE_PASS_SIGNATURE                           = 4;
const uint8_t Packet::SECRET_KEY                                   = 5;
const uint8_t Packet::PUBLIC_KEY                                   = 6;
const uint8_t Packet::SECRET_SUBKEY                                = 7;
const uint8_t Packet::COMPRESSED_DATA                              = 8;
const uint8_t Packet::SYMMETRICALLY_ENCRYPTED_DATA                 = 9;
const uint8_t Packet::MARKER_PACKET                                = 10;
const uint8_t Packet::LITERAL_DATA                                 = 11;
const uint8_t Packet::TRUST                                        = 12;
const uint8_t Packet::USER_ID                                      = 13;
const uint8_t Packet::PUBLIC_SUBKEY                                = 14;
const uint8_t Packet::USER_ATTRIBUTE                               = 17;
const uint8_t Packet::SYM_ENCRYPTED_INTEGRITY_PROTECTED_DATA       = 18;
const uint8_t Packet::MODIFICATION_DETECTION_CODE                  = 19;
const uint8_t Packet::UNKNOWN                                      = 255;

const std::map <uint8_t, std::string> Packet::NAME = {
    std::make_pair(RESERVED,                               "Reserved - a packet tag MUST NOT have this value"),
    std::make_pair(PUBLIC_KEY_ENCRYPTED_SESSION_KEY,       "Public-Key Encrypted Session Key"),
    std::make_pair(SIGNATURE,                              "Signature"),
    std::make_pair(SYMMETRIC_KEY_ENCRYPTED_SESSION_KEY,    "Symmetric-Key Encrypted Session Key"),
    std::make_pair(ONE_PASS_SIGNATURE,                     "One-Pass Signature"),
    std::make_pair(SECRET_KEY,                             "Secret-Key"),
    std::make_pair(PUBLIC_KEY,                             "Public-Key"),
    std::make_pair(SECRET_SUBKEY,                          "Secret-Subkey"),
    std::make_pair(COMPRESSED_DATA,                        "Compressed Data"),
    std::make_pair(SYMMETRICALLY_ENCRYPTED_DATA,           "Symmetrically (Conventional) Encrypted Data"),
    std::make_pair(MARKER_PACKET,                          "Marker Packet (Obsolete Literal Packet)"),
    std::make_pair(LITERAL_DATA,                           "Literal Data"),
    std::make_pair(TRUST,                                  "(Keyring) Trust"),
    std::make_pair(USER_ID,                                "User ID"),
    std::make_pair(PUBLIC_SUBKEY,                          "Public-Subkey (Obsolete Comment Packet)"),
    std::make_pair(USER_ATTRIBUTE,                         "User Attribute"),
    std::make_pair(SYM_ENCRYPTED_INTEGRITY_PROTECTED_DATA, "Sym. Encrypted Integrity Protected Data"),
    std::make_pair(MODIFICATION_DETECTION_CODE,            "Modification Detection Code"),
    std::make_pair(60,                                     "Private or Experimental Values"),
    std::make_pair(61,                                     "Private or Experimental Values"),
    std::make_pair(62,                                     "Private or Experimental Values"),
    std::make_pair(63,                                     "Private or Experimental Values"),
};

bool Packet::is_key_packet(const uint8_t t){
    return (is_primary_key(t) || is_subkey(t));
}

bool Packet::is_primary_key(const uint8_t t){
    return ((t == SECRET_KEY) ||
            (t == PUBLIC_KEY));
}

bool Packet::is_subkey(const uint8_t t){
    return ((t == SECRET_SUBKEY) ||
            (t == PUBLIC_SUBKEY));
}

bool Packet::is_public(const uint8_t t){
    return ((t == PUBLIC_KEY) ||
            (t == PUBLIC_SUBKEY));
}

bool Packet::is_secret(const uint8_t t){
    return ((t == SECRET_KEY) ||
            (t == SECRET_SUBKEY));
}

bool Packet::is_user(const uint8_t t){
    return ((t == USER_ID) ||
            (t == USER_ATTRIBUTE));
}

bool Packet::is_session_key(const uint8_t t){
    return ((t == PUBLIC_KEY_ENCRYPTED_SESSION_KEY) ||
            (t == SYMMETRIC_KEY_ENCRYPTED_SESSION_KEY));
}

bool Packet::is_sym_protected_data(const uint8_t t){
    return ((t == SYMMETRICALLY_ENCRYPTED_DATA) ||
            (t == SYM_ENCRYPTED_INTEGRITY_PROTECTED_DATA));
}


std::string Packet::write_old_length(const std::string & data) const{
    std::string::size_type length = data.size();
    std::string out(1, 0b10000000 | (tag << 2));
    if (partial){
        out[0] |= 3;                                        // partial
    }
    else{
        if (length < 256){
            out[0] |= 0;                                    // 1 octet
            out += std::string(1, length);
        }
        else if ((256 <= length) && (length < 65536)){      // 2 octest
            out[0] |= 1;
            out += unhexlify(makehex(length, 4));
        }
        else if (65536 <= length){                          // 4 octets
            out[0] |= 2;
            out += unhexlify(makehex(length, 8));
        }
    }
    return out + data;
}

// returns formatted length string
std::string Packet::write_new_length(const std::string & data) const{
    std::string::size_type length = data.size();
    std::string out(1, 0b11000000 | tag);
    if (partial){                                           // partial
        uint8_t bits = 0;
        while (length > (1u << bits)){
            bits++;
        }
        length = 224 + bits;
        if (length > 254){
            throw std::runtime_error("Error: Data in partial packet too large.");
        }

        out += std::string(1, length);
    }
    else{
        if (length < 192){                                  // 1 octet
            out += std::string(1, length);
        }
        else if ((192 <= length) && (length < 8383)){       // 2 octets
            length -= 0xc0;
            out += std::string(1, (length >> 8) + 0xc0 ) + std::string(1, length & 0xff);
        }
        else if (length > 8383){                            // 3 octets
            out += std::string(1, '\xff') + unhexlify(makehex(length, 8));
        }
    }
    return out + data;
}

std::string Packet::show_title() const{
    std::string out = std::string(format?"New":"Old") + ": " + Packet::NAME.at(tag) + " (Tag " + std::to_string(tag) + ")";

    switch (partial){
        case 0:
            break;
        case 1:
            out += " (partial start)";
            break;
        case 2:
            out += " (partial continue)";
            break;
        case 3:
            out += " (partial end)";
            break;
        default:
            throw std::runtime_error("Error: Unknown partial type: " + std::to_string(partial));
            break;
    }
    return out;
}

Packet::Packet(const uint8_t t)
    : Packet(t, 0)
{}

Packet::Packet(const uint8_t t, uint8_t ver)
    : tag(t),
      version(ver),
      format(true),
      size(0),
      partial(0)
{}

Packet::Packet(const Packet & copy)
    : tag(copy.tag),
      version(copy.version),
      format(copy.version),
      size(copy.size),
      partial(copy.partial)
{}

Packet::Packet()
    : Packet(UNKNOWN)
{}

Packet::~Packet(){}

std::string Packet::write(const Packet::Format header) const{
    if ((header == NEW) ||      // specified new header
        (tag > 15)){            // tag > 15, so new header is required
        return write_new_length(raw());
    }
    return write_old_length(raw());
}

uint8_t Packet::get_tag() const{
    return tag;
}

bool Packet::get_format() const{
    return format;
}

uint8_t Packet::get_version() const{
    return version;
}

std::size_t Packet::get_size() const{
    return size;
}

uint8_t Packet::get_partial() const{
    return partial;
}

void Packet::set_tag(const uint8_t t){
    tag = t;
}

void Packet::set_format(const bool f){
    format = f;
}

void Packet::set_version(const uint8_t v){
    version = v;
}

void Packet::set_size(const std::size_t s){
    size = s;
}

void Packet::set_partial(const uint8_t p){
    partial = p;
}

Packet & Packet::operator=(const Packet & copy)
{
    tag = copy.tag;
    version = copy.version;
    format = copy.format;
    size = copy.size;
    partial = copy.partial;
    return *this;
}
