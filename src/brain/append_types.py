import os
path = r"d:\Yuki_1.0\src\brain\MeaningTypes.h"
content = open(path, "r", encoding="utf-8").read()
if "struct FactBundle" not in content:
    with open(path, "a", encoding="utf-8") as f:
        f.write("\nstruct FactBundle {\n    std::string summary;\n    std::vector<std::string> sources;\n};\n")
