/*
 * MODT (Modeling Tool)
 * Copyright (C) 2026 Eduard Fekete <modt@eduardfekete.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "DocGenerator.hpp"
#include <sstream>
#include <format>
#include <ranges>
#include <algorithm>
#include <cctype>
#include <initializer_list>

namespace Generator {

std::string trimString(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string htmlEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            default: escaped += c; break;
        }
    }
    return escaped;
}

std::string firstNonEmpty(const std::initializer_list<std::string>& values) {
    for (const auto& value : values) {
        if (!value.empty()) return value;
    }
    return "";
}

std::string operationSignature(const Model::SystemOperation& operation) {
    std::string sig = operation.name + "(";
    for (size_t i = 0; i < operation.parameters.size(); ++i) {
        sig += operation.parameters[i].name;
        if (!operation.parameters[i].type.empty()) sig += ": " + operation.parameters[i].type;
        if (i < operation.parameters.size() - 1) sig += ", ";
    }
    sig += ")";
    return sig;
}

std::string classResponsibilitySummary(const Model::Class& cls) {
    if (!cls.methods.empty()) return std::format("{} operations", cls.methods.size());
    if (!cls.attributes.empty()) return std::format("{} attributes", cls.attributes.size());
    return "structure placeholder";
}

struct TocItem {
    int level = 1;
    std::string title;
    std::string id;
};

std::string slugToken(std::string value) {
    std::string slug;
    bool previousDash = false;
    for (char c : value) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) {
            slug += static_cast<char>(std::tolower(uc));
            previousDash = false;
        } else if (!previousDash) {
            slug += '-';
            previousDash = true;
        }
    }
    while (!slug.empty() && slug.front() == '-') slug.erase(slug.begin());
    while (!slug.empty() && slug.back() == '-') slug.pop_back();
    return slug.empty() ? "section" : slug;
}

std::string markdownHeading(int level, const std::string& title, const std::string& id) {
    return std::format("{} {} {{#{}}}\n\n", std::string(static_cast<size_t>(level), '#'), title, id);
}

std::string assetBaseName(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
    size_t dot = base.find('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    return base;
}

std::string assetDisplayTitle(const Model::Project& project, const DocumentationAsset& asset) {
    std::string base = assetBaseName(asset.path);
    std::string prefix = project.name + "_";
    if (!prefix.empty() && base.starts_with(prefix)) {
        base = base.substr(prefix.size());
    }

    if (!base.empty() && asset.title.ends_with("Diagram") && asset.title != base) {
        return asset.title + ": " + base;
    }
    return asset.title;
}

std::string assetGroupTitle(const DocumentationAsset& asset) {
    if (asset.title == "Design Model" || asset.title == "Domain Model") return "Structural Views";
    if (asset.title == "Activity Diagram") return "Activity Flows";
    if (asset.title == "System Sequence Diagram") return "System Interactions";
    if (asset.title == "Sequence Diagram") return "Collaborator Interactions";
    if (asset.title == "State Machine Diagram") return "Lifecycle Views";
    return "Other Views";
}

std::string htmlImage(const DocumentationAsset& asset, const std::string& altText) {
    return std::format("<img src=\"{}\" alt=\"{}\" class=\"modt-diagram\" />\n\n",
        htmlEscape(asset.path),
        htmlEscape(altText));
}

std::string assetSection(const DocumentationAsset& asset) {
    if (asset.title == "Domain Model") return "domain";
    if (asset.title == "Activity Diagram" || asset.title == "System Sequence Diagram") return "analysis";
    if (asset.title == "Design Model" || asset.title == "Sequence Diagram" || asset.title == "State Machine Diagram") return "design";
    return "implementation";
}

bool hasAssetsForSection(const std::vector<DocumentationAsset>& assets, const std::string& section) {
    return std::ranges::any_of(assets, [&](const auto& asset) {
        return assetSection(asset) == section;
    });
}

bool hasMethods(const Model::Project& project) {
    return std::ranges::any_of(project.classes, [](const auto& cls) {
        return !cls.methods.empty();
    });
}

void addDiagramGroupsToContents(std::vector<TocItem>& items, const std::vector<DocumentationAsset>& assets, const std::string& section) {
    std::string previousGroup;
    int groupIndex = 0;
    for (const auto& asset : assets) {
        if (assetSection(asset) != section) continue;
        std::string group = assetGroupTitle(asset);
        if (group != previousGroup) {
            items.push_back({2, group, section + "-diagram-group-" + std::to_string(++groupIndex) + "-" + slugToken(group)});
            previousGroup = group;
        }
    }
}

void writeSectionDiagrams(std::stringstream& ss, const Model::Project& project, const std::vector<DocumentationAsset>& assets, const std::string& section) {
    std::string previousGroup;
    int groupIndex = 0;
    for (const auto& asset : assets) {
        if (assetSection(asset) != section) continue;
        std::string group = assetGroupTitle(asset);
        if (group != previousGroup) {
            ss << markdownHeading(3, group, section + "-diagram-group-" + std::to_string(++groupIndex) + "-" + slugToken(group));
            previousGroup = group;
        }
        if (asset.embeddable) {
            ss << htmlImage(asset, assetDisplayTitle(project, asset));
        } else {
            ss << std::format("[{}]({})\n\n", asset.title, asset.path);
        }
    }
}

std::vector<TocItem> buildContents(const Model::Project& project, const std::vector<DocumentationAsset>& assets) {
    std::vector<TocItem> items;
    items.push_back({1, "Report overview", "report-overview"});

    bool hasDomain = hasAssetsForSection(assets, "domain") || !project.glossary.empty();
    bool hasAnalysis = !project.supplementaryRequirements.empty()
        || !project.useCases.empty() || !project.systemOperations.empty() || !project.operationContracts.empty()
        || hasAssetsForSection(assets, "analysis");
    bool hasDesign = !project.classes.empty() || !project.enums.empty() || !project.relationships.empty()
        || hasAssetsForSection(assets, "design");
    bool hasImplementation = hasMethods(project) || hasAssetsForSection(assets, "implementation");

    if (hasDomain) {
        items.push_back({1, "Domain", "domain"});
        addDiagramGroupsToContents(items, assets, "domain");
        if (!project.glossary.empty()) items.push_back({2, "Glossary", "glossary"});
    }

    if (hasAnalysis) {
        items.push_back({1, "Analysis", "analysis"});
        addDiagramGroupsToContents(items, assets, "analysis");
    }
    if (!project.supplementaryRequirements.empty()) items.push_back({2, "Supplementary Specification", "supplementary-specification"});
    if (!project.useCases.empty()) {
        items.push_back({2, "Use Cases", "use-cases"});
        for (size_t i = 0; i < project.useCases.size(); ++i) {
            items.push_back({3, project.useCases[i].name, "use-case-" + std::to_string(i + 1) + "-" + slugToken(project.useCases[i].name)});
        }
    }
    if (!project.systemOperations.empty()) {
        items.push_back({2, "System Operations", "system-operations"});
        for (size_t i = 0; i < project.systemOperations.size(); ++i) {
            std::string sig = operationSignature(project.systemOperations[i]);
            items.push_back({3, sig, "system-operation-" + std::to_string(i + 1) + "-" + slugToken(sig)});
        }
    }
    if (!project.operationContracts.empty()) {
        items.push_back({2, "Operation Contracts", "operation-contracts"});
        for (size_t i = 0; i < project.operationContracts.size(); ++i) {
            items.push_back({3, project.operationContracts[i].operation, "operation-contract-" + std::to_string(i + 1) + "-" + slugToken(project.operationContracts[i].operation)});
        }
    }

    if (hasDesign) {
        items.push_back({1, "Design", "design"});
        addDiagramGroupsToContents(items, assets, "design");
    }
    if (!project.classes.empty()) {
        items.push_back({2, "Classes", "classes"});
        for (size_t i = 0; i < project.classes.size(); ++i) {
            items.push_back({3, "Class: " + project.classes[i].name, "class-" + std::to_string(i + 1) + "-" + slugToken(project.classes[i].name)});
        }
    }
    if (!project.enums.empty()) {
        items.push_back({2, "Enumerations", "enumerations"});
        for (size_t i = 0; i < project.enums.size(); ++i) {
            items.push_back({3, "Enum: " + project.enums[i].name, "enum-" + std::to_string(i + 1) + "-" + slugToken(project.enums[i].name)});
        }
    }
    if (!project.relationships.empty()) items.push_back({2, "Relationships", "relationships"});

    if (hasImplementation) {
        items.push_back({1, "Implementation", "implementation"});
        addDiagramGroupsToContents(items, assets, "implementation");
        if (hasMethods(project)) items.push_back({2, "Class Operations", "class-operations"});
    }

    return items;
}

void writeContents(std::stringstream& ss, const std::vector<TocItem>& items) {
    ss << "<nav id=\"TOC\" class=\"modt-contents\">\n";
    ss << "<h2>Contents</h2>\n";
    ss << "<ul>\n";
    for (const auto& item : items) {
        ss << std::format("<li class=\"toc-level-{}\"><a href=\"#{}\">{}</a></li>\n",
            item.level,
            item.id,
            htmlEscape(item.title));
    }
    ss << "</ul>\n";
    ss << "</nav>\n\n";
}

std::string DocGenerator::generate(const Model::Project& project) {
    return generate(project, {});
}

std::string DocGenerator::generate(const Model::Project& project, const std::vector<DocumentationAsset>& assets) {
    std::stringstream ss;
    std::string reportTitle = !project.documentation.title.empty()
        ? project.documentation.title
        : (project.title.empty() ? project.name : project.title);
    const std::vector<TocItem> tocItems = buildContents(project, assets);

    if (project.documentation.titlePage) {
        std::string coverTitle = firstNonEmpty({
            project.documentation.coverTitle,
            project.title,
            project.name,
            reportTitle
        });
        std::string coverSubtitle = firstNonEmpty({
            project.documentation.coverSubtitle,
            project.documentation.subtitle,
            "System documentation"
        });
        std::string coverNote = firstNonEmpty({
            project.documentation.coverNote,
            project.description
        });

        ss << "<div class=\"modt-title-page\">\n";
        ss << std::format("<p class=\"modt-title-name\">{}</p>\n", htmlEscape(coverTitle));
        if (!coverSubtitle.empty()) {
            ss << std::format("<p class=\"modt-title-subtitle\">{}</p>\n", htmlEscape(coverSubtitle));
        }
        if (!coverNote.empty()) {
            ss << std::format("<p class=\"modt-title-note\">{}</p>\n", htmlEscape(coverNote));
        }

        std::vector<Model::DocumentationMetadata> metadata = project.documentation.metadata;
        if (!project.name.empty() && project.name != coverTitle) {
            metadata.insert(metadata.begin(), {"System", project.name});
        }
        if (!metadata.empty()) {
            ss << "<table class=\"modt-title-metadata\">\n";
            for (const auto& item : metadata) {
                ss << std::format("<tr><th>{}</th><td>{}</td></tr>\n",
                    htmlEscape(item.label),
                    htmlEscape(item.value));
            }
            ss << "</table>\n";
        }
        ss << "</div>\n\n";
    }

    writeContents(ss, tocItems);

    ss << markdownHeading(1, reportTitle, "report-overview");

    if (!project.documentation.subtitle.empty()) {
        ss << std::format("_{}_\n\n", project.documentation.subtitle);
    }

    if (!project.description.empty()) {
        ss << project.description << "\n\n";
    }

    ss << "| Item | Count |\n";
    ss << "| --- | ---: |\n";
    ss << std::format("| Requirements | {} |\n", project.supplementaryRequirements.size());
    ss << std::format("| Glossary terms | {} |\n", project.glossary.size());
    ss << std::format("| Use cases | {} |\n", project.useCases.size());
    ss << std::format("| System operations | {} |\n", project.systemOperations.size());
    ss << std::format("| Operation contracts | {} |\n", project.operationContracts.size());
    ss << std::format("| Classes | {} |\n", project.classes.size());
    ss << std::format("| Relationships | {} |\n", project.relationships.size());
    ss << "\n";

    bool hasDomain = hasAssetsForSection(assets, "domain") || !project.glossary.empty();
    bool hasAnalysis = !project.supplementaryRequirements.empty()
        || !project.useCases.empty() || !project.systemOperations.empty() || !project.operationContracts.empty()
        || hasAssetsForSection(assets, "analysis");
    bool hasDesign = !project.classes.empty() || !project.enums.empty() || !project.relationships.empty()
        || hasAssetsForSection(assets, "design");
    bool hasImplementation = hasMethods(project) || hasAssetsForSection(assets, "implementation");

    if (hasDomain) {
        ss << markdownHeading(2, "Domain", "domain");
        writeSectionDiagrams(ss, project, assets, "domain");
    }

    if (!project.glossary.empty()) {
        ss << markdownHeading(3, "Glossary", "glossary");
        ss << "| Term | Definition | Rules |\n";
        ss << "| --- | --- | --- |\n";
        for (const auto& term : project.glossary) {
            std::string rules = "";
            for (const auto& rule : term.rules) {
                if (!rules.empty()) rules += "<br>";
                rules += rule;
            }
            ss << std::format("| {} | {} | {} |\n",
                term.term,
                term.definition.empty() ? "-" : term.definition,
                rules.empty() ? "-" : rules);
        }
        ss << "\n";
    }

    if (hasAnalysis) {
        ss << markdownHeading(2, "Analysis", "analysis");
        writeSectionDiagrams(ss, project, assets, "analysis");
    }

    if (!project.supplementaryRequirements.empty()) {
        ss << markdownHeading(3, "Supplementary Specification", "supplementary-specification");
        ss << "| Category | Requirement | Description |\n";
        ss << "| --- | --- | --- |\n";
        for (const auto& req : project.supplementaryRequirements) {
            ss << std::format("| {} | {} | {} |\n",
                req.category.empty() ? "constraint" : req.category,
                req.name,
                req.description.empty() ? "-" : req.description);
        }
        ss << "\n";
    }

    if (!project.useCases.empty()) {
        ss << markdownHeading(3, "Use Cases", "use-cases");
        ss << "| Use Case | Primary Actor | Steps | Preconditions | Postconditions |\n";
        ss << "| --- | --- | ---: | ---: | ---: |\n";
        for (const auto& uc : project.useCases) {
            ss << std::format("| {} | {} | {} | {} | {} |\n",
                uc.name,
                uc.actor.empty() ? "-" : translateStereotype(uc.actor),
                uc.actions.size(),
                uc.preconditions.size(),
                uc.postconditions.size());
        }
        ss << "\n";

        for (size_t i = 0; i < project.useCases.size(); ++i) {
            const auto& uc = project.useCases[i];
            ss << markdownHeading(4, uc.name, "use-case-" + std::to_string(i + 1) + "-" + slugToken(uc.name));
            if (!uc.description.empty()) ss << std::format("**Description:** {}\n\n", uc.description);
            if (!uc.actor.empty()) ss << std::format("**Actor:** {}\n\n", translateStereotype(uc.actor));
            
            if (!uc.preconditions.empty()) {
                ss << "**Preconditions:**\n";
                for (const auto& pre : uc.preconditions) {
                    if (pre.find('.') != std::string::npos || pre[0] == '#' || pre[0] == '!') {
                        ss << std::format("- `{}`\n", pre);
                    } else {
                        ss << std::format("- {}\n", pre);
                    }
                }
                ss << "\n";
            }

            ss << "**Flow of Events:**\n";
            if (uc.actions.empty()) {
                ss << "_No steps defined._\n\n";
            } else {
                int stepNum = 1;
                for (const auto& action : uc.actions) {
                    std::string prefix = "";
                    if (action.isAlternative) {
                        prefix += action.condition.empty() ? "Alt: " : std::format("Alt: {} ", action.condition);
                    } else if (!action.condition.empty()) {
                        prefix += std::format("[{}] ", action.condition);
                    }
                    
                    std::string actionNameTrimmed = trimString(action.name);

                    if (!action.gotoLabel.empty()) {
                        ss << std::format("{}. {}**Repeat** step labeled @{}\n", stepNum++, prefix, action.gotoLabel);
                    } else {
                        if (!action.label.empty()) {
                            prefix += std::format("@{} ", action.label);
                        }
                        ss << std::format("{}. {}{} ", stepNum++, prefix, actionNameTrimmed);
                        if (!action.target.empty() && action.target != "@sys" && action.target != "sys") {
                            std::string targetLabel = translateStereotype(action.target);
                            ss << std::format("(Target: {})", targetLabel);
                        }
                        ss << "\n";
                    }
                    
                    if (!action.parameters.empty()) {
                        for (const auto& param : action.parameters) {
                            if (param.name == "goto") {
                                ss << std::format("    - **Repeat** step labeled @{}\n", param.type);
                            } else {
                                ss << std::format("    - {}{}\n", param.name, (param.type.empty() ? "" : ": " + param.type));
                            }
                        }
                    }
                }
                ss << "\n";
            }

            if (!uc.postconditions.empty()) {
                ss << "**Postconditions:**\n";
                for (const auto& post : uc.postconditions) {
                    if (post.find('.') != std::string::npos || post[0] == '#' || post[0] == '!') {
                        ss << std::format("- `{}`\n", post);
                    } else {
                        ss << std::format("- {}\n", post);
                    }
                }
                ss << "\n";
            }
        }
    }

    if (!project.systemOperations.empty()) {
        ss << markdownHeading(3, "System Operations", "system-operations");
        ss << "| Operation | Actor | Use Case |\n";
        ss << "| --- | --- | --- |\n";
        for (const auto& operation : project.systemOperations) {
            ss << std::format("| {} | {} | {} |\n",
                operationSignature(operation),
                operation.actor.empty() ? "-" : translateStereotype(operation.actor),
                operation.useCase.empty() ? "-" : operation.useCase);
        }
        ss << "\n";

        for (size_t i = 0; i < project.systemOperations.size(); ++i) {
            const auto& operation = project.systemOperations[i];
            std::string sig = operationSignature(operation);

            ss << markdownHeading(4, sig, "system-operation-" + std::to_string(i + 1) + "-" + slugToken(sig));

            if (!operation.actor.empty()) ss << std::format("**Actor:** {}\n\n", translateStereotype(operation.actor));
            if (!operation.useCase.empty()) ss << std::format("**Use Case:** {}\n\n", operation.useCase);

            if (!operation.preconditions.empty()) {
                ss << "**Preconditions:**\n";
                for (const auto& pre : operation.preconditions) {
                    if (pre.find('.') != std::string::npos || pre[0] == '#' || pre[0] == '!') {
                        ss << std::format("- `{}`\n", pre);
                    } else {
                        ss << std::format("- {}\n", pre);
                    }
                }
                ss << "\n";
            }

            if (!operation.postconditions.empty()) {
                ss << "**Postconditions:**\n";
                for (const auto& post : operation.postconditions) {
                    if (post.find('.') != std::string::npos || post[0] == '#' || post[0] == '!') {
                        ss << std::format("- `{}`\n", post);
                    } else {
                        ss << std::format("- {}\n", post);
                    }
                }
                ss << "\n";
            }

            if (!operation.notes.empty()) {
                ss << "**Notes:**\n";
                for (const auto& note : operation.notes) {
                    ss << std::format("- {}\n", note);
                }
                ss << "\n";
            }
        }
    }

    if (!project.operationContracts.empty()) {
        ss << markdownHeading(3, "Operation Contracts", "operation-contracts");
        for (size_t i = 0; i < project.operationContracts.size(); ++i) {
            const auto& contract = project.operationContracts[i];
            ss << markdownHeading(4, contract.operation, "operation-contract-" + std::to_string(i + 1) + "-" + slugToken(contract.operation));
            if (!contract.useCase.empty()) ss << std::format("**Use Case:** {}\n\n", contract.useCase);

            if (!contract.preconditions.empty()) {
                ss << "**Preconditions:**\n";
                for (const auto& pre : contract.preconditions) {
                    if (pre.find('.') != std::string::npos || pre[0] == '#' || pre[0] == '!') {
                        ss << std::format("- `{}`\n", pre);
                    } else {
                        ss << std::format("- {}\n", pre);
                    }
                }
                ss << "\n";
            }

            if (!contract.postconditions.empty()) {
                ss << "**Postconditions:**\n";
                for (const auto& post : contract.postconditions) {
                    if (post.find('.') != std::string::npos || post[0] == '#' || post[0] == '!') {
                        ss << std::format("- `{}`\n", post);
                    } else {
                        ss << std::format("- {}\n", post);
                    }
                }
                ss << "\n";
            }

            if (!contract.notes.empty()) {
                ss << "**Notes:**\n";
                for (const auto& note : contract.notes) {
                    ss << std::format("- {}\n", note);
                }
                ss << "\n";
            }
        }
    }

    if (hasDesign) {
        ss << markdownHeading(2, "Design", "design");
        writeSectionDiagrams(ss, project, assets, "design");
    }

    if (!project.classes.empty()) {
        ss << markdownHeading(3, "Classes", "classes");
        ss << "| Class | Attributes | Methods | Responsibility Signal |\n";
        ss << "| --- | ---: | ---: | --- |\n";
        for (const auto& cls : project.classes) {
            ss << std::format("| {} | {} | {} | {} |\n",
                cls.name,
                cls.attributes.size(),
                cls.methods.size(),
                classResponsibilitySummary(cls));
        }
        ss << "\n";

        for (size_t i = 0; i < project.classes.size(); ++i) {
            const auto& cls = project.classes[i];
            ss << markdownHeading(4, "Class: " + cls.name, "class-" + std::to_string(i + 1) + "-" + slugToken(cls.name));

            if (!cls.baseClass.empty()) ss << std::format("**Inherits from:** {}\n\n", cls.baseClass);

            if (!cls.stereotypes.empty()) {
                ss << "**Stereotypes:** ";
                ss << "`<<";
                for (size_t i = 0; i < cls.stereotypes.size(); ++i) {
                    ss << translateStereotype(cls.stereotypes[i]);
                    if (i < cls.stereotypes.size() - 1) ss << ", ";
                }
                ss << ">>` \n\n";
            }
            
            if (!cls.attributes.empty()) {
                ss << "##### Attributes\n\n";
                ss << "| Attribute | Type | Visibility | Metadata |\n";
                ss << "| --- | --- | --- | --- |\n";
                for (const auto& attr : cls.attributes) {
                    ss << std::format("| {} | {} | {} | ", 
                        attr.name, 
                        (attr.type.empty() ? "-" : attr.type), 
                        (attr.visibility.empty() ? "unspecified" : attr.visibility));
                    
                    if (!attr.metadata.empty()) {
                        bool first = true;
                        for (auto it = attr.metadata.begin(); it != attr.metadata.end(); ++it) {
                            if (!first) ss << ", ";
                            ss << std::format("`{}", it->first);
                            if (!it->second.empty()) ss << std::format("({})", it->second);
                            ss << "`";
                            first = false;
                        }
                    } else {
                        ss << "-";
                    }
                    ss << " |\n";
                }
                ss << "\n";
            }

        }
    }

    if (!project.enums.empty()) {
        ss << markdownHeading(3, "Enumerations", "enumerations");
        for (size_t i = 0; i < project.enums.size(); ++i) {
            const auto& en = project.enums[i];
            ss << markdownHeading(4, "Enum: " + en.name, "enum-" + std::to_string(i + 1) + "-" + slugToken(en.name));

            ss << "**Values:**\n";
            for (const auto& v : en.values) {
                ss << std::format("- `{}`\n", v);
            }
            ss << "\n";
        }
    }

    if (!project.relationships.empty()) {
        ss << markdownHeading(3, "Relationships", "relationships");
        ss << "| From | Type | To | Label |\n";
        ss << "| --- | --- | --- | --- |\n";
        for (const auto& rel : project.relationships) {
            std::string fromPart = rel.from;
            if (!rel.fromLabel.empty()) fromPart = std::format("{} \"{}\"", rel.from, rel.fromLabel);
            
            std::string toPart = rel.to;
            if (!rel.toLabel.empty()) toPart = std::format("{} \"{}\"", rel.to, rel.toLabel);

            ss << std::format("| {} | {} | {} | {} |\n", 
                fromPart,
                rel.type,
                toPart,
                (rel.label.empty() ? "--" : rel.label));
        }
        ss << "\n";
    }

    if (hasImplementation) {
        ss << markdownHeading(2, "Implementation", "implementation");
        writeSectionDiagrams(ss, project, assets, "implementation");
    }

    if (hasMethods(project)) {
        ss << markdownHeading(3, "Class Operations", "class-operations");
        for (const auto& cls : project.classes) {
            if (cls.methods.empty()) continue;

            ss << markdownHeading(4, "Class: " + cls.name + " Operations", "class-operations-" + slugToken(cls.name));
            ss << "| Method | Visibility | Effects | Metadata | Pre/Post Conditions |\n";
            ss << "| --- | --- | --- | --- | --- |\n";
            for (const auto& method : cls.methods) {
                std::string sig = method.name + "(";
                for (size_t i = 0; i < method.parameters.size(); ++i) {
                    sig += method.parameters[i].name;
                    if (!method.parameters[i].type.empty()) sig += ": " + method.parameters[i].type;
                    if (i < method.parameters.size() - 1) sig += ", ";
                }
                sig += ")";

                ss << std::format("| {} | {} | ",
                    sig,
                    (method.visibility.empty() ? "unspecified" : method.visibility));

                std::string metaString = "";
                for (auto it = method.metadata.begin(); it != method.metadata.end(); ++it) {
                    if (it != method.metadata.begin()) metaString += ", ";
                    metaString += std::format("`{}", it->first);
                    if (!it->second.empty()) metaString += std::format("({})", it->second);
                    metaString += "`";
                }
                if (metaString.empty()) metaString = "-";

                std::string effects = "";
                for (const auto& eff : method.effects) {
                    std::string from = eff.fromValue.empty() ? "" : (std::format("({}) ", eff.fromValue));
                    effects += std::format("`{}`: {}{} -> {}",
                        (eff.trigger.empty() ? "Always" : eff.trigger),
                        from, eff.variable, eff.value);
                    if (&eff != &method.effects.back()) effects += "<br>";
                }
                ss << (effects.empty() ? "-" : effects) << " | " << metaString << " | ";

                std::string conds = "";
                for (const auto& pre : method.preconditions) {
                    std::string p = pre;
                    for (const auto& attr : cls.attributes) {
                        if (attr.metadata.contains("state") && p.find(attr.name) == std::string::npos && p.find("_") != std::string::npos) {
                            p = attr.name + " == " + p;
                        }
                    }
                    conds += "**pre** " + p + "<br>";
                }
                for (const auto& post : method.postconditions) {
                    std::string p = post;
                    for (const auto& attr : cls.attributes) {
                        if (attr.metadata.contains("state") && p.find(attr.name) == std::string::npos && p.find("_") != std::string::npos) {
                            p = attr.name + " := " + p;
                        }
                    }
                    conds += "**post** " + p + "<br>";
                }
                ss << (conds.empty() ? "-" : conds) << " |\n";
            }
            ss << "\n";
        }
    }

    return ss.str();
}

std::string DocGenerator::translateStereotype(const std::string& s) {
    if (s == "i") return "i";
    if (s == "@sys") return "system";
    if (s == "@user" || s == "user") return "User";
    if (s == "@ext" || s == "ext") return "External Service";
    if (s == "@db" || s == "db") return "Database";
    if (s == "@api" || s == "api") return "External API";
    if (!s.empty() && s[0] == '@') return s.substr(1);
    return s;
}

} // namespace Generator
