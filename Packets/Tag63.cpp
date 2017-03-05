#include "Tag63.h"

Tag63::Tag63()
    : Tag63(std::string())
{}

Tag63::Tag63(const Tag63 & copy)
    : Packet(copy),
      stream(copy.stream)
{}

Tag63::Tag63(const std::string & data)
    : Packet(63),
      stream(data)
{}

void Tag63::read(const std::string & data){
    stream = data;
}

std::string Tag63::show(const uint8_t indents, const uint8_t indent_size) const{
    const std::string tab(indents * indent_size, ' ');
    return tab + tab + show_title() + "\n" + std::string((indents  + 1) * indent_size, ' ') + hexlify(stream);
}

std::string Tag63::raw() const{
    return stream;
}

std::string Tag63::get_stream() const{
    return stream;
}

void Tag63::set_stream(const std::string & data){
    stream = data;
}

Packet::Ptr Tag63::clone() const{
    return std::make_shared <Tag63> (*this);
}
