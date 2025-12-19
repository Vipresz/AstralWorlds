#!/usr/bin/env python3
"""
Script to extract RenderUI function from CameraSystemEditor.cpp
and transform it for use in CameraSystemUI.cpp
"""

import re
import sys

def transform_render_ui(content):
    """Transform RenderUI function body to work with editor reference"""
    
    # Replace member variable accesses (m_xxx -> editor.m_xxx)
    # But be careful not to replace in string literals or comments
    patterns = [
        (r'\bm_([a-zA-Z_][a-zA-Z0-9_]*)', r'editor.m_\1'),
        (r'\bUpdatePointCloud\(\)', r'editor.UpdatePointCloud()'),
        (r'\bUpdateCamera\(\)', r'editor.UpdateCamera()'),
        (r'\bUpdateAnimatedCameras\(\)', r'editor.UpdateAnimatedCameras()'),
        (r'\bSaveSafeParameters\(\)', r'editor.SaveSafeParameters()'),
        (r'\bGenerateRemesh\(\)', r'editor.GenerateRemesh()'),
        (r'\bClearTSDF\(\)', r'editor.ClearTSDF()'),
        (r'\bSaveAlgorithmParams\(', r'editor.SaveAlgorithmParams('),
        (r'\bLoadAlgorithmParams\(', r'editor.LoadAlgorithmParams('),
        (r'\bApplyRainbowColorsToObjects\(\)', r'editor.ApplyRainbowColorsToObjects()'),
        (r'\bm_nuklearUI\.', r'm_nuklearUI->'),
        (r'\bm_glfwWindow', r'm_glfwWindow'),
    ]
    
    for pattern, replacement in patterns:
        content = re.sub(pattern, replacement, content)
    
    return content

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: extract_ui.py <input_file>")
        sys.exit(1)
    
    with open(sys.argv[1], 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Find RenderUI function
    match = re.search(r'^void CameraSystemEditor::RenderUI\(\) \{(.*?)^void CameraSystemEditor::Run\(\)', 
                     content, re.MULTILINE | re.DOTALL)
    
    if match:
        func_body = match.group(1)
        transformed = transform_render_ui(func_body)
        print(transformed)
    else:
        print("RenderUI function not found", file=sys.stderr)
        sys.exit(1)

