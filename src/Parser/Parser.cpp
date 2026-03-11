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

namespace {

std::string trimCopy(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string toLowerCopy(std::string value) {
    std::ranges::transform(value, value.begin(), ::tolower);
    return value;
}

bool isAnalysisPhaseTag(const std::string& value) {
    std::string lower = toLowerCopy(value);
    return lower == "a" || lower == "analysis";
}

bool isDesignPhaseTag(const std::string& value) {
    std::string lower = toLowerCopy(value);
    return lower == "d" || lower == "design";
}

void applyPhaseSelection(bool sawAnalysis, bool sawDesign, bool& isAnalysis, bool& isDesign) {
    if (sawAnalysis && !sawDesign) {
        isDesign = false;
    } else if (sawDesign && !sawAnalysis) {
        isAnalysis = false;
    }
}

void extractLeadingLabel(std::string& actionStr, Model::Action& action) {
    if (!actionStr.starts_with("@")) return;

    size_t end = actionStr.find_first_of(" \t");
    std::string token = end == std::string::npos ? actionStr : actionStr.substr(0, end);
    if (token.size() <= 1) return;

    action.label = token.substr(1);
    actionStr = end == std::string::npos ? "" : trimCopy(actionStr.substr(end));
}

std::string stripMatchingQuotes(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

} // namespace

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
        if (line.empty()) continue;
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
    std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
        return;
    }

    size_t firstChar = line.find_first_not_of(" \t");
    bool isMember = (firstChar != std::string::npos && firstChar > 0);

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
            bool sawAnalysis = false;
            bool sawDesign = false;
            
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
                std::string content = match[1];
                
