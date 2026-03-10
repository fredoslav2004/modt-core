import os
import shutil
import subprocess
import platform
import tarfile
import zipfile
import json

# Load global configuration
with open("scripts/package_config.json", "r") as f:
    CONFIG = json.load(f)

APP_NAME = CONFIG["name"]
APP_DISPLAY_NAME = CONFIG["display_name"]
APP_DESC = CONFIG["description"]
APP_LONG_DESC = CONFIG.get("long_description", APP_DESC)
VERSION = CONFIG["version"]
AUTHOR = CONFIG["author"]
EMAIL = CONFIG["email"]
HOMEPAGE = CONFIG["homepage"]

def run_command(command, args=None):
    if args:
        print(f"Running: {' '.join(command if isinstance(command, list) else [command])} {' '.join(args)}")
        result = subprocess.run([command] + args if isinstance(command, str) else command + args)
    else:
        print(f"Running: {command}")
        result = subprocess.run(command, shell=True)
    return result.returncode == 0

def generate_nfpm_yaml(arch="amd64"):
    payload_dir = "dist/linux"
    content = f"""
name: "{APP_NAME}"
arch: "{arch}"
platform: "linux"
version: "{VERSION}"
release: "1"
section: "utils"
priority: "optional"
maintainer: "{AUTHOR} <{EMAIL}>"
description: |
  {APP_DESC}
  {APP_LONG_DESC}
vendor: "{AUTHOR}"
homepage: "{HOMEPAGE}"
license: "{CONFIG['license']}"
contents:
  - src: "{payload_dir}/{APP_NAME}"
    dst: "/usr/bin/{APP_NAME}"
  - src: "{payload_dir}/Documentation.pdf"
    dst: "/usr/share/doc/{APP_NAME}/Documentation.pdf"
  - src: "{payload_dir}/Documentation.html"
    dst: "/usr/share/doc/{APP_NAME}/Documentation.html"
  - src: "{payload_dir}/LICENSE"
    dst: "/usr/share/doc/{APP_NAME}/copyright"
"""
    cfg_path = f"scripts/nfpm_{arch}.yaml"
    with open(cfg_path, "w") as f:
        f.write(content.strip())
    return cfg_path

def build_docs():
    # Update version in Documentation.md if it exists
    if os.path.exists("Documentation.md"):
        print(f"Propagating version {VERSION} to Documentation.md...")
        with open("Documentation.md", "r") as f:
            lines = f.readlines()
        
        with open("Documentation.md", "w") as f:
            for line in lines:
                # Handle subtitle: "Version X.Y.Z"
                if line.strip().startswith('subtitle: "Version'):
                    f.write(f'subtitle: "Version {VERSION}"\n')
                # Handle **Manual Version X.Y.Z**
                elif "Manual Version" in line:
                    f.write(f"**Manual Version {VERSION}**\n")
                # Handle existing logic
                elif line.lower().startswith("version:") or line.lower().startswith("**version:**"):
                    f.write(f"**Version:** {VERSION}\n")
                elif line.startswith("% Version:"): # Pandoc style
                    f.write(f"% Version: {VERSION}\n")
                elif "# Version" in line:
                    f.write(f"# Version {VERSION}\n")
                else:
                    f.write(line)

    # Build Documentation
    print("\n--- Building Documentation ---")
    doc_cmd = "pandoc Documentation.md -o Documentation.html --css scripts/docs.css -s --toc --number-sections --syntax-definition scripts/modt.xml --highlight-style pygments"
    if not run_command(doc_cmd):
        print("Warning: Failed to build HTML documentation.")
    
    doc_pdf_cmd = "pandoc Documentation.md -o Documentation.pdf --template scripts/minimal.tex --pdf-engine=xelatex --toc --number-sections --syntax-definition scripts/modt.xml --highlight-style pygments"
    if not run_command(doc_pdf_cmd):
        print("Warning: Failed to build PDF documentation.")

