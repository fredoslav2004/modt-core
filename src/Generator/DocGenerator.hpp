/*
 * MODT (Modeling Tool)
 * Copyright (C) 2026 Eduard Fekete <modt@eduardfekete.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef DOC_GENERATOR_HPP
#define DOC_GENERATOR_HPP

#include "../Model/Model.hpp"
#include <string>

namespace Generator {

class DocGenerator {
public:
    std::string generate(const Model::Project& project);

private:
    std::string translateStereotype(const std::string& s);
};

} // namespace Generator

#endif // DOC_GENERATOR_HPP
