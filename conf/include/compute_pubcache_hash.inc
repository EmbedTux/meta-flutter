
inherit python3native

DEPENDS += " \
    python3-pyyaml-native \
"
# DEPENDS_class-native += "pubcache-tools-native"
PYTHONUNBUFFERED = "1"


# ------------------------------------------------------------------
# Utility: Load YAML safely
# ------------------------------------------------------------------
def load_yaml(d, path):
    import subprocess, ast

    python = d.getVar('PYTHON')  # python3-native interpreter
    cmd = (
        "import yaml\n"
        f"with open('{path}', 'r', encoding='utf-8') as f:\n"
        "    res = yaml.safe_load(f) or {}\n"
        "print(res)\n"
    )

    yaml_data = subprocess.check_output(
        [python, "-c", cmd],
        text=True
    )

    print("yaml_data type : ")
    print(type(yaml_data))
    print(f"yaml_data: {yaml_data}")
    return ast.literal_eval(yaml_data)

# ------------------------------------------------------------------
# Extract only the dependency-related parts of a pubspec.yaml
# ------------------------------------------------------------------
def extract_dependency_info(d, pubspec_path, visited_paths=None):
    import os

    if visited_paths is None:
        visited_paths = set()

    pubspec_path = os.path.abspath(pubspec_path)

    # Prevent infinite recursion from circular path dependencies
    if pubspec_path in visited_paths:
        return None
    visited_paths.add(pubspec_path)

    data = load_yaml(d, pubspec_path)

    result = {
        "dependencies": {},
        "dev_dependencies": {},
        "dependency_overrides": {},
        "environment": {},
        "path_dependencies": {},
    }

    # Copy relevant sections if present
    for key in ["dependencies", "dev_dependencies", "dependency_overrides", "environment"]:
        if key in data:
            # Only keep content that affects dependency resolution
            result[key] = data[key]

    # ------------------------------------------------------------------
    # Handle path dependencies (recurse into their pubspec.yaml)
    # ------------------------------------------------------------------
    def handle_dep_section(section):
        deps = data.get(section, {})
        for name, spec in deps.items():
            if isinstance(spec, dict) and "path" in spec:
                path = spec["path"]
                path_abs = os.path.abspath(os.path.join(os.path.dirname(pubspec_path), path))
                sub_pubspec = os.path.join(path_abs, "pubspec.yaml")

                if os.path.exists(sub_pubspec):
                    # Recursively extract dependency info from path dependency
                    sub_info = extract_dependency_info(sub_pubspec, visited_paths)
                    result["path_dependencies"][name] = sub_info
                else:
                    # Path exists but no pubspec.yaml (unlikely but handle gracefully)
                    result["path_dependencies"][name] = {"missing_pubspec": path_abs}

    handle_dep_section("dependencies")
    handle_dep_section("dev_dependencies")
    handle_dep_section("dependency_overrides")

    return result

# ------------------------------------------------------------------
# Normalize JSON for deterministic hashing
# ------------------------------------------------------------------
def canonical_json(data):
    import json
    return json.dumps(data, sort_keys=True, separators=(",", ":"))

# ------------------------------------------------------------------
# Compute SHA256
# ------------------------------------------------------------------
def sha256(s):
    import hashlib
    return hashlib.sha256(s.encode("utf-8")).hexdigest()

# ------------------------------------------------------------------
# Main
# ------------------------------------------------------------------
def compute_pubcache_hash(d, pubspec_path, flutter_version, platform):
    import os

    if not os.path.exists(pubspec_path):
        print(f"Error: pubspec.yaml not found at {pubspec_path}")
        raise FileNotFoundError

    deps_info = extract_dependency_info(d, pubspec_path)

    full_info = {
        "flutter_version": flutter_version,
        "platform": platform,
        "inputs": deps_info,
    }

    json_text = canonical_json(full_info)
    digest = sha256(json_text)

    return digest
