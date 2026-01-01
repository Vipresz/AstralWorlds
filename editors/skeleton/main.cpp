#include "SkeletonEditor.h"
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>

using namespace Engine::Editors::Skeleton;

void PrintHelp() {
    std::cout << "\n=== Skeleton Editor Commands ===\n\n";
    std::cout << "  new [name]              - Create a new skeleton\n";
    std::cout << "  load <file>             - Load skeleton from file\n";
    std::cout << "  save [file]             - Save skeleton to file\n";
    std::cout << "  list                    - List all bones\n";
    std::cout << "  tree                    - Show bone hierarchy tree\n";
    std::cout << "  add <name> [parent]     - Add a bone\n";
    std::cout << "  remove <name>           - Remove a bone\n";
    std::cout << "  rename <old> <new>      - Rename a bone\n";
    std::cout << "  parent <bone> <parent>  - Set bone parent\n";
    std::cout << "  pos <bone> <x> <y> <z>  - Set bone position\n";
    std::cout << "  rot <bone> <p> <y> <r>  - Set bone rotation (euler)\n";
    std::cout << "  scale <bone> <x> <y> <z> - Set bone scale\n";
    std::cout << "  info <bone>             - Show bone information\n";
    std::cout << "  validate                - Validate skeleton\n";
    std::cout << "  humanoid                - Create basic humanoid skeleton\n";
    std::cout << "  quadruped               - Create basic quadruped skeleton\n";
    std::cout << "  help                    - Show this help\n";
    std::cout << "  quit/exit               - Exit editor\n";
    std::cout << "\n";
}

void PrintBoneTree(const SkeletonEditor& editor, const std::string& boneName, int indent = 0) {
    std::string indentStr(indent * 2, ' ');
    std::cout << indentStr << "└─ " << boneName;
    
    const BoneDefinition* bone = editor.GetSkeleton()->GetBone(boneName);
    if (bone) {
        std::cout << " (ID: " << bone->id << ")";
        std::cout << " [pos: (" << std::fixed << std::setprecision(2)
                  << bone->position.x << ", " << bone->position.y << ", " << bone->position.z << ")]";
    }
    std::cout << "\n";
    
    auto children = editor.GetBoneChildren(boneName);
    for (const auto& child : children) {
        PrintBoneTree(editor, child, indent + 1);
    }
}

