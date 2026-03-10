/*
 * MODT (Modeling Tool)
 * Copyright (C) 2026 Eduard Fekete <modt@eduardfekete.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "Parser.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <regex>

namespace Parser {

std::string ModtParser::trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

Model::Project ModtParser::parse(const std::string& filePath) {
    Model::Project project;
    parseTo(filePath, project);
    return project;
}

void ModtParser::parseTo(const std::string& filePath, Model::Project& project) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filePath << "\n";
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        parseLine(line, project);
    }
}

void ModtParser::addRelationship(Model::Project& project, Model::Relationship newRel) {
    if (newRel.from.empty() || newRel.to.empty()) return;

    for (auto& r : project.relationships) {
        bool sameEnds = (r.from == newRel.from && r.to == newRel.to);
        bool swappedEnds = (r.from == newRel.to && r.to == newRel.from);
        
        if ((sameEnds || swappedEnds) && r.type == newRel.type && r.label == newRel.label) {
            // Found a match to merge
            if (sameEnds) {
                if (r.fromLabel.empty()) r.fromLabel = newRel.fromLabel;
                if (r.toLabel.empty()) r.toLabel = newRel.toLabel;
            } else {
                if (r.fromLabel.empty()) r.fromLabel = newRel.toLabel;
                if (r.toLabel.empty()) r.toLabel = newRel.fromLabel;
            }
            return;
        }
    }
    project.relationships.push_back(newRel);
}

void ModtParser::parseLine(const std::string& line, Model::Project& project) {
    if (line.empty() || trim(line).empty() || trim(line)[0] == '#') {
        return;
    }

    // Check for indentation (simplified: leading space/tab means member)
    bool isMember = (line[0] == ' ' || line[0] == '\t');
    std::string trimmed = trim(line);

    if (!isMember) {
        currentClass = nullptr;
        currentEnum = nullptr;
        currentMethod = nullptr;
        currentUseCase = nullptr;
        currentAction = nullptr;
        inArtifactsBlock = false;
        inSystemBlock = false;

        if (trimmed == "artifacts") {
            inArtifactsBlock = true;
        } else if (trimmed == "system") {
            inSystemBlock = true;
        } else if (trimmed.starts_with("@puml-head")) {
            size_t firstSpace = trimmed.find_first_of(" \t");
            if (firstSpace != std::string::npos) {
                project.pumlHeaders.push_back(trim(trimmed.substr(firstSpace)));
            }
        } else if (trimmed.starts_with("@puml-header")) {
            size_t firstSpace = trimmed.find_first_of(" \t");
            if (firstSpace != std::string::npos) {
                project.pumlHeaders.push_back(trim(trimmed.substr(firstSpace)));
            }
        } else if (trimmed.starts_with("obj")) {
            Model::Class newClass;
            std::string classStr = trim(trimmed.substr(4));
            
            // Resolve name and inheritance
            size_t inheritPos = classStr.find("-|>");
            if (inheritPos != std::string::npos) {
                newClass.baseClass = trim(classStr.substr(inheritPos + 3));
                classStr = trim(classStr.substr(0, inheritPos));
            }
            
            std::stringstream ss(classStr);
            ss >> newClass.name;

            // Simple check for stereotypes [A]
            std::regex stereotypeRegex("\\[([^\\]]+)\\]");
            std::smatch match;
            std::string searchStr = trimmed;
            while (std::regex_search(searchStr, match, stereotypeRegex)) {
                std::string s = match[1];
                std::string slower = s;
                std::ranges::transform(slower, slower.begin(), ::tolower);
                
                if (slower == "a" || slower == "analysis") {
                    newClass.isDesign = false;
                } else if (slower == "d" || slower == "design") {
                    newClass.isAnalysis = false;
                } else {
                    newClass.stereotypes.push_back(s);
                }
                searchStr = match.suffix().str();
            }

            project.classes.push_back(newClass);
            currentClass = &project.classes.back();
        } else if (trimmed.starts_with("enum")) {
            Model::Enum newEnum;
            std::stringstream ss(trimmed.substr(5));
            ss >> newEnum.name;

            // Optional stereotypes/phase tags
            std::regex stereotypeRegex("\\[([^\\]]+)\\]");
            std::smatch match;
            std::string searchStr = trimmed;
            while (std::regex_search(searchStr, match, stereotypeRegex)) {
                std::string s = match[1];
                std::string slower = s;
                std::ranges::transform(slower, slower.begin(), ::tolower);
                if (slower == "a" || slower == "analysis") newEnum.isDesign = false;
                else if (slower == "d" || slower == "design") newEnum.isAnalysis = false;
                searchStr = match.suffix().str();
            }

            // Look for inline values enum Name { VAL1, VAL2 }
            size_t openBrace = trimmed.find('{');
            size_t closeBrace = trimmed.find_last_of('}');
            if (openBrace != std::string::npos && closeBrace != std::string::npos && closeBrace > openBrace) {
                std::string vals = trimmed.substr(openBrace + 1, closeBrace - openBrace - 1);
                std::stringstream ssVals(vals);
                std::string v;
                while (std::getline(ssVals, v, ',')) {
                    v = trim(v);
                    if (!v.empty()) newEnum.values.push_back(v);
                }
            }

            project.enums.push_back(newEnum);
            currentEnum = &project.enums.back();
            currentClass = nullptr;
        } else if (trimmed.starts_with("uc")) {
            Model::UseCase uc;
            uc.name = trim(trimmed.substr(3));
            project.useCases.push_back(uc);
            currentUseCase = &project.useCases.back();
        } else if (trimmed.starts_with("rel")) {
            // Top-level relationship: rel From ["fLabel"] type ["tLabel"] To [: label]
            std::regex relRegex("rel\\s+(?:\"([^\"]+)\"|(\\S+))\\s*(?:\"([^\"]*)\")?\\s*(\\S+)\\s*(?:\"([^\"]*)\")?\\s+(?:\"([^\"]+)\"|(\\S+))(?:\\s*:\\s*(.*))?");
            std::smatch match;
            if (std::regex_search(trimmed, match, relRegex)) {
                Model::Relationship rel;
                rel.from = match[1].matched ? match[1].str() : match[2].str();
                rel.fromLabel = match[3];
                rel.type = match[4];
                rel.toLabel = match[5];
                rel.to = match[6].matched ? match[6].str() : match[7].str();
                rel.label = match[8];
                addRelationship(project, rel);
            }
        }
    } else if (inArtifactsBlock) {
        Model::Artifact art;
        // Use a more robust regex that requires spaces between components
        std::regex artRegex("^([^\\s\\[]+)(?:\\s+\\[([^\\]]+)\\])?(?:\\s+(.*))?$");
        std::smatch match;
        if (std::regex_search(trimmed, match, artRegex)) {
            art.type = match[1];
            art.platform = match[2];
            std::string out = trim(match[3]);

            // Check for format at the end like [svg]
            std::regex formatRegex("\\[([^\\]]+)\\]$");
            std::smatch fmtMatch;
            if (std::regex_search(out, fmtMatch, formatRegex)) {
                art.format = fmtMatch[1];
                art.outputPath = trim(out.substr(0, fmtMatch.position()));
            } else {
                art.outputPath = out;
            }
            project.requestedArtifacts.push_back(art);
        }
    } else if (inSystemBlock) {
        if (trimmed.starts_with("title")) {
            project.title = trim(trimmed.substr(5));
        } else if (trimmed.starts_with("name")) {
            project.name = trim(trimmed.substr(4));
        } else if (trimmed.starts_with("description")) {
            project.description = trim(trimmed.substr(11));
        }
    } else if (currentEnum) {
        if (trimmed[0] == '-' || trimmed[0] == '*') trimmed = trim(trimmed.substr(1));
        if (trimmed.find(',') != std::string::npos) {
            std::stringstream ss(trimmed);
            std::string v;
            while (std::getline(ss, v, ',')) {
                v = trim(v);
                if (!v.empty()) currentEnum->values.push_back(v);
            }
        } else {
            currentEnum->values.push_back(trimmed);
        }
    } else if (currentClass) {
        // It's a member of the current object
        if (trimmed.starts_with("rel")) {
            // Indented relationship: rel ["myL"] type ["targetL"] Target [: label]
            std::regex memberRelRegex("rel\\s*(?:\"([^\"]*)\")?\\s*(\\S+)\\s*(?:\"([^\"]*)\")?\\s+(?:\"([^\"]+)\"|(\\S+))(?:\\s*:\\s*(.*))?");
            std::smatch match;
            if (std::regex_search(trimmed, match, memberRelRegex)) {
                Model::Relationship rel;
                rel.from = currentClass->name;
                rel.fromLabel = match[1];
                rel.type = match[2];
                rel.toLabel = match[3];
                rel.to = match[4].matched ? match[4].str() : match[5].str();
                rel.label = match[6];
                addRelationship(project, rel);
                return;
            }
        }

        if (trimmed.starts_with("pre") && currentMethod) {
            currentMethod->preconditions.push_back(trim(trimmed.substr(3)));
            return;
        }
        if (trimmed.starts_with("post") && currentMethod) {
            currentMethod->postconditions.push_back(trim(trimmed.substr(4)));
            return;
        }

        std::string visibility = "";
        if (trimmed[0] == '+' || trimmed[0] == '-' || trimmed[0] == '#' || trimmed[0] == '~') {
            visibility = trimmed[0];
            trimmed = trim(trimmed.substr(1));
        }

        size_t firstParen = trimmed.find('(');
        size_t lastParen = trimmed.find_last_of(')');
        size_t firstBracket = trimmed.find('[');

        bool isMethod = (firstParen != std::string::npos && lastParen != std::string::npos && lastParen > firstParen);
        if (isMethod && firstBracket != std::string::npos && firstBracket < firstParen) {
            // Probably an attribute with metadata that happens to have parens, like [db(VARCHAR(20))]
            // Unless there's another set of parens earlier?
            // Actually, a method would be name(...) [...]. So name is before (.
            // If [ is before (, then it's likely an attribute metadata.
            isMethod = false;
        }

        if (isMethod) {
            Model::Method method;
            method.visibility = visibility;
            
            // Find the parenthesis matching the first one
            int depth = 0;
            size_t endOfSig = std::string::npos;
            for (size_t i = 0; i < trimmed.size(); ++i) {
                if (trimmed[i] == '(') {
                    if (depth == 0 && i != firstParen) {
                        // This shouldn't happen for a simple method name, but let's be safe
                    }
                    depth++;
                }
                else if (trimmed[i] == ')') {
                    depth--;
                    if (depth == 0 && i >= firstParen) {
                        endOfSig = i;
                        break;
                    }
                }
            }

            if (endOfSig == std::string::npos) endOfSig = lastParen; // Fallback

            std::string sig = trimmed.substr(0, endOfSig + 1);
            std::string metadataPart = (endOfSig + 1 < trimmed.size()) ? trim(trimmed.substr(endOfSig + 1)) : "";
            
            method.name = trim(sig.substr(0, firstParen));
            if (method.name.starts_with("method ")) {
                method.name = trim(method.name.substr(7));
            }
            std::string paramStr = sig.substr(firstParen + 1, endOfSig - firstParen - 1);
            
            std::stringstream ssParam(paramStr);
            std::string p;
            while (std::getline(ssParam, p, ',')) {
                p = trim(p);
                if (p.empty()) continue;
                Model::Attribute param;
                size_t colon = p.find(':');
                if (colon != std::string::npos) {
                    param.name = trim(p.substr(0, colon));
                    param.type = trim(p.substr(colon + 1));
                } else {
                    param.name = p;
                }
                method.parameters.push_back(param);
            }

            // Parse metadata in metadataPart
            std::regex modRegex("\\[([^\\]]+)\\]");
            std::smatch match;
            std::string searchStr = metadataPart;
            while (std::regex_search(searchStr, match, modRegex)) {
                std::string content = match[1];
                
                std::vector<std::string> items;
                std::string current;
                int pDepth = 0;
                for (char c : content) {
                    if (c == '(') pDepth++;
                    if (c == ')') pDepth--;
                    if (c == ',' && pDepth == 0) {
                        items.push_back(trim(current));
                        current = "";
                    } else {
                        current += c;
                    }
                }
                if (!current.empty()) items.push_back(trim(current));

                for (auto& item : items) {
                    std::string slower = item;
                    std::ranges::transform(slower, slower.begin(), ::tolower);
                    
                    if (slower == "a" || slower == "analysis") {
                        method.isDesign = false;
                    } else if (slower == "d" || slower == "design") {
                        method.isAnalysis = false;
                    } else if (slower.starts_with("set(")) {
                        size_t start = item.find('(');
                        size_t end = item.find_last_of(')');
                        if (start != std::string::npos && end != std::string::npos) {
                            std::string inner = item.substr(start + 1, end - start - 1);
                            std::vector<std::string> parts;
                            std::stringstream innerSS(inner);
                            std::string part;
                            while (std::getline(innerSS, part, ',')) {
                                parts.push_back(trim(part));
                            }
                            Model::StateEffect eff;
                            if (parts.size() >= 1) eff.variable = parts[0];
                            if (parts.size() >= 2) eff.value = parts[1];
                            if (parts.size() >= 3) eff.trigger = parts[2];
                            if (parts.size() >= 4) eff.fromValue = parts[3];
                            method.effects.push_back(eff);
                        }
                    } else if (slower.starts_with("pre(")) {
                        size_t start = item.find('(');
                        size_t end = item.find_last_of(')');
                        if (start != std::string::npos && end != std::string::npos) {
                            method.preconditions.push_back(item.substr(start + 1, end - start - 1));
                        }
                    } else if (!item.empty()) {
                        method.modifiers.push_back(item);
                    }
                }
                searchStr = match.suffix().str();
            }
            currentClass->methods.push_back(method);
            currentMethod = &currentClass->methods.back();
        } else {
            // Attribute
            currentMethod = nullptr;
            Model::Attribute attr;
            attr.visibility = visibility;

            std::regex modRegex("\\[([^\\]]+)\\]");
            std::smatch match;
            std::string searchStr = trimmed;
            while (std::regex_search(searchStr, match, modRegex)) {
                std::string content = match[1];
                
                // Better splitting that ignores commas inside parens
                std::vector<std::string> items;
                std::string current;
                int pDepth = 0;
                for (char c : content) {
                    if (c == '(') pDepth++;
                    else if (c == ')') pDepth--;
                    
                    if (c == ',' && pDepth == 0) {
                        items.push_back(trim(current));
                        current = "";
                    } else {
                        current += c;
                    }
                }
                items.push_back(trim(current));

                for (auto& item : items) {
                    if (item.empty()) continue;
                    std::string slower = item;
                    std::ranges::transform(slower, slower.begin(), ::tolower);
                    
                    if (slower == "a" || slower == "analysis") {
                        attr.isDesign = false;
                    } else if (slower == "d" || slower == "design") {
                        attr.isAnalysis = false;
                    } else {
                        // Check for key(value) metadata
                        size_t openParen = item.find('(');
                        size_t closeParen = item.find_last_of(')');
                        if (openParen != std::string::npos && closeParen != std::string::npos && closeParen > openParen) {
                            std::string key = trim(item.substr(0, openParen));
                            std::string value = trim(item.substr(openParen + 1, closeParen - openParen - 1));
                            attr.metadata[key] = value;
                        } else {
                            // Single word metadata/stereotype
                            attr.metadata[item] = "";
                        }
                    }
                }
                searchStr = match.suffix().str();
            }
            trimmed = trim(std::regex_replace(trimmed, modRegex, ""));
            
            size_t colonPos = trimmed.find(':');
            if (colonPos != std::string::npos) {
                attr.name = trim(trimmed.substr(0, colonPos));
                attr.type = trim(trimmed.substr(colonPos + 1));
            } else {
                attr.name = trimmed;
            }

            if (attr.name.starts_with("attr ")) {
                attr.name = trim(attr.name.substr(5));
            }
            currentClass->attributes.push_back(attr);
        }
    } else if (currentUseCase) {
        if (trimmed.starts_with("rel")) {
            // Indented relationship: rel ["myL"] type ["targetL"] Target [: label]
            std::regex memberRelRegex("rel\\s*(?:\"([^\"]*)\")?\\s*(\\S+)\\s*(?:\"([^\"]*)\")?\\s+(?:\"([^\"]+)\"|(\\S+))(?:\\s*:\\s*(.*))?");
            std::smatch match;
            if (std::regex_search(trimmed, match, memberRelRegex)) {
                Model::Relationship rel;
                rel.from = "uc:" + currentUseCase->name;
                rel.fromLabel = match[1];
                rel.type = match[2];
                rel.toLabel = match[3];
                rel.to = match[4].matched ? match[4].str() : match[5].str();
                rel.label = match[6];
                addRelationship(project, rel);
                return;
            }
        }

        if (trimmed.starts_with("step") || trimmed.starts_with(":>") || trimmed.starts_with("alt") || trimmed.starts_with("else") || trimmed.starts_with("goto")) {
            Model::Action action;
            bool isAlt = (trimmed.starts_with("alt") || trimmed.starts_with("else"));
            bool isGoto = (trimmed.starts_with("goto"));
            
            std::string actionStr;
            if (isGoto) actionStr = trim(trimmed.substr(4));
            else if (trimmed.starts_with("else")) actionStr = trim(trimmed.substr(4));
            else if (isAlt) actionStr = trim(trimmed.substr(3));
            else if (trimmed.starts_with("step")) actionStr = trim(trimmed.substr(4));
            else actionStr = trim(trimmed.substr(2));
            
            if (isGoto) {
                action.name = "Jump to " + actionStr;
                action.gotoLabel = actionStr;
            } else {
                size_t targetPos = actionStr.find(":>");
                if (targetPos != std::string::npos) {
                    action.target = trim(actionStr.substr(targetPos + 2));
                    actionStr = trim(actionStr.substr(0, targetPos));
                }

                // Check for label @Label
                std::regex labelRegex("@(\\w+)");
                std::smatch labelMatch;
                if (std::regex_search(actionStr, labelMatch, labelRegex)) {
                    action.label = labelMatch[1];
                    actionStr = trim(std::regex_replace(actionStr, labelRegex, ""));
                }

                std::regex condRegex("\\[([^\\]]+)\\]");
                std::smatch condMatchSub;
                if (std::regex_search(actionStr, condMatchSub, condRegex)) {
                    action.condition = condMatchSub[1];
                    actionStr = trim(std::regex_replace(actionStr, condRegex, ""));
                }

                if (trimmed.starts_with("else") || trimmed.starts_with("alt")) {
                    if (action.condition.empty() && !actionStr.empty()) {
                        action.condition = actionStr;
                        actionStr = "";
                    }
                    action.isAlternative = true;
                }

                if (actionStr.starts_with("goto ")) {
                    action.gotoLabel = trim(actionStr.substr(5));
                    action.name = "Jump to " + action.gotoLabel;
                } else if (!actionStr.empty()) {
                    action.name = actionStr;
                }
            }
            
            currentUseCase->actions.push_back(action);
            currentAction = &currentUseCase->actions.back();
        } else if (trimmed.starts_with("description")) {
            currentUseCase->description = trim(trimmed.substr(11));
        } else if (trimmed.starts_with("actor")) {
            currentUseCase->actor = trim(trimmed.substr(5));
        } else if (trimmed.starts_with("pre")) {
            currentUseCase->preconditions.push_back(trim(trimmed.substr(3)));
        } else if (trimmed.starts_with("post")) {
            currentUseCase->postconditions.push_back(trim(trimmed.substr(4)));
        } else if (currentAction) {
            // Parameter for the step/action
            Model::Attribute param;
            if (trimmed[0] == '-' || trimmed[0] == '*') trimmed = trim(trimmed.substr(1));
            
            size_t colonPos = trimmed.find(':');
            if (colonPos != std::string::npos) {
                param.name = trim(trimmed.substr(0, colonPos));
                param.type = trim(trimmed.substr(colonPos + 1));
            } else {
                param.name = trimmed;
            }
            currentAction->parameters.push_back(param);
        }
    }
}

} // namespace Parser
