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
#include <cctype>
#ifdef __linux__
#include <unistd.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#include "Parser/Parser.hpp"
#include "Generator/PumlGenerator.hpp"
#include "Generator/SqlGenerator.hpp"
#include "Generator/DocGenerator.hpp"

namespace fs = std::filesystem;

namespace {

bool isValidVersionString(const std::string& v) {
    if (v.empty() || v.size() > 32) return false;
    for (char c : v) {
        if (!std::isalnum(c) && c != '.' && c != '-' && c != '_') return false;
    }
    return true;
}

std::string getCurrentExePath() {
#ifdef __linux__
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) { buf[len] = '\0'; return std::string(buf); }
#elif defined(_WIN32)
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (len > 0) return std::string(buf, len);
#endif
    return "";
}

int performUpdate(const std::string& currentVersion) {
    const std::string repoOwner = "fredoslav2004";
    const std::string repoName  = "modt-core";
    const std::string apiUrl    = "https://api.github.com/repos/" + repoOwner + "/" + repoName + "/releases/latest";

    // Verify curl is available
#ifdef _WIN32
    if (std::system("curl --version > NUL 2>&1") != 0) {
#else
    if (std::system("curl --version > /dev/null 2>&1") != 0) {
#endif
        std::cerr << "Error: 'curl' is required for updates but was not found.\n";
        return 1;
    }

    // Temp working directory
#ifdef _WIN32
    const char* tmp = std::getenv("TEMP");
    std::string tmpDir = std::string(tmp ? tmp : "C:\\Temp") + "\\modt_update";
    std::string tmpJson = tmpDir + "\\release.json";
    std::string fetchCmd = "curl -s -L -H \"Accept: application/vnd.github.v3+json\" -A \"modt-updater\" \"" + apiUrl + "\" -o \"" + tmpJson + "\"";
#else
    std::string tmpDir  = "/tmp/modt_update";
    std::string tmpJson = tmpDir + "/release.json";
    std::string fetchCmd = "curl -s -L -H 'Accept: application/vnd.github.v3+json' -A 'modt-updater' '" + apiUrl + "' -o '" + tmpJson + "'";
#endif

    fs::create_directories(tmpDir);

    std::cout << "Checking for updates...\n";
    if (std::system(fetchCmd.c_str()) != 0) {
        std::cerr << "Error: Failed to fetch release info. Check your internet connection.\n";
        fs::remove_all(tmpDir);
        return 1;
    }

    // Read JSON response
    std::ifstream jsonFile(tmpJson);
    if (!jsonFile.is_open()) {
        std::cerr << "Error: Could not read release info.\n";
        fs::remove_all(tmpDir);
        return 1;
    }
    std::string json((std::istreambuf_iterator<char>(jsonFile)), std::istreambuf_iterator<char>());
    jsonFile.close();

    // Parse tag_name field
    const std::string tagKey = "\"tag_name\"";
    size_t tagPos = json.find(tagKey);
    if (tagPos == std::string::npos) {
        std::cerr << "Error: Could not parse release info from server.\n";
        fs::remove_all(tmpDir);
        return 1;
    }
    size_t colon = json.find(':', tagPos + tagKey.size());
    size_t start = json.find('"', colon + 1) + 1;
    size_t end   = json.find('"', start);
    std::string latestTag = json.substr(start, end - start);

    if (!isValidVersionString(latestTag)) {
        std::cerr << "Error: Invalid version string received from server.\n";
        fs::remove_all(tmpDir);
        return 1;
    }

    std::string latestVersion = latestTag;
    if (!latestVersion.empty() && latestVersion[0] == 'v') latestVersion = latestVersion.substr(1);

    if (latestVersion == currentVersion) {
        std::cout << "MODT is already up to date (v" << currentVersion << ").\n";
        fs::remove_all(tmpDir);
        return 0;
    }

    std::cout << "Update available: v" << currentVersion << " -> v" << latestVersion << "\n";
    std::cout << "Downloading...\n";

    // Build platform-specific paths and commands
#ifdef _WIN32
    std::string archiveName     = "modt-" + latestVersion + "-windows.zip";
    std::string tmpArchive      = tmpDir + "\\" + archiveName;
    std::string extractedBinary = tmpDir + "\\modt-" + latestVersion + "-windows\\modt.exe";
    std::string downloadUrl     = "https://github.com/" + repoOwner + "/" + repoName + "/releases/download/" + latestTag + "/" + archiveName;
    std::string downloadCmd     = "curl -# -L \"" + downloadUrl + "\" -o \"" + tmpArchive + "\"";
    std::string extractCmd      = "powershell -Command \"Expand-Archive -Path '" + tmpArchive + "' -DestinationPath '" + tmpDir + "' -Force\"";
#else
    std::string archiveName     = "modt-" + latestVersion + "-linux.tar.gz";
    std::string tmpArchive      = tmpDir + "/" + archiveName;
    std::string extractedBinary = tmpDir + "/modt-" + latestVersion + "-linux/modt";
    std::string downloadUrl     = "https://github.com/" + repoOwner + "/" + repoName + "/releases/download/" + latestTag + "/" + archiveName;
    std::string downloadCmd     = "curl -# -L '" + downloadUrl + "' -o '" + tmpArchive + "'";
    std::string extractCmd      = "tar -xzf '" + tmpArchive + "' -C '" + tmpDir + "'";
#endif

    if (std::system(downloadCmd.c_str()) != 0) {
        std::cerr << "Error: Failed to download update.\n";
        fs::remove_all(tmpDir);
        return 1;
    }

    if (std::system(extractCmd.c_str()) != 0) {
        std::cerr << "Error: Failed to extract update.\n";
        fs::remove_all(tmpDir);
        return 1;
    }

    if (!fs::exists(extractedBinary)) {
        std::cerr << "Error: Binary not found in downloaded archive.\n";
        fs::remove_all(tmpDir);
        return 1;
    }

    std::string currentExe = getCurrentExePath();
    if (currentExe.empty()) {
        std::cerr << "Error: Could not determine current executable path.\n";
        fs::remove_all(tmpDir);
        return 1;
    }

    std::error_code ec;
#ifdef _WIN32
    // Windows locks running executables — rename the old one first
    std::string oldExe = currentExe + ".old";
    fs::rename(currentExe, oldExe, ec);
    if (ec) {
        std::cerr << "Error: Could not replace binary: " << ec.message() << "\n";
        fs::remove_all(tmpDir);
        return 1;
    }
    fs::copy_file(extractedBinary, currentExe, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        fs::rename(oldExe, currentExe); // attempt restore
        std::cerr << "Error: Could not write new binary: " << ec.message() << "\n";
        fs::remove_all(tmpDir);
        return 1;
    }
    fs::remove(oldExe, ec);
#else
    fs::copy_file(extractedBinary, currentExe, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "Error: Could not replace binary: " << ec.message() << "\n";
        if (ec == std::errc::permission_denied) {
            std::cerr << "Tip: Try running with elevated privileges: sudo modt --update\n";
        }
        fs::remove_all(tmpDir);
        return 1;
    }
    fs::permissions(currentExe,
        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
        fs::perm_options::add, ec);
#endif

    fs::remove_all(tmpDir);
    std::cout << "MODT updated to v" << latestVersion << " successfully!\n";
    return 0;
}

bool containsModtFiles(const fs::path& rootPath) {
    if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) {
        return false;
    }

    try {
        for (const auto& entry : fs::recursive_directory_iterator(rootPath, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file() && entry.path().extension() == ".modt") {
                return true;
            }
        }
    } catch (const fs::filesystem_error&) {
        return false;
    }

    return false;
}

