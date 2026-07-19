import os
import re

ROOT = "D:/Yuki_1.0"
SRC_DIR = os.path.join(ROOT, "src")
INC_DIR = os.path.join(ROOT, "include")
REPORT_PATH = os.path.join(ROOT, "system_report.md")

out = open(REPORT_PATH, "w", encoding="utf-8")
def write(s): out.write(s + "\n")

write("# System Integration Report\n")

# 1. DIRECTORY TREE
write("### 1. DIRECTORY TREE\n")
write("```")
for d in [SRC_DIR, INC_DIR]:
    if not os.path.exists(d): continue
    for root, dirs, files in os.walk(d):
        if "vendor" in root: continue
        rel = os.path.relpath(root, ROOT)
        depth = rel.count(os.sep)
        indent = "  " * depth
        write(f"{indent}- {os.path.basename(root)}/ ({len(files)} files)")
write("```\n")

# 2. CORE COMPONENT INVENTORY
write("### 2. CORE COMPONENT INVENTORY\n")
components = [
    "SignalConditioningLayer", "ObservationEncoder", "TextEncoder", "AudioEncoder", "VisualEncoder",
    "MultiModalFusionGate", "VariationalStateEstimator", "PrecisionEngine", "GenerativeModel", "BeliefState",
    "FreeEnergyCalculator", "PolicySelector", "BabyMode", "TurnCoordinator", "PredictiveTurnEngine",
    "KnowledgeDaemon", "Executor", "CameraRuntime", "ScreenRuntime", "STT", "TTS", "InternetAgency",
    "SelfCodeEngine", "CognitiveGraph", "OmegaEngine", "ProactiveEngine", "InquisitiveEngine", "SystemCortex",
    "AutoCurriculum", "BackgroundLearningEngine", "ControlPlane", "GlobalWorkspace", "StatePlane",
    "SecuritySandbox", "EthicalConstraintEngine", "ActiveInferenceCore"
]

all_files = []
for root, _, files in os.walk(SRC_DIR):
    if "vendor" in root: continue
    for f in files: all_files.append(os.path.join(root, f))
if os.path.exists(INC_DIR):
    for root, _, files in os.walk(INC_DIR):
        if "vendor" in root: continue
        for f in files: all_files.append(os.path.join(root, f))

# interface hubs tracking
hubs = {}

for comp in components:
    header_path = None
    source_path = None
    
    for f in all_files:
        bn = os.path.basename(f)
        if bn.lower() == (comp.lower() + ".h"): header_path = f
        if bn.lower() == (comp.lower() + ".cpp"): source_path = f
        
    if not header_path:
        for f in all_files:
            if f.endswith(".h"):
                try:
                    content = open(f, encoding="utf-8").read()
                    if f"class {comp}" in content or f"struct {comp}" in content:
                        header_path = f
                        break
                except: pass

    if not source_path:
        for f in all_files:
            if f.endswith(".cpp"):
                try:
                    content = open(f, encoding="utf-8").read()
                    if f"{comp}::" in content:
                        source_path = f
                        break
                except: pass

    if not header_path and not source_path:
        write(f"#### {comp}\n**NOT FOUND**\n")
        continue

    write(f"#### {comp}\n**FOUND**")
    if header_path: write(f"- **Header**: `{os.path.relpath(header_path, ROOT).replace(os.sep, '/')}`")
    if source_path: write(f"- **Source**: `{os.path.relpath(source_path, ROOT).replace(os.sep, '/')}`")
    
    if header_path:
        content = open(header_path, encoding="utf-8").read()
        lines = content.split('\n')
        
        includes = [l.strip() for l in lines if l.strip().startswith("#include")]
        write("- **Includes**:\n  ```cpp\n" + ("\n".join(includes) if includes else "  // none") + "\n  ```")
        
        public_methods = []
        private_pointers = []
        in_public = False
        in_class = False
        class_name = ""
        pointers_count = 0
        
        for i, line in enumerate(lines):
            line_strip = line.strip()
            if "class " in line or "struct " in line:
                m = re.search(r'(?:class|struct)\s+(\w+)', line)
                if m: 
                    class_name = m.group(1)
                    if class_name == comp:
                        in_class = True
                        in_public = ("struct" in line)
            if in_class:
                if line_strip == "public:": in_public = True
                elif line_strip == "private:" or line_strip == "protected:": in_public = False
                elif line_strip == "};": 
                    in_class = False
                    if pointers_count >= 3:
                        hubs[class_name] = private_pointers
                
                if in_public and "(" in line and ")" in line and not line_strip.startswith("//"):
                    if not any(kw in line for kw in ["=", "if", "for", "while", "switch"]):
                        public_methods.append(line_strip.rstrip('; {'))
                
                if not in_public and ("std::shared_ptr" in line or "std::unique_ptr" in line or "*" in line):
                    if not line_strip.startswith("//") and not "(" in line:
                        if "*" in line and not ("shared_ptr" in line or "unique_ptr" in line):
                            if re.search(r'\w+\s*\*\s*\w+', line):
                                private_pointers.append(line_strip)
                                pointers_count += 1
                        else:
                            private_pointers.append(line_strip)
                            pointers_count += 1

        write("- **Public Methods**:\n  ```cpp\n" + ("\n".join(public_methods) if public_methods else "  // none") + "\n  ```")
        write("- **Private Pointers**:\n  ```cpp\n" + ("\n".join(private_pointers) if private_pointers else "  // none") + "\n  ```")
    write("\n")