void PrintBoneInfo(const SkeletonEditor& editor, const std::string& boneName) {
    const BoneDefinition* bone = editor.GetSkeleton()->GetBone(boneName);
    if (!bone) {
        std::cout << "Bone '" << boneName << "' not found.\n";
        return;
    }
    
    std::cout << "\n=== Bone: " << bone->name << " ===\n";
    std::cout << "  ID: " << bone->id << "\n";
    std::cout << "  Parent: " << (bone->parentName.empty() ? "(root)" : bone->parentName) << "\n";
    std::cout << "  Position: (" << std::fixed << std::setprecision(4)
              << bone->position.x << ", " << bone->position.y << ", " << bone->position.z << ")\n";
    std::cout << "  Rotation: (" << bone->rotation.x << ", " << bone->rotation.y 
              << ", " << bone->rotation.z << ", " << bone->rotation.w << ")\n";
    std::cout << "  Scale: (" << bone->scale.x << ", " << bone->scale.y << ", " << bone->scale.z << ")\n";
    
    auto children = editor.GetBoneChildren(boneName);
    if (!children.empty()) {
        std::cout << "  Children: ";
        for (size_t i = 0; i < children.size(); i++) {
            std::cout << children[i];
            if (i < children.size() - 1) std::cout << ", ";
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

std::vector<std::string> ParseCommand(const std::string& line) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream iss(line);
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

int main(int argc, char* argv[]) {
    std::cout << "=== Skeleton Editor ===\n";
    std::cout << "Type 'help' for commands, 'quit' to exit\n\n";
    
    SkeletonEditor editor;
    std::string line;
    
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            break;
        }
        
        if (line.empty()) {
            continue;
        }
        
        std::vector<std::string> tokens = ParseCommand(line);
        if (tokens.empty()) {
            continue;
        }
        
        std::string command = tokens[0];
        
        if (command == "quit" || command == "exit") {
            if (editor.HasUnsavedChanges()) {
                std::cout << "Warning: You have unsaved changes. Are you sure? (y/n): ";
                std::string confirm;
                std::getline(std::cin, confirm);
                if (confirm != "y" && confirm != "Y") {
                    continue;
                }
            }
            break;
        }
        else if (command == "help") {
            PrintHelp();
        }
        else if (command == "new") {
            std::string name = tokens.size() > 1 ? tokens[1] : "Untitled Skeleton";
            editor.NewSkeleton(name);
            std::cout << "Created new skeleton: " << name << "\n";
        }
        else if (command == "load") {
            if (tokens.size() < 2) {
                std::cout << "Usage: load <file>\n";
                continue;
            }
            if (editor.LoadSkeleton(tokens[1])) {
                std::cout << "Loaded skeleton: " << editor.GetSkeleton()->GetName() << "\n";
            } else {
                std::cout << "Failed to load skeleton from: " << tokens[1] << "\n";
            }
        }
        else if (command == "save") {
            std::string file = tokens.size() > 1 ? tokens[1] : "skeleton.skel";
            if (editor.SaveSkeleton(file)) {
                std::cout << "Saved skeleton to: " << file << "\n";
            } else {
                std::cout << "Failed to save skeleton to: " << file << "\n";
            }
        }
        else if (command == "list") {
            auto bones = editor.GetAllBoneNames();
            std::cout << "\nBones (" << bones.size() << "):\n";
            for (const auto& name : bones) {
                const BoneDefinition* bone = editor.GetSkeleton()->GetBone(name);
                std::cout << "  " << name << " (ID: " << (bone ? bone->id : -1) << ")\n";
            }
            std::cout << "\n";
        }
        else if (command == "tree") {
            auto roots = editor.GetRootBones();
            if (roots.empty()) {
                std::cout << "No bones in skeleton.\n";
            } else {
                std::cout << "\nBone Hierarchy:\n";
                for (const auto& root : roots) {
                    PrintBoneTree(editor, root);
                }
                std::cout << "\n";
            }
        }
        else if (command == "add") {
            if (tokens.size() < 2) {
                std::cout << "Usage: add <name> [parent]\n";
                continue;
            }
            std::string parent = tokens.size() > 2 ? tokens[2] : "";
            if (editor.AddBone(tokens[1], parent)) {
                std::cout << "Added bone: " << tokens[1] << "\n";
            } else {
                std::cout << "Failed to add bone: " << tokens[1] << "\n";
            }
        }
        else if (command == "remove") {
            if (tokens.size() < 2) {
                std::cout << "Usage: remove <name>\n";
                continue;
            }
            if (editor.RemoveBone(tokens[1])) {
                std::cout << "Removed bone: " << tokens[1] << "\n";
            } else {
                std::cout << "Failed to remove bone: " << tokens[1] << "\n";
            }
        }
        else if (command == "rename") {
            if (tokens.size() < 3) {
                std::cout << "Usage: rename <old> <new>\n";
                continue;
            }
            if (editor.RenameBone(tokens[1], tokens[2])) {
                std::cout << "Renamed bone: " << tokens[1] << " -> " << tokens[2] << "\n";
            } else {
                std::cout << "Failed to rename bone\n";
            }
        }
        else if (command == "parent") {
            if (tokens.size() < 3) {
                std::cout << "Usage: parent <bone> <parent>\n";
                continue;
            }
            std::string parent = tokens[2] == "none" ? "" : tokens[2];
            if (editor.SetBoneParent(tokens[1], parent)) {
                std::cout << "Set parent of " << tokens[1] << " to " 
                          << (parent.empty() ? "(root)" : parent) << "\n";
            } else {
                std::cout << "Failed to set parent\n";
            }
        }
        else if (command == "pos") {
            if (tokens.size() < 5) {
                std::cout << "Usage: pos <bone> <x> <y> <z>\n";
                continue;
            }
            try {
                float x = std::stof(tokens[2]);
                float y = std::stof(tokens[3]);
                float z = std::stof(tokens[4]);
                editor.SetBonePosition(tokens[1], x, y, z);
                std::cout << "Set position of " << tokens[1] << " to (" << x << ", " << y << ", " << z << ")\n";
            } catch (...) {
                std::cout << "Invalid position values\n";
            }
        }
        else if (command == "rot") {
            if (tokens.size() < 5) {
                std::cout << "Usage: rot <bone> <pitch> <yaw> <roll>\n";
                continue;
            }
            try {
                float p = std::stof(tokens[2]);
                float y = std::stof(tokens[3]);
                float r = std::stof(tokens[4]);
                editor.SetBoneRotationEuler(tokens[1], p, y, r);
                std::cout << "Set rotation of " << tokens[1] << " to (" << p << ", " << y << ", " << r << ")\n";
            } catch (...) {
                std::cout << "Invalid rotation values\n";
            }
        }
        else if (command == "scale") {
            if (tokens.size() < 5) {
                std::cout << "Usage: scale <bone> <x> <y> <z>\n";
                continue;
            }
            try {
                float x = std::stof(tokens[2]);
                float y = std::stof(tokens[3]);
                float z = std::stof(tokens[4]);
                editor.SetBoneScale(tokens[1], x, y, z);
                std::cout << "Set scale of " << tokens[1] << " to (" << x << ", " << y << ", " << z << ")\n";
            } catch (...) {
                std::cout << "Invalid scale values\n";
            }
        }
        else if (command == "info") {
            if (tokens.size() < 2) {
                std::cout << "Usage: info <bone>\n";
                continue;
            }
            PrintBoneInfo(editor, tokens[1]);
        }
        else if (command == "validate") {
            if (editor.Validate()) {
                std::cout << "Skeleton is valid.\n";
            } else {
                std::cout << "Skeleton has errors:\n";
                auto errors = editor.GetValidationErrors();
                for (const auto& error : errors) {
                    std::cout << "  - " << error << "\n";
                }
            }
        }
        else if (command == "humanoid") {
            editor.CreateHumanoidSkeleton();
            std::cout << "Created humanoid skeleton template.\n";
        }
        else if (command == "quadruped") {
            editor.CreateQuadrupedSkeleton();
            std::cout << "Created quadruped skeleton template.\n";
        }
        else {
            std::cout << "Unknown command: " << command << ". Type 'help' for commands.\n";
        }
    }
    
    std::cout << "Goodbye!\n";
    return 0;
}

