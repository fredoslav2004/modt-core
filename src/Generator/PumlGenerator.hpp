/*
 * MODT (Modeling Tool)
 * Copyright (C) 2026 Eduard Fekete <modt@eduardfekete.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PUML_GENERATOR_HPP
#define PUML_GENERATOR_HPP

#include "../Model/Model.hpp"
#include <string>
#include <map>

namespace Generator {

class PumlGenerator {
public:
    std::string generateDesignModel(const Model::Project& project);
    std::string generateDomainModel(const Model::Project& project);
    std::string generateActivityDiagram(const Model::Project& project);
    std::map<std::string, std::string> generateSystemSequenceDiagrams(const Model::Project& project);
    std::map<std::string, std::string> generateStateMachineDiagrams(const Model::Project& project);

private:
    bool isClass(const Model::Project& project, const std::string& name);
    bool isUseCase(const Model::Project& project, const std::string& name);
    bool isActor(const Model::Class& cls);
    std::string translateStereotype(const std::string& s);
};

} // namespace Generator

#endif // PUML_GENERATOR_HPP
