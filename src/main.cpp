/*
 * MODT (Modeling Tool)
 * Copyright (C) 2026 Eduard Fekete <modt@eduardfekete.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <map>
#include "Parser/Parser.hpp"
#include "Generator/PumlGenerator.hpp"
#include "Generator/SqlGenerator.hpp"
#include "Generator/DocGenerator.hpp"

namespace fs = std::filesystem;

void printUsage() {
    std::cout << "Usage: modt --input <file.modt> [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --out-path <path>  Directory to save output files (Default: same as input)" << std::endl;
    std::cout << "  -genDesign         Generate Class Diagram (Design phase)" << std::endl;
    std::cout << "  -genDomain         Generate Domain Model (Analysis phase)" << std::endl;
    std::cout << "  -genSQL            Generate SQL/DDL schema" << std::endl;
    std::cout << "  -genDocs           Generate Markdown documentation" << std::endl;
    std::cout << "  -i, --interactive  Start in interactive mode" << std::endl;
    std::cout << "  -genSequence       Generate full Sequence Diagrams" << std::endl;
    std::cout << "  -v, --version      Display the current version" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string version = "1.2.3";
    std::string inputPath;
    std::string outPathArg;
    bool genPUML = false;
    bool genDomainModel = false;
    bool genSQL = false;
    bool genDocs = false;
    bool genActivity = false;
    bool genSSD = false;
    bool genSequence = false;
    bool genState = false;
    bool interactive = false;

    std::map<std::string, std::string> customPaths;
    std::map<std::string, std::string> customFormats;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            inputPath = argv[++i];
        } else if (arg == "--out-path" && i + 1 < argc) {
            outPathArg = argv[++i];
        } else if (arg == "-genPUML" || arg == "-genDesign") {
            genPUML = true;
        } else if (arg == "-genDomainModel" || arg == "-genDomain") {
            genDomainModel = true;
        } else if (arg == "-genSQL") {
            genSQL = true;
        } else if (arg == "-genDocs") {
            genDocs = true;
        } else if (arg == "-genActivity") {
            genActivity = true;
        } else if (arg == "-genSSD") {
            genSSD = true;
        } else if (arg == "-genSequence") {
            genSequence = true;
        } else if (arg == "-genState") {
            genState = true;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "MODT (Modeling Tool) Version " << version << std::endl;
            return 0;
        } else if (arg == "-i" || arg == "--interactive") {
            interactive = true;
        }
    }

    if (interactive) {
        if (inputPath.empty()) {
            std::cout << "Enter input path (.modt file or directory): ";
            std::getline(std::cin >> std::ws, inputPath);
        }
        
        std::string choice;
        auto ask = [&](const std::string& question) {
            std::cout << question << " (y/n): ";
            std::cin >> choice;
            return (choice == "y" || choice == "Y");
        };

        if (!genPUML) genPUML = ask("Generate Class Diagram (Design)?");
        if (!genDomainModel) genDomainModel = ask("Generate Domain Model (Analysis)?");
        if (!genSQL) genSQL = ask("Generate SQL Schema?");
        if (!genDocs) genDocs = ask("Generate Markdown Documentation?");
        if (!genActivity) genActivity = ask("Generate Activity Diagram?");
        if (!genSSD) genSSD = ask("Generate System Sequence Diagrams?");
        if (!genSequence) genSequence = ask("Generate Sequence Diagrams?");
        if (!genState) genState = ask("Generate State Machine Diagrams?");
    }

    if (inputPath.empty() && !interactive) {
        printUsage();
        return 1;
    }

    Parser::ModtParser parser;
    Model::Project project;

    fs::path inputPathObj(inputPath);
    if (!fs::exists(inputPathObj)) {
        std::cerr << "Error: Path does not exist: " << inputPath << "\n";
        return 1;
    }

    std::vector<fs::path> filesToProcess;
    if (fs::is_directory(inputPathObj)) {
        for (const auto& entry : fs::recursive_directory_iterator(inputPathObj)) {
            if (entry.path().extension() == ".modt") {
                filesToProcess.push_back(entry.path());
            }
        }
        project.name = inputPathObj.filename().string();
    } else {
        filesToProcess.push_back(inputPathObj);
        project.name = inputPathObj.stem().string();
    }

    if (filesToProcess.empty()) {
        std::cerr << "Error: No .modt files found to process.\n";
        return 1;
    }

    for (const auto& modtFile : filesToProcess) {
        std::cout << "Loading: " << modtFile.string() << "\n";
        parser.parseTo(modtFile.string(), project);
    }

    // Apply artifacts requested in modt files
    for (const auto& art : project.requestedArtifacts) {
        std::string type = art.type;
        std::transform(type.begin(), type.end(), type.begin(), ::tolower);
        
        auto setOptions = [&](const std::string& key) {
            if (!art.outputPath.empty()) customPaths[key] = art.outputPath;
            if (!art.format.empty()) customFormats[key] = art.format;
            else if (!art.platform.empty()) customFormats[key] = art.platform;
        };

        if (type == "design" || type == "puml" || type == "class") {
            genPUML = true;
            setOptions("design");
        } else if (type == "domain") {
            genDomainModel = true;
            setOptions("domain");
        } else if (type == "sql" || type == "ddl") {
            genSQL = true;
            setOptions("sql");
        } else if (type == "docs" || type == "markdown") {
            genDocs = true;
            setOptions("docs");
        } else if (type == "activity") {
            genActivity = true;
            setOptions("activity");
        } else if (type == "ssd") {
            genSSD = true;
            setOptions("ssd");
        } else if (type == "sequence" || type == "seq") {
            genSequence = true;
            setOptions("sequence");
        } else if (type == "state") {
            genState = true;
            setOptions("state");
        }
    }

    if (project.name == "Testing" || project.name.empty()) project.name = "Project";

    fs::path targetDir = outPathArg.empty() ? (fs::is_directory(inputPathObj) ? inputPathObj : inputPathObj.parent_path()) : fs::path(outPathArg);
    if (targetDir.empty()) targetDir = ".";

    auto triggerPlantUML = [&](const fs::path& pumlPath, const std::string& format) {
        if (format.empty()) return;
        
        std::string cmd = "plantuml -t" + format + " \"" + pumlPath.string() + "\"";
        std::cout << "Running: " << cmd << "\n";
        int result = std::system(cmd.c_str());
        if (result != 0) {
            std::cerr << "Warning: PlantUML command failed with code " << result << "\n";
        }
    };

    auto triggerPdfExport = [&](const fs::path& mdPath) {
        fs::path pdfPath = mdPath;
        pdfPath.replace_extension(".pdf");
        
        std::string cmd = "pandoc \"" + fs::absolute(mdPath).string() + "\" -o \"" + fs::absolute(pdfPath).string() + "\"";
        std::cout << "Running: " << cmd << "\n";
        int result = std::system(cmd.c_str());
        if (result != 0) {
            std::cerr << "Warning: PDF export failed with code " << result << ". Make sure 'pandoc' is installed and your environment is set up.\n";
        } else {
            std::cout << "Exported PDF: " << pdfPath.string() << "\n";
        }
    };

    auto saveFile = [&](const std::string& content, const std::string& suffix, const std::string& overridePath = "", const std::string& format = "") {
        if (content.empty()) return;
        
        fs::path fullOutPath;
        if (!overridePath.empty()) {
            fullOutPath = fs::path(overridePath);
            // If it's a directory or has no extension, treat it as a folder and append filename
            if (!fullOutPath.has_extension()) {
                fullOutPath = fullOutPath / (project.name + suffix);
            }
            if (fullOutPath.is_relative()) {
                fullOutPath = targetDir / fullOutPath;
            }
        } else {
            std::string fileName = project.name + suffix;
            fullOutPath = targetDir / fileName;
        }

        // Ensure directory exists
        if (fullOutPath.has_parent_path()) {
            fs::create_directories(fullOutPath.parent_path());
        }
        
        std::ofstream outFile(fullOutPath);
        if (outFile.is_open()) {
            outFile << content;
            outFile.close(); // Ensure file is written before triggering external tools

            std::cout << "Generated: " << fullOutPath.string() << "\n";
            if (!format.empty()) {
                if (fullOutPath.extension() == ".puml") {
                    triggerPlantUML(fullOutPath, format);
                } else if (fullOutPath.extension() == ".md" && format == "pdf") {
                    triggerPdfExport(fullOutPath);
                }
            }
        } else {
            std::cerr << "Error: Could not write to " << fullOutPath.string() << "\n";
        }
    };

    auto saveMultipleFiles = [&](const std::map<std::string, std::string>& files, const std::string& prefix, const std::string& suffix, const std::string& overrideDir = "", const std::string& format = "") {
        fs::path baseDir = targetDir;
        if (!overrideDir.empty()) {
            baseDir = fs::path(overrideDir);
            if (baseDir.is_relative()) {
                baseDir = targetDir / baseDir;
            }
        }

        for (const auto& [name, content] : files) {
            if (content.empty()) continue;
            std::string fileName = name;
            // Always apply prefix and suffix if provided
            if (!suffix.empty()) {
                fileName = prefix + "_" + fileName + suffix;
            }
            fs::path fullOutPath = baseDir / fileName;

            if (fullOutPath.has_parent_path()) {
                fs::create_directories(fullOutPath.parent_path());
            }

            std::ofstream outFile(fullOutPath);
            if (outFile.is_open()) {
                outFile << content;
                outFile.close(); // Ensure file is written before triggering external tools

                std::cout << "Generated: " << fullOutPath.string() << "\n";
                if (!format.empty()) {
                    if (fullOutPath.extension() == ".puml") {
                        triggerPlantUML(fullOutPath, format);
                    } else if (fullOutPath.extension() == ".md" && format == "pdf") {
                        triggerPdfExport(fullOutPath);
                    }
                }
            }
        }
    };

    Generator::PumlGenerator pumlGen;
    if (genPUML) {
        saveFile(pumlGen.generateDesignModel(project), ".design.puml", customPaths["design"], customFormats["design"]);
    }
    if (genDomainModel) {
        saveFile(pumlGen.generateDomainModel(project), ".domain.puml", customPaths["domain"], customFormats["domain"]);
    }
    
    if (genActivity && !project.useCases.empty()) {
        saveFile(pumlGen.generateActivityDiagram(project), ".activity.puml", customPaths["activity"], customFormats["activity"]);
    }

    if (genSSD && !project.useCases.empty()) {
        saveMultipleFiles(pumlGen.generateSystemSequenceDiagrams(project), project.name, ".ssd.puml", customPaths["ssd"], customFormats["ssd"]);
    }

    if (genSequence && !project.useCases.empty()) {
        saveMultipleFiles(pumlGen.generateSequenceDiagrams(project), project.name, ".sequence.puml", customPaths["sequence"], customFormats["sequence"]);
    }

    if (genState) {
        saveMultipleFiles(pumlGen.generateStateMachineDiagrams(project), project.name, ".state.puml", customPaths["state"], customFormats["state"]);
    }

    if (genSQL) {
        Generator::SqlGenerator sqlGen;
        std::string sql = sqlGen.generate(project);
        saveFile(sql, ".sql", customPaths["sql"]);
    }

    if (genDocs) {
        Generator::DocGenerator docGen;
        std::string docs = docGen.generate(project);
        saveFile(docs, ".md", customPaths["docs"], customFormats["docs"]);
    }

    return 0;
}
