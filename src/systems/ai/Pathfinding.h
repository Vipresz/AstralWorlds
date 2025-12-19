#pragma once

#include "core/math/DeterministicMath.h"
#include "core/Types.h"
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>

namespace Engine {
namespace Systems {
namespace AI {

using namespace Math;

// Pathfinding node for A* algorithm
struct PathNode {
    Vec3 position;
    uint32_t index;
    float gCost;        // Cost from start
    float hCost;        // Heuristic cost to goal
    float fCost() const { return gCost + hCost; }
    PathNode* parent;
    bool walkable;
    
    PathNode() : index(0), gCost(0), hCost(0), parent(nullptr), walkable(true) {}
};

// Navigation mesh or grid for pathfinding
class NavMesh {
public:
    // Find path from start to goal
    std::vector<Vec3> FindPath(const Vec3& start, const Vec3& goal) {
        // A* pathfinding implementation
        // This is a placeholder - full implementation would:
        // 1. Convert world positions to grid/navmesh indices
        // 2. Run A* algorithm
        // 3. Return path as vector of positions
        
        std::vector<Vec3> path;
        // TODO: Implement A* algorithm
        return path;
    }
    
    // Check if position is walkable
    bool IsWalkable(const Vec3& position) const {
        // TODO: Implement walkability check
        return true;
    }
    
    // Get nearest walkable position
    Vec3 GetNearestWalkable(const Vec3& position) const {
        // TODO: Implement nearest walkable finding
        return position;
    }
};

// Component for entities that can pathfind
struct PathfindingComponent {
    Vec3 targetPosition;
    std::vector<Vec3> currentPath;
    float movementSpeed;
    float stoppingDistance;
    bool hasTarget;
    
    PathfindingComponent() 
        : movementSpeed(5.0f)
        , stoppingDistance(0.5f)
        , hasTarget(false)
    {}
};

// Behavior tree node status
enum class NodeStatus {
    Running,
    Success,
    Failure
};

// Behavior tree context (blackboard)
struct BehaviorContext {
    EntityID entityId;
    float deltaTime;
    uint64_t tickNumber;
    
    // Shared data (AI "memory")
    Vec3 targetPosition;
    bool shouldAttack;
    bool shouldFlee;
    // Add more as needed
};

// Base behavior tree node
class BehaviorNode {
public:
    virtual ~BehaviorNode() = default;
    virtual NodeStatus Execute(BehaviorContext& ctx) = 0;
};

// Composite nodes
class SequenceNode : public BehaviorNode {
public:
    SequenceNode(std::vector<std::unique_ptr<BehaviorNode>> children)
        : m_children(std::move(children))
    {}
    
    NodeStatus Execute(BehaviorContext& ctx) override {
        for (auto& child : m_children) {
            NodeStatus status = child->Execute(ctx);
            if (status != NodeStatus::Success) {
                return status;
            }
        }
        return NodeStatus::Success;
    }
    
private:
    std::vector<std::unique_ptr<BehaviorNode>> m_children;
};

class SelectorNode : public BehaviorNode {
public:
    SelectorNode(std::vector<std::unique_ptr<BehaviorNode>> children)
        : m_children(std::move(children))
    {}
    
    NodeStatus Execute(BehaviorContext& ctx) override {
        for (auto& child : m_children) {
            NodeStatus status = child->Execute(ctx);
            if (status != NodeStatus::Failure) {
                return status;
            }
        }
        return NodeStatus::Failure;
    }
    
private:
    std::vector<std::unique_ptr<BehaviorNode>> m_children;
};

// Leaf action nodes (examples)
class MoveToTargetNode : public BehaviorNode {
public:
    NodeStatus Execute(BehaviorContext& ctx) override {
        // TODO: Implement movement logic
        // Check distance to target
        // Move towards target
        // Return Running until reached, then Success
        return NodeStatus::Running;
    }
};

class AttackTargetNode : public BehaviorNode {
public:
    NodeStatus Execute(BehaviorContext& ctx) override {
        // TODO: Implement attack logic
        return NodeStatus::Success;
    }
};

// Decorator nodes
class RepeatNode : public BehaviorNode {
public:
    RepeatNode(std::unique_ptr<BehaviorNode> child, int32_t times = -1)
        : m_child(std::move(child))
        , m_times(times)
        , m_count(0)
    {}
    
    NodeStatus Execute(BehaviorContext& ctx) override {
        if (m_times > 0 && m_count >= m_times) {
            return NodeStatus::Success;
        }
        
        NodeStatus status = m_child->Execute(ctx);
        if (status == NodeStatus::Success || status == NodeStatus::Failure) {
            m_count++;
            if (m_times < 0) {
                return NodeStatus::Running; // Repeat forever
            }
        }
        return status;
    }
    
private:
    std::unique_ptr<BehaviorNode> m_child;
    int32_t m_times;
    int32_t m_count;
};

// Behavior tree component
struct BehaviorTreeComponent {
    std::unique_ptr<BehaviorNode> root;
    BehaviorContext context;
    
    void Execute() {
        if (root) {
            root->Execute(context);
        }
    }
};

} // namespace AI
} // namespace Systems
} // namespace Engine


