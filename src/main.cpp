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
#include <stdexcept>
#include <sstream>
#include <functional>
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
#include "Inspector/Inspector.hpp"

namespace fs = std::filesystem;

namespace {

bool isValidVersionString(const std::string& v) {
    if (v.empty() || v.size() > 32) return false;
    for (char c : v) {
        if (!std::isalnum(c) && c != '.' && c != '-' && c != '_') return false;
    }
    return true;
}

std::string shellQuote(const std::string& value) {
#ifdef _WIN32
    std::string quoted = "\"";
    for (char c : value) {
        if (c == '"') quoted += "\\\"";
        else quoted += c;
    }
    quoted += "\"";
    return quoted;
#else
    std::string quoted = "'";
    for (char c : value) {
        if (c == '\'') quoted += "'\\''";
        else quoted += c;
    }
    quoted += "'";
    return quoted;
#endif
}

bool commandExists(const std::string& command) {
#ifdef _WIN32
    std::string check = "where " + shellQuote(command) + " > NUL 2>&1";
#else
    std::string check = "command -v " + shellQuote(command) + " > /dev/null 2>&1";
#endif
    return std::system(check.c_str()) == 0;
}

std::string cssStringLiteral(const std::string& value) {
    std::string escaped = "\"";
    for (char c : value) {
        switch (c) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\A "; break;
            case '\r': break;
            default: escaped += c; break;
        }
    }
    escaped += "\"";
    return escaped;
}

std::string defaultReportCss(const std::string& footerText = "") {
    std::stringstream css;
    css << R"CSS(@page {
  size: A4;
  margin: 18mm 18mm 20mm 18mm;
  @bottom-right {
    content: counter(page);
    font-family: "Inter", "Helvetica Neue", Arial, sans-serif;
    font-size: 9pt;
    color: #111;
  }
)CSS";

    if (!footerText.empty()) {
        css << R"CSS(  @bottom-left {
    content: )CSS" << cssStringLiteral(footerText) << R"CSS(;
    font-family: "Inter", "Helvetica Neue", Arial, sans-serif;
    font-size: 9pt;
    color: #555;
  }
)CSS";
    }

    css << R"CSS(}

body {
  font-family: "Inter", "Helvetica Neue", Arial, sans-serif;
  color: #080808;
  background: #fff;
  font-size: 10.5pt;
  line-height: 1.55;
}

.modt-title-page {
  min-height: 235mm;
  display: flex;
  flex-direction: column;
  justify-content: center;
  break-after: page;
  page-break-after: always;
}

.modt-title-kicker {
  margin: 0 0 8mm;
  font-size: 9pt;
  font-weight: 700;
  letter-spacing: .14em;
  text-transform: uppercase;
}

.modt-title-name {
  margin: 0;
  padding: 0 0 7mm;
  border-bottom: 2.2pt solid #000;
  font-size: 34pt;
  font-weight: 800;
  line-height: 1.05;
}

.modt-title-subtitle {
  margin: 7mm 0 0;
  font-size: 14pt;
  font-weight: 600;
}

.modt-title-note {
  max-width: 135mm;
  margin: 6mm 0 0;
  font-size: 10pt;
  color: #333;
}

.modt-title-metadata {
  width: auto;
  min-width: 85mm;
  max-width: 135mm;
  margin-top: 14mm;
  font-size: 8.8pt;
}

.modt-title-metadata th {
  width: 32mm;
  background: #fff;
  color: #000;
  border-color: #444;
  font-size: 8pt;
  text-transform: uppercase;
  letter-spacing: .08em;
}

.modt-title-metadata td {
  border-color: #444;
}

h1 {
  font-size: 31pt;
  line-height: 1.05;
  margin: 0 0 12mm;
  padding: 0 0 8mm;
  border-bottom: 2.5pt solid #000;
  letter-spacing: 0;
}

h2 {
  break-before: auto;
  margin: 12mm 0 4mm;
  padding-top: 2mm;
  border-top: 1.4pt solid #000;
  font-size: 16pt;
  line-height: 1.2;
}

