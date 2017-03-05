#include "Tag19.h"

Tag19::Tag19()
    : Packet(Packet::ID::Modification_Detection_Code),
      hash()
{
    size = 20;
}

Tag19::Tag19(const Tag19 & copy)
    : Packet(copy),
      hash(copy.hash)
{}

Tag19::Tag19(const std::string & data)
    : Tag19()
{
    read(data);
}

void Tag19::read(const std::string & data){
    size = data.size();
    hash = data;
}

std::string Tag19::show(const uint8_t indents, const uint8_t indent_size) const{
    const std::string tab(indents * indent_size, ' ');
    return tab + show_title() + "\n" + tab + "    SHA - 1 Hash of previous packet: " + hash;
}

std::string Tag19::raw() const{
    return hash;
}

std::string Tag19::get_hash() const{
    return hash;
}

void Tag19::set_hash(const std::string & h){
    hash = h;
    size = raw().size();
}

Packet::Ptr Tag19::clone() const{
    return std::make_shared <Tag19> (*this);
}
