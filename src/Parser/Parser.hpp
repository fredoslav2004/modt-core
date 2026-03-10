/*
 * MODT (Modeling Tool)
 * Copyright (C) 2026 Eduard Fekete <modt@eduardfekete.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PARSER_HPP
#define PARSER_HPP

#include "../Model/Model.hpp"
#include <string>
#include <vector>

namespace Parser {

class ModtParser {
public:
    Model::Project parse(const std::string& filePath);
    void parseTo(const std::string& filePath, Model::Project& project);

private:
    void parseLine(const std::string& line, Model::Project& project);
    void addRelationship(Model::Project& project, Model::Relationship rel);
    std::string trim(const std::string& s);
    
    Model::Class* currentClass = nullptr;
    Model::Enum* currentEnum = nullptr;
    Model::Method* currentMethod = nullptr;
    Model::UseCase* currentUseCase = nullptr;
    Model::Action* currentAction = nullptr;
    bool inArtifactsBlock = false;
    bool inSystemBlock = false;
};

} // namespace Parser

#endif // PARSER_HPP