# 3. BUILD SYSTEM
write("### 3. BUILD SYSTEM\n")
cmake_path = os.path.join(ROOT, "CMakeLists.txt")
if os.path.exists(cmake_path):
    write("```cmake\n" + open(cmake_path, encoding="utf-8").read() + "\n```\n")
else: write("**NOT FOUND**\n")

# 4. MAIN INITIALIZATION ORDER
write("### 4. MAIN INITIALIZATION ORDER\n")
main_path = os.path.join(SRC_DIR, "main.cpp")
if os.path.exists(main_path):
    write("```cpp\n")
    content = open(main_path, encoding="utf-8").read()
    for line in content.split('\n'):
        line_strip = line.strip()
        if "make_shared" in line_strip or "make_unique" in line_strip or ".init(" in line_strip or ".start(" in line_strip or ".run(" in line_strip or re.match(r'^[\w:]+\s+\w+\(.*?\);', line_strip) or re.match(r'^[\w:]+\s+\w+;', line_strip):
            if not line_strip.startswith("//") and not line_strip.startswith("return"):
                write(line_strip)
    write("```\n")

# 5. TEST INVENTORY
write("### 5. TEST INVENTORY\n")
tests = [f for f in all_files if "test" in f.lower() and f.endswith(".cpp")]
if tests:
    for t in tests: write(f"- `{os.path.relpath(t, ROOT).replace(os.sep, '/')}`")
else: write("**NOT FOUND**\n")

# 6. DATA & CONFIG FILES
write("\n### 6. DATA & CONFIG FILES\n")
write("```")
for root, _, files in os.walk(ROOT):
    rel = os.path.relpath(root, ROOT)
    if any(p in rel.lower() for p in ["data", "config", "assets"]) or root == ROOT:
        for f in files:
            if rel == "." and not f.endswith((".json", ".yaml", ".xml", ".ini", ".txt")): continue
            if "build" in rel.lower() or ".git" in rel.lower(): continue
            write(f"{rel.replace(os.sep, '/')}/{f}")
write("```\n")

# 7. TODOS
write("### 7. ALL TODO / FIXME / STUB / HACK COMMENTS\n")
keywords = ["TODO", "FIXME", "STUB", "HACK", "XXX", "PLACEHOLDER", "TEMP"]
for f in all_files:
    try:
        lines = open(f, encoding="utf-8").read().split('\n')
        for i, line in enumerate(lines):
            if any(k in line for k in keywords):
                write(f"**`{os.path.relpath(f, ROOT).replace(os.sep, '/')}`: L{i+1}**")
                write("```cpp")
                for j in range(max(0, i-3), min(len(lines), i+4)):
                    write(f"{j+1:4d} | {lines[j]}")
                write("```\n")
    except: pass

# 8. INTERFACE HUBS
write("### 8. INTERFACE HUBS\n")
if hubs:
    for cls, ptrs in hubs.items():
        write(f"#### {cls}\n```cpp\n" + "\n".join(ptrs) + "\n```\n")
else: write("None found with 3+ pointers.\n")

# 9. EXISTING MEMORY / STORAGE CODE
write("### 9. EXISTING MEMORY / STORAGE CODE\n")
for f in all_files:
    try:
        lines = open(f, encoding="utf-8").read().split('\n')
        for i, line in enumerate(lines):
            if any(k in line.lower() for k in ["fstream", "sqlite3", "save(", "load(", ".csv", ".json"]):
                if line.strip().startswith("//"): continue
                write(f"- `{os.path.relpath(f, ROOT).replace(os.sep, '/')}`: L{i+1}: `{line.strip()}`")
    except: pass

# 10. THREADING MODEL
write("\n### 10. THREADING MODEL\n")
for f in all_files:
    try:
        lines = open(f, encoding="utf-8").read().split('\n')
        for i, line in enumerate(lines):
            if any(k in line for k in ["std::thread", "std::async", "std::mutex", "std::lock_guard", "std::unique_lock", "std::condition_variable", "std::atomic"]):
                if line.strip().startswith("//"): continue
                write(f"- `{os.path.relpath(f, ROOT).replace(os.sep, '/')}`: L{i+1}: `{line.strip()}`")
    except: pass

out.close()
