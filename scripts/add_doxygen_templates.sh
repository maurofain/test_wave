#!/usr/bin/env bash
# insert a doxygen skeleton before each function definition that lacks one

find . -name '*.c' -type f | while read -r file; do
    awk '
    BEGIN { prev=""; }
    # match a typical function definition at start of line (return type + name)
    /^[A-Za-z_][A-Za-z0-9_ \t\*]*\([^)]+\)[ \t]*\{/ {
        if (prev !~ "/\*\*") {
            print "/**";
            print " * @brief TODO: describe this function";
            print " *";
            print " * @param [in]  <param> description";
            print " * @param [out] <param> description";
            print " * @return <description>";
            print " */";
        }
    }
    { print; prev=$0; }
    ' "$file" > "$file.new" && mv "$file.new" "$file"
done
