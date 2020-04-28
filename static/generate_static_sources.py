import sys
import argparse
import os

function_c_format = """
    const char* {variable_name}(size_t* out_size) {{
        static const char _{variable_name}[] = {variable_value};
        *out_size = {variable_size};
        return _{variable_name};
    }}
"""

accessor_h_format = """
#ifndef {accessor_name}_H
#define {accessor_name}_H
namespace {namespace} {{
    const char* accessor(const char* filename, size_t* out_size);
}}
#endif
"""

accessor_cpp_format = """
namespace {namespace} {{

    constexpr unsigned long djb2_hash(const char* str) {{
        return str[0] == 0 ? 0 : str[0] + (5381 * djb2_hash(str + 1));
    }}

    {functions}

    const char* accessor(const char* filename, size_t* out_size) {{
        unsigned long hash = djb2_hash(filename);
        switch(hash) {{
            {cases}
            default:
                *out_size = 0;
                return 0;
        }}
    }}
}}
"""

accessor_case_format = """
            case djb2_hash("{filename}"):
                return {variable_name}(out_size);
"""

def main():
    parser = argparse.ArgumentParser(description="Generate C files for any arbitrary file")
    parser.add_argument("--input_files", dest="input_files", nargs="+", type=str, help="Path to input files")
    parser.add_argument("--out-c-dir", dest="out_c_dir", type=str)
    parser.add_argument("--out-h-dir", dest="out_h_dir", type=str)
    parser.add_argument("--accessor-name", dest="cpp_accessor_name", type=str, default="accessor")
    parser.add_argument("--namespace", dest="namespace", type=str, default="StaticResource")
    args = parser.parse_args()

    out_c_dir = args.out_c_dir
    out_h_dir = args.out_h_dir

    functions = []
    cases = []
    for input_file in args.input_files:
        chars = ""
        with open(input_file, "rb") as f:
            chars = f.read().hex()
        chars = [chars[i:i+2] for i in range(0, len(chars), 2)]
        chars = [f"0x{char}" for char in chars]

        split_in_file_path = os.path.split(input_file)
        filename = split_in_file_path[len(split_in_file_path) - 1]

        var_name = filename.replace(".", "_").upper()

        functions.append(function_c_format.format(variable_name=var_name, variable_value="{ " + ", ".join(chars) + "}", variable_size=len(chars)))
        cases.append(accessor_case_format.format(variable_name=var_name, filename=filename))

    cpp_accessor_path = os.path.join(out_c_dir, args.cpp_accessor_name + ".cpp")
    h_accessor_path = os.path.join(out_h_dir, args.cpp_accessor_name + ".h")
    with open(cpp_accessor_path, "w") as out_c_file:
        out_c_file.write(accessor_cpp_format.format(namespace=args.namespace, functions="\n".join(functions), cases="\n".join(cases)))
    with open(h_accessor_path, "w") as out_h_file:
        out_h_file.write(accessor_h_format.format(namespace=args.namespace, accessor_name=args.cpp_accessor_name))

if __name__ == "__main__":
    main()