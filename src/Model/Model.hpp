/*
 * MODT (Modeling Tool)
 * Copyright (C) 2026 Eduard Fekete <modt@eduardfekete.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef MODEL_HPP
#define MODEL_HPP

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace Model {

struct Attribute {
    std::string name;
    std::string type;
    std::string visibility = ""; // Empty means unspecified
    bool isAnalysis = true;
    bool isDesign = true;
    std::map<std::string, std::string> metadata;
};

struct StateEffect {
    std::string variable;
    std::string value;
    std::string trigger; // Optional tag or condition
    std::string fromValue; // Optional source state
};

struct Method {
    std::string name;
    std::string returnType;
    std::string visibility = ""; // Empty means unspecified
    std::vector<Attribute> parameters;
    std::vector<std::string> modifiers;
    std::map<std::string, std::string> metadata;
    std::vector<StateEffect> effects;
    std::vector<std::string> preconditions;
    std::vector<std::string> postconditions;
    bool isAnalysis = true;
    bool isDesign = true;
};

struct Class {
    std::string name;
    std::vector<std::string> stereotypes;
    std::string baseClass;
    std::vector<Attribute> attributes;
    std::vector<Method> methods;

    bool isAnalysis = true;
    bool isDesign = true;
};

struct Relationship {
    std::string from;
    std::string to;
    std::string type; // e.g., "-|>" (inheritance), "--" (association)
    std::string fromLabel;
    std::string toLabel;
    std::string label;
};

struct Action {
    std::string name;
    std::string target; // e.g., @sys
    std::string condition; // For alternate paths or guards
    std::string label; // Optional label for jumps
    std::string gotoLabel; // If this is a jump
    bool isAlternative = false;
    std::vector<Attribute> parameters;
};

struct UseCase {
    std::string name;
    std::string description;
    std::string actor;
    std::vector<std::string> preconditions;
    std::vector<std::string> postconditions;
    std::vector<Action> actions; // Now treated as steps
};

struct Artifact {
    std::string type;
    std::string platform;
    std::string format;
    std::string outputPath;
};

struct Enum {
    std::string name;
    std::vector<std::string> values;
    bool isAnalysis = true;
    bool isDesign = true;
};

struct Project {
    std::string name;
    std::string title;
    std::string description;
    std::vector<std::string> pumlHeaders;
    std::vector<Class> classes;
    std::vector<Enum> enums;
    std::vector<Relationship> relationships;
    std::vector<UseCase> useCases;
    std::vector<Artifact> requestedArtifacts;
};

} // namespace Model

#endif // MODEL_HPP
