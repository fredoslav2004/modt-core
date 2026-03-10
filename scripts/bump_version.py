import json
import sys
import os

def bump(new_version):
    config_path = "scripts/package_config.json"
    if not os.path.exists(config_path):
        print(f"Error: {config_path} not found.")
        return False
    
    try:
        with open(config_path, 'r') as f:
            config = json.load(f)
        
        old_version = config.get('version', 'unknown')
        config['version'] = new_version
        
        with open(config_path, 'w') as f:
            json.dump(config, f, indent=4)

        # Update src/main.cpp
        main_cpp = "src/main.cpp"
        if os.path.exists(main_cpp):
            with open(main_cpp, "r") as f:
                lines = f.readlines()
            
            with open(main_cpp, "w") as f:
                for line in lines:
                    if 'std::string version = "' in line:
                        # Replace the version string
                        import re
                        line = re.sub(r'(std::string version = ")[^"]+(";)', r'\g<1>' + new_version + r'\g<2>', line)
                    f.write(line)
            print(f"Updated {main_cpp}")
        
        # Update scripts/minimal.tex
        tex_file = "scripts/minimal.tex"
        if os.path.exists(tex_file):
            with open(tex_file, "r") as f:
                content = f.read()
            
            import re
            new_content = re.sub(r'(\\fancyhead\[L\]\{MODT Documentation v)[^}]+\}', r'\g<1>' + new_version + '}', content)
            
            with open(tex_file, "w") as f:
                f.write(new_content)
            print(f"Updated {tex_file}")

        print(f"Successfully bumped version: {old_version} -> {new_version}")
        return True
    except Exception as e:
        print(f"Error updating config: {e}")
        return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python bump_version.py <new_version>")
        sys.exit(1)
    
    if bump(sys.argv[1]):
        sys.exit(0)
    else:
        sys.exit(1)
