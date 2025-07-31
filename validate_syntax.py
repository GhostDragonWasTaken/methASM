#!/usr/bin/env python3
"""
Simple syntax validation script for C files
Checks for common issues that would prevent compilation
"""

import os
import re
import sys

def check_includes(file_path, content):
    """Check for missing includes"""
    issues = []
    
    # Check for functions that need specific includes
    if 'malloc(' in content or 'free(' in content:
        if '#include <stdlib.h>' not in content:
            issues.append(f"{file_path}: Missing #include <stdlib.h> for malloc/free")
    
    if 'printf(' in content or 'fprintf(' in content or 'snprintf(' in content or 'vsnprintf(' in content:
        if '#include <stdio.h>' not in content:
            issues.append(f"{file_path}: Missing #include <stdio.h> for printf functions")
    
    if 'strcmp(' in content or 'strlen(' in content or 'strcpy(' in content or 'strdup(' in content:
        if '#include <string.h>' not in content:
            issues.append(f"{file_path}: Missing #include <string.h> for string functions")
    
    if 'isalpha(' in content or 'isdigit(' in content or 'isspace(' in content:
        if '#include <ctype.h>' not in content:
            issues.append(f"{file_path}: Missing #include <ctype.h> for character functions")
    
    return issues

def check_braces(file_path, content):
    """Check for balanced braces"""
    issues = []
    brace_count = 0
    line_num = 0
    
    for line in content.split('\n'):
        line_num += 1
        for char in line:
            if char == '{':
                brace_count += 1
            elif char == '}':
                brace_count -= 1
                if brace_count < 0:
                    issues.append(f"{file_path}:{line_num}: Unmatched closing brace")
                    return issues
    
    if brace_count != 0:
        issues.append(f"{file_path}: Unmatched braces (missing {abs(brace_count)} {'opening' if brace_count < 0 else 'closing'} braces)")
    
    return issues

def check_semicolons(file_path, content):
    """Check for missing semicolons (basic check)"""
    issues = []
    lines = content.split('\n')
    
    for i, line in enumerate(lines, 1):
        line = line.strip()
        if not line or line.startswith('//') or line.startswith('/*') or line.startswith('#'):
            continue
        
        # Lines that should end with semicolon
        if (line.endswith(')') and not line.startswith('if') and not line.startswith('while') 
            and not line.startswith('for') and '{' not in line):
            if not line.endswith(';'):
                issues.append(f"{file_path}:{i}: Possible missing semicolon")
    
    return issues

def validate_c_file(file_path):
    """Validate a single C file"""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        return [f"{file_path}: Could not read file - {e}"]
    
    issues = []
    issues.extend(check_includes(file_path, content))
    issues.extend(check_braces(file_path, content))
    # Skip semicolon check for now as it's too basic
    
    return issues

def main():
    """Main validation function"""
    src_dir = 'src'
    if not os.path.exists(src_dir):
        print("Error: src directory not found")
        return 1
    
    all_issues = []
    
    # Find all .c and .h files
    for root, dirs, files in os.walk(src_dir):
        for file in files:
            if file.endswith(('.c', '.h')):
                file_path = os.path.join(root, file)
                issues = validate_c_file(file_path)
                all_issues.extend(issues)
    
    if all_issues:
        print("Syntax validation issues found:")
        for issue in all_issues:
            print(f"  {issue}")
        return 1
    else:
        print("✓ No obvious syntax issues found")
        print("✓ All required includes appear to be present")
        print("✓ Braces appear to be balanced")
        print("\nNote: This is a basic syntax check. Full compilation testing requires a C compiler.")
        return 0

if __name__ == '__main__':
    sys.exit(main())