int scaffoldHelloWorldProject(const fs::path& targetDir) {
    const fs::path modtPath = targetDir / "hello.modt";
    const fs::path readmePath = targetDir / "README.md";

    if (fs::exists(modtPath)) {
        std::cerr << "Error: Refusing to overwrite existing file: " << modtPath.string() << "\n";
        return 1;
    }

    const std::string helloWorldModel = R"MODT(system
    name HelloMODT
    title My First Project
    description "A tiny MODT starter project"

artifacts
    docs generated/docs/
    design generated/design/ [svg]
    sql generated/sql/hello.sql

obj User
    name: string
    login()

obj Database
    rel "uses" -- User
)MODT";

    const std::string readmeContent = R"README(# Hello MODT

This starter project was generated by `modt hello-world`.

## Next steps

1. Edit `hello.modt`
2. Run `modt` from this folder
3. Check the generated output in `generated/`
)README";

    fs::create_directories(targetDir);

    std::ofstream modtFile(modtPath);
    if (!modtFile.is_open()) {
        std::cerr << "Error: Could not write to " << modtPath.string() << "\n";
        return 1;
    }
    modtFile << helloWorldModel;
    modtFile.close();

    if (!fs::exists(readmePath)) {
        std::ofstream readmeFile(readmePath);
        if (!readmeFile.is_open()) {
            std::cerr << "Error: Could not write to " << readmePath.string() << "\n";
            return 1;
        }
        readmeFile << readmeContent;
    }

    std::cout << "Created: " << modtPath.string() << "\n";
    if (!fs::exists(readmePath) || fs::file_size(readmePath) > 0) {
        std::cout << "Ready: run `modt` in this folder to generate docs, SQL, and diagrams.\n";
    }

    return 0;
}

} // namespace