def package_linux():
    print(f"\n--- Packaging {APP_DISPLAY_NAME} for Linux ---")
    binary = APP_NAME
    # Staging area for specific platform
    staging_dir = "dist/linux"
    release_dir = "dist/releases"
    dist_dir = f"{release_dir}/{APP_NAME}-{VERSION}-linux"
    
    if os.path.exists(staging_dir):
        shutil.rmtree(staging_dir)
    os.makedirs(staging_dir, exist_ok=True)
    os.makedirs(dist_dir, exist_ok=True)

    # Build optimized binary
    build_cmd = f"g++ -O3 -std=c++23 src/main.cpp src/Generator/DocGenerator.cpp src/Generator/PumlGenerator.cpp src/Generator/SqlGenerator.cpp src/Parser/Parser.cpp -o {binary}"
    if run_command(build_cmd):
        shutil.copy(binary, os.path.join(staging_dir, binary))
        shutil.move(binary, os.path.join(dist_dir, binary))
        
        # Add documentation and metadata to both dist and staging
        docs_to_bundle = ["Documentation.md", "Documentation.html", "Documentation.pdf", "README.md", "LICENSE"]
        for doc in docs_to_bundle:
            if os.path.exists(doc):
                shutil.copy(doc, staging_dir)
                shutil.copy(doc, dist_dir)
        
        # Create standard tarball
        pkg_name = f"{release_dir}/{APP_NAME}-{VERSION}-linux.tar.gz"
        with tarfile.open(pkg_name, "w:gz") as tar:
            tar.add(dist_dir, arcname=os.path.basename(dist_dir))
        print(f"Created {pkg_name}")

        # RPM / DEB generation via NFPM
        if shutil.which("nfpm"):
            print("Generating .deb and .rpm via NFPM...")
            deb_cfg = generate_nfpm_yaml("amd64")
            run_command(f"nfpm pkg --config {deb_cfg} --target {release_dir}/{APP_NAME}_{VERSION}_amd64.deb")
            
            rpm_cfg = generate_nfpm_yaml("x86_64")
            run_command(f"nfpm pkg --config {rpm_cfg} --target {release_dir}/{APP_NAME}_{VERSION}_x86_64.rpm")
        
        # AppImage creation
        appimagetool = os.path.expanduser("~/Downloads/appimagetool-x86_64.AppImage")
        if os.path.exists(appimagetool):
            print("Generating AppImage...")
            appdir = "dist/AppDir"
            if os.path.exists(appdir): shutil.rmtree(appdir)
            os.makedirs(f"{appdir}/usr/bin", exist_ok=True)
            shutil.copy(f"{staging_dir}/{binary}", f"{appdir}/usr/bin/{binary}")
            # Mocking icon and desktop file for basic CLI AppImage
            with open(f"{appdir}/{APP_NAME}.desktop", "w") as f:
                f.write(f"[Desktop Entry]\nName={APP_DISPLAY_NAME}\nExec={binary}\nIcon={APP_NAME}\nType=Application\nCategories=Utility;\nTerminal=true")
            with open(f"{appdir}/AppRun", "w") as f:
                f.write(f"#!/bin/sh\nself=\"$(dirname \"$(readlink -f \"$0\")\")\"\nexec \"$self/usr/bin/{binary}\" \"$@\"")
            run_command(f"chmod +x {appdir}/AppRun")
            run_command(f"touch {appdir}/{APP_NAME}.png") # Placeholder
            
            # Use the local appimagetool from Downloads
            run_command(f"ARCH=x86_64 {appimagetool} {appdir} {release_dir}/{APP_NAME}-{VERSION}-x86_64.AppImage")
        else:
            print(f"Skipping AppImage creation: {appimagetool} not found.")
    else:
        print("Linux build failed.")

def package_windows():
    print(f"\n--- Packaging {APP_DISPLAY_NAME} for Windows ---")
    binary = f"{APP_NAME}.exe"
    # Staging area for specific platform
    staging_dir = "dist/windows"
    release_dir = "dist/releases"
    dist_dir = f"{release_dir}/{APP_NAME}-{VERSION}-windows"
    
    if os.path.exists(staging_dir):
        shutil.rmtree(staging_dir)
    os.makedirs(staging_dir, exist_ok=True)
    os.makedirs(dist_dir, exist_ok=True)

    compiler = "x86_64-w64-mingw32-g++" if platform.system() != "Windows" else "g++"
    if not shutil.which(compiler):
        print(f"Warning: {compiler} not found. Skipping Windows packaging.")
        return

    # Build optimized binary
    build_cmd = f"{compiler} -O3 -std=c++23 src/main.cpp src/Generator/DocGenerator.cpp src/Generator/PumlGenerator.cpp src/Generator/SqlGenerator.cpp src/Parser/Parser.cpp -o {binary}"
    if run_command(build_cmd):
        shutil.copy(binary, os.path.join(staging_dir, binary))
        shutil.move(binary, os.path.join(dist_dir, binary))
        
        # Add documentation and metadata to both dist and staging
        docs_to_bundle = ["Documentation.md", "Documentation.html", "Documentation.pdf", "README.md", "LICENSE"]
        for doc in docs_to_bundle:
            if os.path.exists(doc):
                shutil.copy(doc, staging_dir)
                shutil.copy(doc, dist_dir)
        
        # Create standard zip
        pkg_name = f"{release_dir}/{APP_NAME}-{VERSION}-windows.zip"
        with zipfile.ZipFile(pkg_name, 'w', zipfile.ZIP_DEFLATED) as zipf:
            for root, dirs, files in os.walk(dist_dir):
                for file in files:
                    zipf.write(os.path.join(root, file), 
                               os.path.relpath(os.path.join(root, file), os.path.join(dist_dir, '..')))
        print(f"Created {pkg_name}")

        # Try NSIS
        if shutil.which("makensis"):
            print("Creating Windows installer wizard...")
            nsis_args = [
                f"-DAPPNAME={APP_DISPLAY_NAME}",
                f"-DNAME={APP_NAME}",
                f"-DDESCRIPTION={APP_DESC}",
                f"-DVERSION={VERSION}",
                f"-DPRODUCT_VERSION={VERSION}",
                f"-DOUTFILE=../dist/releases/{APP_NAME}-{VERSION}-setup.exe",
                "scripts/modt.nsi"
            ]
            run_command("makensis", nsis_args)
        else:
            print("NSIS (makensis) not found. Skipping Windows installer.")
    else:
        print("Windows build failed.")

def main():
    # Ensure dist/releases exists and is clean
    if os.path.exists("dist"):
        shutil.rmtree("dist")
    os.makedirs("dist/releases", exist_ok=True)
    
    # Build common docs first
    build_docs()
    
    if platform.system() == "Linux":
        package_linux()
        package_windows()
    elif platform.system() == "Windows":
        package_windows()
    else:
        print(f"Unsupported platform: {platform.system()}")
    
    print(f"\nPackaging complete. Check 'dist/releases' for artifacts.")

if __name__ == "__main__":
    main()
