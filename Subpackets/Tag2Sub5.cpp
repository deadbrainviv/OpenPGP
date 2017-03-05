#include "Tag2Sub5.h"

Tag2Sub5::Tag2Sub5()
    : Tag2Subpacket(5, 2),
      level(),
      amount()
{}

Tag2Sub5::Tag2Sub5(const std::string & data)
    : Tag2Sub5()
{
    read(data);
}

void Tag2Sub5::read(const std::string & data){
    level = data[0];
    amount = data[1];
}

std::string Tag2Sub5::show(const uint8_t indents, const uint8_t indent_size) const{
    const std::string tab(indents * indent_size, ' ');
    std::stringstream out;
    out << tab << show_title() << "\n"
        << tab << "            Trust Level: " << std::to_string(level) << "\n"
        << tab << "            Trust Amount: " << std::to_string(amount);
    return out.str();
}

std::string Tag2Sub5::raw() const{
    return std::string(1, level) + std::string(1, amount);
}

uint8_t Tag2Sub5::get_level() const{
    return level;
}

uint8_t Tag2Sub5::get_amount() const{
    return amount;
}

void Tag2Sub5::set_level(const uint8_t l){
    level = l;
}

void Tag2Sub5::set_amount(const uint8_t a){
    amount = a;
}

Tag2Subpacket::Ptr Tag2Sub5::clone() const{
    return std::make_shared <Tag2Sub5> (*this);
}