h3 {
  margin: 8mm 0 2mm;
  font-size: 12.5pt;
  line-height: 1.25;
}

h4 {
  margin: 6mm 0 2mm;
  font-size: 10.5pt;
  text-transform: uppercase;
  letter-spacing: .08em;
}

p {
  margin: 0 0 3.5mm;
}

img {
  display: block;
  max-width: 100%;
  max-height: 220mm;
  width: auto;
  height: auto;
  object-fit: contain;
}

.modt-diagram {
  margin: 2mm auto 6mm;
  page-break-inside: avoid;
  break-inside: avoid;
}

a {
  color: #000;
  text-decoration-thickness: .4pt;
}

table {
  width: 100%;
  border-collapse: collapse;
  margin: 4mm 0 7mm;
  font-size: 9.2pt;
}

th {
  text-align: left;
  background: #000;
  color: #fff;
  font-weight: 700;
  border: .8pt solid #000;
  padding: 2.4mm 2mm;
}

td {
  vertical-align: top;
  border: .65pt solid #111;
  padding: 2.2mm 2mm;
}

tr:nth-child(even) td {
  background: #f4f4f4;
}

code {
  font-family: "Fira Code", "SFMono-Regular", Consolas, monospace;
  font-size: .92em;
  background: #f1f1f1;
  border: .55pt solid #cfcfcf;
  padding: .2mm 1mm;
}

pre {
  background: #0a0a0a;
  color: #fff;
  border: 1pt solid #000;
  padding: 4mm;
  white-space: pre-wrap;
}

pre code {
  background: transparent;
  border: 0;
  color: inherit;
  padding: 0;
}

blockquote {
  margin: 5mm 0;
  padding: 1mm 0 1mm 4mm;
  border-left: 2pt solid #000;
}

ul, ol {
  padding-left: 6mm;
}

li {
  margin-bottom: 1.4mm;
}

hr {
  border: 0;
  border-top: 1.2pt solid #000;
  margin: 10mm 0;
}

#TOC {
  border: 1.2pt solid #000;
  padding: 5mm;
  margin: 6mm 0 10mm;
}

#TOC ul {
  list-style: none;
  padding-left: 4mm;
}

#TOC .toc-level-2 {
  margin-left: 5mm;
  font-size: 8.8pt;
}
)CSS";
    return css.str();
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
    std::string replacementExe = currentExe + ".new";
    fs::remove(replacementExe, ec);
    ec.clear();

    fs::copy_file(extractedBinary, replacementExe, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "Error: Could not replace binary: " << ec.message() << "\n";
        if (ec == std::errc::permission_denied) {
            std::cerr << "Tip: Try running with elevated privileges: sudo modt --update\n";
        }
        fs::remove_all(tmpDir);
        return 1;
    }
    fs::permissions(replacementExe,
        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
        fs::perm_options::add, ec);
    ec.clear();

    fs::rename(replacementExe, currentExe, ec);
    if (ec) {
        fs::remove(replacementExe);
        std::cerr << "Error: Could not replace binary: " << ec.message() << "\n";
        if (ec == std::errc::permission_denied) {
            std::cerr << "Tip: Try running with elevated privileges: sudo modt --update\n";
        }
        fs::remove_all(tmpDir);
        return 1;
    }
#endif

    fs::remove_all(tmpDir);
    std::cout << "MODT updated to v" << latestVersion << " successfully!\n";
    return 0;
}

int printDocs() {
    std::vector<fs::path> searchPaths;

    // Look next to the binary first (tarball installs)
    std::string currentExe = getCurrentExePath();
    if (!currentExe.empty()) {
        fs::path exeDir = fs::path(currentExe).parent_path();
        searchPaths.push_back(exeDir / "Documentation.md");
    }

    // Standard system install locations (deb/rpm)
    searchPaths.push_back("/usr/share/doc/modt/Documentation.md");

    // Source checkout / local build convenience
    searchPaths.push_back(fs::current_path() / "Documentation.md");

    for (const auto& p : searchPaths) {
        if (fs::exists(p)) {
            std::ifstream f(p);
            if (f.is_open()) {
                std::cout << f.rdbuf();
                return 0;
            }
        }
    }

    std::cerr << "Documentation not found.\n";
    std::cerr << "Full documentation is available at: https://github.com/fredoslav2004/modt-core\n";
    return 1;
}