void printUsage() {
    std::cout << "Usage: modt [input-path] [options]" << std::endl;
    std::cout << "       modt --input <file-or-directory> [options]" << std::endl;
    std::cout << "       modt hello-world" << std::endl;
    std::cout << std::endl;
    std::cout << "If no input path is provided and the current directory contains .modt files," << std::endl;
    std::cout << "MODT will use the current directory automatically." << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --out-path <path>  Directory to save output files (Default: same as input)" << std::endl;
    std::cout << "  -genDesign         Generate Class Diagram (Design phase)" << std::endl;
    std::cout << "  -genDomain         Generate Domain Model (Analysis phase)" << std::endl;
    std::cout << "  -genSQL            Generate SQL/DDL schema" << std::endl;
    std::cout << "  -genDocs           Generate Markdown documentation" << std::endl;
    std::cout << "  -i, --interactive  Start in interactive mode" << std::endl;
    std::cout << "  -genSequence       Generate full Sequence Diagrams" << std::endl;
    std::cout << "  -h, --help         Show this help message" << std::endl;
    std::cout << "  -v, --version      Display the current version" << std::endl;
    std::cout << "  --update           Download and install the latest release from GitHub" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  hello-world        Create a minimal MODT starter project in the current folder" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string version = "1.2.7";
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
    bool showHelp = false;
    bool scaffoldHelloWorld = false;
    bool doUpdate = false;
    std::vector<std::string> positionalArgs;

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
        } else if (arg == "-h" || arg == "--help") {
            showHelp = true;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "MODT (Modeling Tool) Version " << version << std::endl;
            return 0;
        } else if (arg == "-i" || arg == "--interactive") {
            interactive = true;
        } else if (arg == "--update") {
            doUpdate = true;
        } else if (arg == "hello-world" || arg == "hello" || arg == "init") {
            scaffoldHelloWorld = true;
        } else if (!arg.empty() && arg[0] != '-') {
            positionalArgs.push_back(arg);
        }
    }

    if (showHelp) {
        printUsage();
        return 0;
    }

    if (doUpdate) {
        return performUpdate(version);
    }

    if (scaffoldHelloWorld) {
        if (!inputPath.empty() || !outPathArg.empty() || interactive || genPUML || genDomainModel || genSQL || genDocs || genActivity || genSSD || genSequence || genState || !positionalArgs.empty()) {
            std::cerr << "Error: hello-world must be run by itself.\n";
            return 1;
        }
        return scaffoldHelloWorldProject(fs::current_path());
    }

    if (inputPath.empty()) {
        if (positionalArgs.size() > 1) {
            std::cerr << "Error: Too many positional arguments. Pass a single input path or use --input.\n";
            return 1;
        }

        if (positionalArgs.size() == 1) {
            inputPath = positionalArgs.front();
        } else if (containsModtFiles(fs::current_path())) {
            inputPath = fs::current_path().string();
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
