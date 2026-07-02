/*
 * MODT (Modeling Tool)
 * Copyright (C) 2026 Eduard Fekete <modt@eduardfekete.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "Inspector.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <io.h>
#else
#include <ncurses.h>
#include <unistd.h>
#endif

namespace Inspector {
namespace {

struct DetailLine {
    std::string label;
    std::string value;
};

struct Part {
    std::string title;
    std::vector<DetailLine> details;
};

struct Section {
    std::string title;
    std::vector<Part> parts;
    bool hasEntries = true;
};

enum class Focus {
    Sections,
    Parts,
    Details,
};

std::string join(const std::vector<std::string>& values, const std::string& separator) {
    std::ostringstream ss;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) ss << separator;
        ss << values[i];
    }
    return ss.str();
}

std::string phaseLabel(bool analysis, bool design) {
    if (analysis && design) return "analysis, design";
    if (analysis) return "analysis";
    if (design) return "design";
    return "unspecified";
}

std::string attributeText(const Model::Attribute& attribute) {
    std::ostringstream ss;
    if (!attribute.visibility.empty()) ss << attribute.visibility;
    ss << attribute.name;
    if (!attribute.type.empty()) ss << ": " << attribute.type;
    if (!attribute.metadata.empty()) ss << " {" << attribute.metadata.size() << " metadata}";
    return ss.str();
}

std::string parametersText(const std::vector<Model::Attribute>& parameters) {
    std::vector<std::string> values;
    for (const auto& parameter : parameters) values.push_back(attributeText(parameter));
    return join(values, ", ");
}

std::string methodText(const Model::Method& method) {
    std::ostringstream ss;
    if (!method.visibility.empty()) ss << method.visibility;
    ss << method.name << "(" << parametersText(method.parameters) << ")";
    if (!method.returnType.empty()) ss << ": " << method.returnType;
    if (!method.modifiers.empty()) ss << " [" << join(method.modifiers, ", ") << "]";
    return ss.str();
}

std::string actionText(const Model::Action& action) {
    std::ostringstream ss;
    if (action.isAlternative) ss << "alt ";
    if (!action.label.empty()) ss << action.label << ": ";
    ss << action.name;
    if (!action.parameters.empty()) ss << "(" << parametersText(action.parameters) << ")";
    if (!action.target.empty()) ss << " -> " << action.target;
    if (!action.condition.empty()) ss << " [" << action.condition << "]";
    if (!action.gotoLabel.empty()) ss << " goto " << action.gotoLabel;
    return ss.str();
}

std::string relationshipText(const Model::Relationship& relationship) {
    std::ostringstream ss;
    ss << relationship.from;
    if (!relationship.fromLabel.empty()) ss << " \"" << relationship.fromLabel << "\"";
    ss << " " << relationship.type << " ";
    if (!relationship.toLabel.empty()) ss << "\"" << relationship.toLabel << "\" ";
    ss << relationship.to;
    if (!relationship.label.empty()) ss << " : " << relationship.label;
    return ss.str();
}

void addDetail(std::vector<DetailLine>& details, const std::string& label, const std::string& value) {
    if (!value.empty()) details.push_back({label, value});
}

void addCountDetail(std::vector<DetailLine>& details, const std::string& label, size_t count) {
    details.push_back({label, std::to_string(count)});
}

std::vector<Section> buildSections(const Model::Project& project, const std::vector<std::filesystem::path>& sourceFiles) {
    std::vector<Section> sections;

    Section overview{"Overview", {}};
    Part summary{"Project", {}};
    addDetail(summary.details, "Name", project.name);
    addDetail(summary.details, "Title", project.title);
    addDetail(summary.details, "Description", project.description);
    addCountDetail(summary.details, "Source files", sourceFiles.size());
    addCountDetail(summary.details, "Objects", project.classes.size());
    addCountDetail(summary.details, "Enums", project.enums.size());
    addCountDetail(summary.details, "Relationships", project.relationships.size());
    addCountDetail(summary.details, "Use cases", project.useCases.size());
    addCountDetail(summary.details, "System operations", project.systemOperations.size());
    overview.parts.push_back(summary);

    for (const auto& file : sourceFiles) {
        overview.parts.push_back({"Source: " + file.filename().string(), {{"Path", file.string()}}});
    }
    sections.push_back(overview);

    Section classes{"Objects", {}};
    for (const auto& klass : project.classes) {
        Part part{klass.name, {}};
        addDetail(part.details, "Phase", phaseLabel(klass.isAnalysis, klass.isDesign));
        addDetail(part.details, "Stereotypes", join(klass.stereotypes, ", "));
        addDetail(part.details, "Base class", klass.baseClass);
        addCountDetail(part.details, "Attributes", klass.attributes.size());
        for (const auto& attribute : klass.attributes) addDetail(part.details, "Attribute", attributeText(attribute));
        addCountDetail(part.details, "Methods", klass.methods.size());
        for (const auto& method : klass.methods) addDetail(part.details, "Method", methodText(method));
        classes.parts.push_back(part);
    }
    sections.push_back(classes);

    Section enums{"Enums", {}};
    for (const auto& enumValue : project.enums) {
        Part part{enumValue.name, {}};
        addDetail(part.details, "Phase", phaseLabel(enumValue.isAnalysis, enumValue.isDesign));
        addCountDetail(part.details, "Values", enumValue.values.size());
        for (const auto& value : enumValue.values) addDetail(part.details, "Value", value);
        enums.parts.push_back(part);
    }
    sections.push_back(enums);

    Section relationships{"Relationships", {}};
    for (const auto& relationship : project.relationships) {
        relationships.parts.push_back({relationshipText(relationship), {
            {"From", relationship.from},
            {"Type", relationship.type},
            {"To", relationship.to},
            {"Label", relationship.label},
            {"From label", relationship.fromLabel},
            {"To label", relationship.toLabel},
        }});
    }
    sections.push_back(relationships);

    Section useCases{"Use Cases", {}};
    for (const auto& useCase : project.useCases) {
        Part part{useCase.name, {}};
        addDetail(part.details, "Actor", useCase.actor);
        addDetail(part.details, "Description", useCase.description);
        for (const auto& precondition : useCase.preconditions) addDetail(part.details, "Precondition", precondition);
        for (const auto& action : useCase.actions) addDetail(part.details, "Step", actionText(action));
        for (const auto& postcondition : useCase.postconditions) addDetail(part.details, "Postcondition", postcondition);
        useCases.parts.push_back(part);
    }
    sections.push_back(useCases);

    Section operations{"System Operations", {}};
    for (const auto& operation : project.systemOperations) {
        Part part{operation.name, {}};
        addDetail(part.details, "Actor", operation.actor);
        addDetail(part.details, "Use case", operation.useCase);
        addDetail(part.details, "Parameters", parametersText(operation.parameters));
        addDetail(part.details, "Phase", phaseLabel(operation.isAnalysis, operation.isDesign));
        for (const auto& precondition : operation.preconditions) addDetail(part.details, "Precondition", precondition);
        for (const auto& postcondition : operation.postconditions) addDetail(part.details, "Postcondition", postcondition);
        for (const auto& note : operation.notes) addDetail(part.details, "Note", note);
        operations.parts.push_back(part);
    }
    sections.push_back(operations);

    Section contracts{"Operation Contracts", {}};
    for (const auto& contract : project.operationContracts) {
        Part part{contract.operation, {}};
        addDetail(part.details, "Use case", contract.useCase);
        for (const auto& precondition : contract.preconditions) addDetail(part.details, "Precondition", precondition);
        for (const auto& postcondition : contract.postconditions) addDetail(part.details, "Postcondition", postcondition);
        for (const auto& note : contract.notes) addDetail(part.details, "Note", note);
        contracts.parts.push_back(part);
    }
    sections.push_back(contracts);

    Section supplementary{"Supplementary Requirements", {}};
    for (const auto& requirement : project.supplementaryRequirements) {
        supplementary.parts.push_back({requirement.name, {
            {"Category", requirement.category},
            {"Description", requirement.description},
        }});
    }
    sections.push_back(supplementary);

    Section glossary{"Glossary", {}};
    for (const auto& term : project.glossary) {
        Part part{term.term, {}};
        addDetail(part.details, "Definition", term.definition);
        for (const auto& rule : term.rules) addDetail(part.details, "Rule", rule);
        glossary.parts.push_back(part);
    }
    sections.push_back(glossary);

    Section requestedArtifacts{"Requested Outputs", {}};
    for (const auto& artifact : project.requestedArtifacts) {
        Part part{artifact.type, {}};
        addDetail(part.details, "Platform", artifact.platform);
        addDetail(part.details, "Format", artifact.format);
        addDetail(part.details, "Output path", artifact.outputPath);
        requestedArtifacts.parts.push_back(part);
    }
    sections.push_back(requestedArtifacts);

    for (auto& section : sections) {
        if (section.parts.empty()) {
            section.parts.push_back({"No entries", {{"Status", "This section has no parsed items."}}});
            section.hasEntries = false;
        }
    }

    return sections;
}

bool isTerminal() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0 && _isatty(_fileno(stdout)) != 0;
#else
    return isatty(STDIN_FILENO) != 0 && isatty(STDOUT_FILENO) != 0;
#endif
}

std::string truncateToWidth(const std::string& value, int width) {
    if (width <= 0) return "";
    if (static_cast<int>(value.size()) <= width) return value;
    if (width <= 3) return value.substr(0, width);
    return value.substr(0, width - 3) + "...";
}

std::vector<std::string> wrapText(const std::string& text, int width) {
    std::vector<std::string> lines;
    if (width <= 0) return lines;

    std::istringstream words(text);
    std::string word;
    std::string line;
    while (words >> word) {
        while (static_cast<int>(word.size()) > width) {
            if (!line.empty()) {
                lines.push_back(line);
                line.clear();
            }
            lines.push_back(word.substr(0, width));
            word = word.substr(width);
        }

        if (line.empty()) {
            line = word;
        } else if (static_cast<int>(line.size() + 1 + word.size()) <= width) {
            line += " " + word;
        } else {
            lines.push_back(line);
            line = word;
        }
    }

    if (!line.empty()) lines.push_back(line);
    if (lines.empty()) lines.push_back("");
    return lines;
}

void printPanelLine(const std::string& content, int width) {
    std::cout << truncateToWidth(content, width);
    int remaining = width - static_cast<int>(std::min(content.size(), static_cast<size_t>(std::max(width, 0))));
    for (int i = 0; i < remaining; ++i) std::cout << ' ';
}

int scrollOffsetFor(size_t selected, size_t count, int visibleRows, int currentOffset) {
    if (count == 0 || visibleRows <= 0) return 0;

    int maxOffset = std::max(0, static_cast<int>(count) - visibleRows);
    int offset = std::clamp(currentOffset, 0, maxOffset);
    int selectedRow = static_cast<int>(selected);

    if (selectedRow < offset) {
        offset = selectedRow;
    } else if (selectedRow >= offset + visibleRows) {
        offset = selectedRow - visibleRows + 1;
    }

    return std::clamp(offset, 0, maxOffset);
}

std::vector<std::string> detailLinesFor(const Part& part, int width) {
    std::vector<std::string> detailLines;
    for (const auto& detail : part.details) {
        std::string value = detail.value.empty() ? "-" : detail.value;
        auto wrapped = wrapText(detail.label + ": " + value, width);
        detailLines.insert(detailLines.end(), wrapped.begin(), wrapped.end());
    }
    if (detailLines.empty()) {
        detailLines.push_back("No details available.");
    }
    return detailLines;
}

void printPlain(const std::vector<Section>& sections);

int runInteractive(const std::vector<Section>& sections) {
#ifdef _WIN32
    printPlain(sections);
    return 0;
#else
    setenv("TERM", "ansi", 1);
    SCREEN* screen = newterm("ansi", stdout, stdin);
    if (!screen) {
        printPlain(sections);
        return 0;
    }
    set_term(screen);
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    define_key("\033[A", KEY_UP);
    define_key("\033[B", KEY_DOWN);
    define_key("\033[C", KEY_RIGHT);
    define_key("\033[D", KEY_LEFT);
    define_key("\033[5~", KEY_PPAGE);
    define_key("\033[6~", KEY_NPAGE);
    nonl();
    set_escdelay(25);
    curs_set(0);

    size_t sectionIndex = 0;
    std::vector<size_t> partIndices(sections.size(), 0);
    std::vector<int> detailScrolls(sections.size(), 0);
    int sectionScroll = 0;
    int partScroll = 0;
    Focus focus = Focus::Sections;

    auto bodyRows = [] {
        return std::max(LINES, 10) - 4;
    };

    auto clampCurrentState = [&] {
        int visibleRows = bodyRows();
        sectionScroll = scrollOffsetFor(sectionIndex, sections.size(), visibleRows, sectionScroll);
        size_t partCount = sections[sectionIndex].parts.size();
        partIndices[sectionIndex] = std::min(partIndices[sectionIndex], partCount - 1);
        partScroll = scrollOffsetFor(partIndices[sectionIndex], partCount, visibleRows, partScroll);

        int cols = std::max(COLS, 60);
        int rightWidth = std::max(20, cols - std::max(18, cols / 4) - std::max(22, cols / 3) - 4);
        auto lines = detailLinesFor(sections[sectionIndex].parts[partIndices[sectionIndex]], rightWidth - 2);
        int detailRows = std::max(1, visibleRows - 1);
        int maxDetailScroll = std::max(0, static_cast<int>(lines.size()) - detailRows);
        detailScrolls[sectionIndex] = std::clamp(detailScrolls[sectionIndex], 0, maxDetailScroll);
    };

    auto drawPanelLine = [](int y, int x, int width, const std::string& text, bool selected, bool focused) {
        if (width <= 0) return;
        (void)selected;
        (void)focused;
        std::string display = truncateToWidth(text, width);
        mvaddnstr(y, x, display.c_str(), width);
        for (int i = static_cast<int>(display.size()); i < width; ++i) addch(' ');
    };

    auto draw = [&] {
        int rows = std::max(LINES, 10);
        int cols = std::max(COLS, 60);
        int leftWidth = std::max(18, cols / 4);
        int middleWidth = std::max(22, cols / 3);
        int rightWidth = std::max(20, cols - leftWidth - middleWidth - 4);
        int visibleRows = rows - 4;

        erase();
        std::string title = "MODT Inspector - " + sections[sectionIndex].title;
        mvaddnstr(0, 0, truncateToWidth(title, cols).c_str(), cols);
        mvaddnstr(1, 0, "Left/Right change column, Up/Down move, PageUp/PageDown scroll details, q exits", cols);
        mvhline(2, 0, '-', cols);

        const auto& section = sections[sectionIndex];
        const auto& part = section.parts[partIndices[sectionIndex]];
        auto detailLines = detailLinesFor(part, rightWidth - 2);

        for (int row = 0; row < visibleRows; ++row) {
            int y = row + 3;
            int sectionRow = sectionScroll + row;
            if (sectionRow < static_cast<int>(sections.size())) {
                bool selected = sectionRow == static_cast<int>(sectionIndex);
                std::string marker = selected ? (focus == Focus::Sections ? "> " : "* ") : "  ";
                drawPanelLine(y, 0, leftWidth, marker + sections[sectionRow].title, selected, focus == Focus::Sections);
            }

            mvaddch(y, leftWidth + 1, '|');

            int partRow = partScroll + row;
            if (partRow < static_cast<int>(section.parts.size())) {
                bool selected = partRow == static_cast<int>(partIndices[sectionIndex]);
                std::string marker = selected ? (focus == Focus::Parts ? "> " : "* ") : "  ";
                drawPanelLine(y, leftWidth + 3, middleWidth, marker + section.parts[partRow].title, selected, focus == Focus::Parts);
            }

            mvaddch(y, leftWidth + middleWidth + 4, '|');

            if (row == 0) {
                std::string marker = focus == Focus::Details ? "> " : "* ";
                drawPanelLine(y, leftWidth + middleWidth + 6, rightWidth, marker + part.title, true, focus == Focus::Details);
            } else {
                int detailRow = detailScrolls[sectionIndex] + row - 1;
                if (detailRow >= 0 && detailRow < static_cast<int>(detailLines.size())) {
                    drawPanelLine(y, leftWidth + middleWidth + 6, rightWidth, "  " + detailLines[detailRow], false, false);
                }
            }
        }

        refresh();
    };

    while (true) {
        clampCurrentState();
        draw();
        int key = getch();
        if (key == 'q' || key == 'Q' || key == 27) {
            break;
        }

        if (key == KEY_UP && focus == Focus::Sections && sectionIndex > 0) {
            --sectionIndex;
            partScroll = 0;
        } else if (key == KEY_DOWN && focus == Focus::Sections && sectionIndex + 1 < sections.size()) {
            ++sectionIndex;
            partScroll = 0;
        } else if (key == KEY_UP && focus == Focus::Parts && partIndices[sectionIndex] > 0) {
            --partIndices[sectionIndex];
            detailScrolls[sectionIndex] = 0;
        } else if (key == KEY_DOWN && focus == Focus::Parts && partIndices[sectionIndex] + 1 < sections[sectionIndex].parts.size()) {
            ++partIndices[sectionIndex];
            detailScrolls[sectionIndex] = 0;
        } else if (key == KEY_UP && focus == Focus::Details) {
            detailScrolls[sectionIndex] = std::max(0, detailScrolls[sectionIndex] - 1);
        } else if (key == KEY_DOWN && focus == Focus::Details) {
            detailScrolls[sectionIndex] += 1;
        } else if (key == KEY_LEFT && focus == Focus::Details) {
            focus = Focus::Parts;
        } else if (key == KEY_LEFT && focus == Focus::Parts) {
            focus = Focus::Sections;
        } else if (key == KEY_RIGHT && focus == Focus::Sections) {
            focus = Focus::Parts;
        } else if (key == KEY_RIGHT && focus == Focus::Parts) {
            focus = Focus::Details;
        } else if (key == KEY_PPAGE) {
            detailScrolls[sectionIndex] = std::max(0, detailScrolls[sectionIndex] - 5);
        } else if (key == KEY_NPAGE) {
            detailScrolls[sectionIndex] += 5;
        }
    }

    endwin();
    delscreen(screen);
    return 0;
#endif
}

void printPlain(const std::vector<Section>& sections) {
    std::cout << "MODT Inspector\n";
    for (const auto& section : sections) {
        std::cout << "\n[" << section.title << "]\n";
        for (const auto& part : section.parts) {
            std::cout << "- " << part.title << "\n";
            for (const auto& detail : part.details) {
                if (!detail.value.empty()) {
                    std::cout << "  " << detail.label << ": " << detail.value << "\n";
                }
            }
        }
    }
}

} // namespace

int run(const Model::Project& project, const std::vector<std::filesystem::path>& sourceFiles) {
    auto sections = buildSections(project, sourceFiles);
    if (!isTerminal()) {
        printPlain(sections);
        return 0;
    }

    std::vector<Section> interactiveSections;
    for (const auto& section : sections) {
        if (section.hasEntries) interactiveSections.push_back(section);
    }
    if (interactiveSections.empty()) interactiveSections = sections;

    return runInteractive(interactiveSections);
}

} // namespace Inspector