struct LoadedProject {
    Model::Project project;
    std::vector<fs::path> files;
    fs::path inputPath;
};

LoadedProject loadProject(const std::string& inputPath, bool verbose) {
    Parser::ModtParser parser;
    LoadedProject loaded;
    loaded.inputPath = fs::path(inputPath);

    if (!fs::exists(loaded.inputPath)) {
        throw std::runtime_error("Path does not exist: " + inputPath);
    }

    if (fs::is_directory(loaded.inputPath)) {
        for (const auto& entry : fs::recursive_directory_iterator(loaded.inputPath, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file() && entry.path().extension() == ".modt") {
                loaded.files.push_back(entry.path());
            }
        }
        std::sort(loaded.files.begin(), loaded.files.end());
        loaded.project.name = loaded.inputPath.filename().string();
    } else {
        loaded.files.push_back(loaded.inputPath);
        loaded.project.name = loaded.inputPath.stem().string();
    }

    if (loaded.files.empty()) {
        throw std::runtime_error("No .modt files found to process.");
    }

    for (const auto& modtFile : loaded.files) {
        if (verbose) std::cout << "Loading: " << modtFile.string() << "\n";
        parser.parseTo(modtFile.string(), loaded.project);
    }

    if (loaded.project.name == "Testing" || loaded.project.name.empty()) loaded.project.name = "Project";
    return loaded;
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
    std::cout << "Usage: modt <input-path> [options]" << std::endl;
    std::cout << "       modt --input <file-or-directory> [options]" << std::endl;
    std::cout << "       modt inspect [file-or-directory]" << std::endl;
    std::cout << "       modt hello-world" << std::endl;
    std::cout << std::endl;
    std::cout << "Pass . as the input path to process the current directory." << std::endl;
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
    std::cout << "  --docs             Print the full documentation to stdout" << std::endl;
    std::cout << "  --inspect          Open a navigable project inspector" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  inspect            Open a navigable project inspector" << std::endl;
    std::cout << "  hello-world        Create a minimal MODT starter project in the current folder" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string version = "1.2.9";
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
    bool showDocs = false;
    bool inspectMode = false;
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
        } else if (arg == "--docs") {
            showDocs = true;
        } else if (arg == "--inspect" || arg == "inspect") {
            inspectMode = true;
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

    if (showDocs) {
        return printDocs();
    }

    if (scaffoldHelloWorld) {
        if (!inputPath.empty() || !outPathArg.empty() || interactive || inspectMode || genPUML || genDomainModel || genSQL || genDocs || genActivity || genSSD || genSequence || genState || !positionalArgs.empty()) {
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
        return 0;
    }

    LoadedProject loaded;
    try {
        loaded = loadProject(inputPath, !inspectMode);
    } catch (const std::exception& exc) {
        std::cerr << "Error: " << exc.what() << "\n";
        return 1;
    }

    Model::Project& project = loaded.project;
    fs::path inputPathObj = loaded.inputPath;

    if (inspectMode) {
        return Inspector::run(project, loaded.files);
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

    auto resolveCssPath = [&](const std::string& configuredPath, const fs::path& outputDir) {
        if (configuredPath.empty()) return fs::path();

        fs::path cssPath(configuredPath);
        if (cssPath.is_absolute() && fs::exists(cssPath)) return cssPath;
        if (cssPath.is_absolute()) return cssPath;

        std::vector<fs::path> candidates;
        candidates.push_back(inputPathObj.parent_path() / cssPath);
        if (fs::is_directory(inputPathObj)) candidates.push_back(inputPathObj / cssPath);
        candidates.push_back(outputDir / cssPath);
        candidates.push_back(fs::current_path() / cssPath);

        for (const auto& candidate : candidates) {
            if (fs::exists(candidate)) return candidate;
        }
        return cssPath;
    };

    auto triggerPdfExport = [&](const fs::path& mdPath) {
        fs::path pdfPath = mdPath;
        pdfPath.replace_extension(".pdf");
        std::string cssToken = std::to_string(std::hash<std::string>{}(fs::absolute(mdPath).string()));

        fs::path cssPath = resolveCssPath(project.documentation.cssPath, mdPath.parent_path());
        fs::path tempCssPath;
        fs::path tempFooterCssPath;
        if (cssPath.empty()) {
            tempCssPath = fs::temp_directory_path() / ("modt-report-" + cssToken + ".css");
            std::ofstream cssFile(tempCssPath);
            if (cssFile.is_open()) {
                cssFile << defaultReportCss(project.documentation.footer);
                cssFile.close();
                cssPath = tempCssPath;
            }
        } else if (!project.documentation.footer.empty()) {
            tempFooterCssPath = fs::temp_directory_path() / ("modt-report-footer-" + cssToken + ".css");
            std::ofstream cssFile(tempFooterCssPath);
            if (cssFile.is_open()) {
                cssFile << "@page {\n"
                        << "  @bottom-left {\n"
                        << "    content: " << cssStringLiteral(project.documentation.footer) << ";\n"
                        << "    font-family: \"Inter\", \"Helvetica Neue\", Arial, sans-serif;\n"
                        << "    font-size: 9pt;\n"
                        << "    color: #555;\n"
                        << "  }\n"
                        << "}\n";
                cssFile.close();
            }
        }

        std::stringstream cmd;
        cmd << "cd " << shellQuote(fs::absolute(mdPath.parent_path()).string())
            << " && pandoc " << shellQuote(mdPath.filename().string())
            << " -o " << shellQuote(fs::absolute(pdfPath).string())
            << " --standalone --number-sections";

        if (!cssPath.empty()) {
            cmd << " --css " << shellQuote(fs::absolute(cssPath).string());
        }
        if (!tempFooterCssPath.empty()) {
            cmd << " --css " << shellQuote(fs::absolute(tempFooterCssPath).string());
        }

        if (commandExists("weasyprint")) {
            cmd << " --pdf-engine=weasyprint";
        } else if (commandExists("wkhtmltopdf")) {
            cmd << " --pdf-engine=wkhtmltopdf";
        }

        std::cout << "Running: " << cmd.str() << "\n";
        int result = std::system(cmd.str().c_str());
        if (result != 0) {
            std::cerr << "Warning: PDF export failed with code " << result << ". Make sure 'pandoc' and a PDF engine are installed. CSS styling requires a CSS-capable engine such as weasyprint or wkhtmltopdf.\n";
        } else {
            std::cout << "Exported PDF: " << pdfPath.string() << "\n";
        }
    };

    auto resolveOutputPath = [&](const std::string& suffix, const std::string& overridePath = "") {
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
        return fullOutPath;
    };

    auto renderedArtifactPath = [](const fs::path& sourcePath, const std::string& format) {
        if (format.empty()) return sourcePath;
        fs::path rendered = sourcePath;
        rendered.replace_extension("." + format);
        return rendered;
    };

    auto makeDocumentationAsset = [](const std::string& title, const fs::path& assetPath, const fs::path& mdPath) {
        Generator::DocumentationAsset asset;
        asset.title = title;

        std::error_code ec;
        fs::path rel = fs::relative(assetPath, mdPath.parent_path(), ec);
        asset.path = ec ? assetPath.string() : rel.string();

        std::string ext = assetPath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        asset.embeddable = ext == ".svg" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif";
        return asset;
    };

    auto saveFile = [&](const std::string& content, const std::string& suffix, const std::string& overridePath = "", const std::string& format = "") {
        if (content.empty()) return fs::path();
        
        fs::path fullOutPath = resolveOutputPath(suffix, overridePath);

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
            return renderedArtifactPath(fullOutPath, format);
        } else {
            std::cerr << "Error: Could not write to " << fullOutPath.string() << "\n";
        }
        return fs::path();
    };

    auto saveMultipleFiles = [&](const std::map<std::string, std::string>& files, const std::string& prefix, const std::string& suffix, const std::string& overrideDir = "", const std::string& format = "") {
        std::vector<fs::path> writtenPaths;
        fs::path baseDir = targetDir;
        if (!overrideDir.empty()) {
            baseDir = fs::path(overrideDir);
            if (baseDir.is_relative()) {
                baseDir = targetDir / baseDir;
            }
        }

        auto safeFileToken = [](std::string value) {
            for (char& c : value) {
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-' && c != '.') {
                    c = '_';
                }
            }
            while (value.find("__") != std::string::npos) {
                value.replace(value.find("__"), 2, "_");
            }
            while (!value.empty() && value.front() == '_') value.erase(value.begin());
            while (!value.empty() && value.back() == '_') value.pop_back();
            return value.empty() ? std::string("output") : value;
        };

        for (const auto& [name, content] : files) {
            if (content.empty()) continue;
            std::string fileName = safeFileToken(name);
            // Always apply prefix and suffix if provided
            if (!suffix.empty()) {
                fileName = safeFileToken(prefix) + "_" + fileName + suffix;
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
                writtenPaths.push_back(renderedArtifactPath(fullOutPath, format));
            }
        }
        return writtenPaths;
    };

    Generator::PumlGenerator pumlGen;
    std::vector<std::pair<std::string, fs::path>> generatedDiagramAssets;
    if (genPUML) {
        fs::path p = saveFile(pumlGen.generateDesignModel(project), ".design.puml", customPaths["design"], customFormats["design"]);
        if (!p.empty()) generatedDiagramAssets.push_back({"Design Model", p});
    }
    if (genDomainModel) {
        fs::path p = saveFile(pumlGen.generateDomainModel(project), ".domain.puml", customPaths["domain"], customFormats["domain"]);
        if (!p.empty()) generatedDiagramAssets.push_back({"Domain Model", p});
    }
    
    if (genActivity && !project.useCases.empty()) {
        fs::path p = saveFile(pumlGen.generateActivityDiagram(project), ".activity.puml", customPaths["activity"], customFormats["activity"]);
        if (!p.empty()) generatedDiagramAssets.push_back({"Activity Diagram", p});
    }

    if (genSSD && !project.useCases.empty()) {
        for (const auto& p : saveMultipleFiles(pumlGen.generateSystemSequenceDiagrams(project), project.name, ".ssd.puml", customPaths["ssd"], customFormats["ssd"])) {
            if (!p.empty()) generatedDiagramAssets.push_back({"System Sequence Diagram", p});
        }
    }

    if (genSequence && !project.useCases.empty()) {
        for (const auto& p : saveMultipleFiles(pumlGen.generateSequenceDiagrams(project), project.name, ".sequence.puml", customPaths["sequence"], customFormats["sequence"])) {
            if (!p.empty()) generatedDiagramAssets.push_back({"Sequence Diagram", p});
        }
    }

    if (genState) {
        for (const auto& p : saveMultipleFiles(pumlGen.generateStateMachineDiagrams(project), project.name, ".state.puml", customPaths["state"], customFormats["state"])) {
            if (!p.empty()) generatedDiagramAssets.push_back({"State Machine Diagram", p});
        }
    }

    if (genSQL) {
        Generator::SqlGenerator sqlGen;
        std::string sql = sqlGen.generate(project);
        saveFile(sql, ".sql", customPaths["sql"]);
    }

    if (genDocs) {
        Generator::DocGenerator docGen;
        fs::path docsPath = resolveOutputPath(".md", customPaths["docs"]);
        std::vector<Generator::DocumentationAsset> docAssets;
        for (const auto& [title, path] : generatedDiagramAssets) {
            if (!path.empty()) docAssets.push_back(makeDocumentationAsset(title, path, docsPath));
        }
        std::string docs = docGen.generate(project, docAssets);
        saveFile(docs, ".md", customPaths["docs"], customFormats["docs"]);
    }

    return 0;
}