                // Split on commas
                std::stringstream ss(content);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    item = trim(item);
                    if (isAnalysisPhaseTag(item)) {
                        sawAnalysis = true;
                    } else if (isDesignPhaseTag(item)) {
                        sawDesign = true;
                    } else if (!item.empty()) {
                        newClass.stereotypes.push_back(item);
                    }
                }
                searchStr = match.suffix().str();
            }

            applyPhaseSelection(sawAnalysis, sawDesign, newClass.isAnalysis, newClass.isDesign);

            project.classes.push_back(newClass);
            currentClass = &project.classes.back();
        } else if (trimmed.starts_with("enum")) {
            Model::Enum newEnum;
            bool sawAnalysis = false;
            bool sawDesign = false;
            std::stringstream ss(trimmed.substr(5));
            ss >> newEnum.name;

            // Optional stereotypes/phase tags
            std::regex stereotypeRegex("\\[([^\\]]+)\\]");
            std::smatch match;
            std::string searchStr = trimmed;
            while (std::regex_search(searchStr, match, stereotypeRegex)) {
                std::string content = match[1];
                
                // Split on commas
                std::stringstream ss(content);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    item = trim(item);
                    if (isAnalysisPhaseTag(item)) sawAnalysis = true;
                    else if (isDesignPhaseTag(item)) sawDesign = true;
                }
                searchStr = match.suffix().str();
            }

            applyPhaseSelection(sawAnalysis, sawDesign, newEnum.isAnalysis, newEnum.isDesign);

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
            bool sawAnalysis = false;
            bool sawDesign = false;
            
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
                    std::string slower = toLowerCopy(item);
                    
                    if (isAnalysisPhaseTag(item)) {
                        sawAnalysis = true;
                    } else if (isDesignPhaseTag(item)) {
                        sawDesign = true;
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
                    } else if (item.find('(') != std::string::npos) {
                        size_t start = item.find('(');
                        size_t end = item.find_last_of(')');
                        std::string key = trim(item.substr(0, start));
                        std::string val = trim(item.substr(start + 1, end - start - 1));
                        method.metadata[key] = val;
                    } else if (!item.empty()) {
                        method.metadata[item] = "";
                        method.modifiers.push_back(item);
                    }
                }
                searchStr = match.suffix().str();
            }
            applyPhaseSelection(sawAnalysis, sawDesign, method.isAnalysis, method.isDesign);
            currentClass->methods.push_back(method);
            currentMethod = &currentClass->methods.back();
        } else {
            // Attribute
            currentMethod = nullptr;
            Model::Attribute attr;
            attr.visibility = visibility;
            bool sawAnalysis = false;
            bool sawDesign = false;

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
                    if (isAnalysisPhaseTag(item)) {
                        sawAnalysis = true;
                    } else if (isDesignPhaseTag(item)) {
                        sawDesign = true;
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
            applyPhaseSelection(sawAnalysis, sawDesign, attr.isAnalysis, attr.isDesign);
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

        bool isStep = trimmed.starts_with("step") || trimmed.starts_with(":>") || trimmed.starts_with("alt") || 
                      trimmed.starts_with("else") || trimmed.starts_with("goto") || 
                      (trimmed.starts_with("@") && (trimmed.find("step") != std::string::npos || trimmed.find(":>") != std::string::npos || trimmed.find("goto") != std::string::npos || trimmed.find("alt") != std::string::npos)) ||
                      (trimmed.starts_with("[") && (trimmed.find("alt") != std::string::npos || trimmed.find("goto") != std::string::npos));

        if (isStep) {
            Model::Action action;
            std::string actionStr = trimmed;

            extractLeadingLabel(actionStr, action);

            // 1. Extract condition [Condition]
            std::regex condRegex("\\[([^\\]]+)\\]");
            std::smatch match;
            if (std::regex_search(actionStr, match, condRegex)) {
                action.condition = match[1].str();
                size_t cpos = actionStr.find("[" + action.condition + "]");
                if (cpos != std::string::npos) {
                    actionStr.erase(cpos, action.condition.length() + 2);
                    actionStr = trim(actionStr);
                }
            }

            // 2. Handle goto
            std::regex gotoRegex("\\bgoto\\s+(@?\\w+|end)\\b");
            if (std::regex_search(actionStr, match, gotoRegex)) {
                action.gotoLabel = match[1].str();
                if (!action.gotoLabel.empty() && action.gotoLabel[0] == '@') {
                    action.gotoLabel = action.gotoLabel.substr(1);
                }
                actionStr = trim(std::regex_replace(actionStr, gotoRegex, ""));
            }

            // 3. Check for keywords
            if (actionStr.find("alt") != std::string::npos) {
                action.isAlternative = true;
                actionStr = trim(std::regex_replace(actionStr, std::regex("\\balt\\b"), ""));
            }
            if (actionStr.find("else") != std::string::npos) {
                action.isAlternative = true;
                actionStr = trim(std::regex_replace(actionStr, std::regex("\\belse\\b"), ""));
            }
            if (actionStr.starts_with("step")) {
                actionStr = trim(actionStr.substr(4));
            }
            extractLeadingLabel(actionStr, action);

            // 4. Target :>
            size_t targetPos = actionStr.find(":>");
            if (targetPos != std::string::npos) {
                action.target = trim(actionStr.substr(targetPos + 2));
                actionStr = trim(actionStr.substr(0, targetPos));
            }

            // 5. Final name
            action.name = stripMatchingQuotes(trim(actionStr));
            if (action.name.empty() && !action.gotoLabel.empty()) {
                action.name = "Jump to @" + action.gotoLabel;
            }

            // In UseCase block, treat matches as steps even if indented, unless they are parameters (start with - or *)
            bool isParameter = (trimmed[0] == '-' || trimmed[0] == '*');

            if (!isParameter) {
                currentUseCase->actions.push_back(action);
                currentAction = &currentUseCase->actions.back();
            } else if (currentAction) {
                // If explicitly marked as parameter
                Model::Attribute param;
                std::string pStr = trim(trimmed.substr(1));
                size_t colon = pStr.find(':');
                if (colon != std::string::npos) {
                    param.name = trim(pStr.substr(0, colon));
                    param.type = trim(pStr.substr(colon + 1));
                } else {
                    param.name = pStr;
                }
                currentAction->parameters.push_back(param);
            }
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
