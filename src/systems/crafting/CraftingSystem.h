#pragma once

#include "systems/inventory/InventoryComponent.h"
#include "core/System.h"
#include "core/ecs/Registry.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace Engine {
namespace Systems {
namespace Crafting {

using ItemID = uint32_t;

// Requirement for a recipe
struct ItemRequirement {
    ItemID itemId;
    uint32_t quantity;
};

// Output for a recipe
struct ItemOutput {
    ItemID itemId;
    uint32_t quantity;
    float chance; // 0.0 - 1.0 probability of getting this output
};

// Crafting recipe definition
struct CraftingRecipe {
    uint32_t recipeId;
    std::vector<ItemRequirement> inputs;
    std::vector<ItemOutput> outputs;
    float craftTime;          // Time in seconds
    uint32_t requiredSkill;   // Skill level required
    ItemID requiredTool;      // Optional tool requirement (0 = none)
    
    bool CanCraft(const Inventory::InventoryComponent& inventory) const {
        // Check if player has all required items
        for (const auto& req : inputs) {
            if (inventory.GetItemCount(req.itemId) < req.quantity) {
                return false;
            }
        }
        return true;
    }
};

// Active crafting job
struct CraftingJob {
    uint32_t recipeId;
    EntityID crafterId;
    float timeRemaining;
    bool isActive;
    
    CraftingJob() : recipeId(0), crafterId(0), timeRemaining(0), isActive(false) {}
    CraftingJob(uint32_t recipeId, EntityID crafterId, float craftTime)
        : recipeId(recipeId)
        , crafterId(crafterId)
        , timeRemaining(craftTime)
        , isActive(true)
    {}
};

// Component for entities that can craft
struct CraftingComponent {
    std::vector<CraftingJob> activeJobs;
    uint32_t maxConcurrentJobs;
    uint32_t skillLevel; // Overall crafting skill
    
    CraftingComponent() : maxConcurrentJobs(1), skillLevel(1) {}
};

// Crafting system - handles recipe processing
class CraftingSystem : public ECS::ISystem {
public:
    CraftingSystem() {
        SetPriority(300);
    }
    
    void Initialize(ECS::Registry& registry) override {
        // Load recipes (in real implementation, load from JSON/data files)
        LoadRecipes();
    }
    
    void Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) override {
        // Get all entities with crafting component
        auto entities = registry.View<CraftingComponent>();
        
        for (EntityID entity : entities) {
            CraftingComponent* crafting = registry.GetComponent<CraftingComponent>(entity);
            Inventory::InventoryComponent* inventory = registry.GetComponent<Inventory::InventoryComponent>(entity);
            
            if (!crafting || !inventory) continue;
            
            // Update active crafting jobs
            for (auto& job : crafting->activeJobs) {
                if (!job.isActive) continue;
                
                job.timeRemaining -= deltaTime;
                
                if (job.timeRemaining <= 0.0f) {
                    CompleteCraftingJob(job, registry, *inventory);
                    job.isActive = false;
                }
            }
            
            // Remove completed jobs
            crafting->activeJobs.erase(
                std::remove_if(crafting->activeJobs.begin(), 
                              crafting->activeJobs.end(),
                              [](const CraftingJob& job) { return !job.isActive; }),
                crafting->activeJobs.end()
            );
        }
    }
    
    // Start crafting a recipe
    bool StartCrafting(EntityID entity, uint32_t recipeId, ECS::Registry& registry) {
        CraftingComponent* crafting = registry.GetComponent<CraftingComponent>(entity);
        Inventory::InventoryComponent* inventory = registry.GetComponent<Inventory::InventoryComponent>(entity);
        
        if (!crafting || !inventory) return false;
        
        // Check if can have more jobs
        uint32_t activeCount = 0;
        for (const auto& job : crafting->activeJobs) {
            if (job.isActive) activeCount++;
        }
        if (activeCount >= crafting->maxConcurrentJobs) return false;
        
        // Find recipe
        auto it = m_recipes.find(recipeId);
        if (it == m_recipes.end()) return false;
        
        const CraftingRecipe& recipe = it->second;
        
        // Check if can craft
        if (!recipe.CanCraft(*inventory)) return false;
        
        // Remove input items
        for (const auto& req : recipe.inputs) {
            inventory->RemoveItem(req.itemId, req.quantity);
        }
        
        // Create crafting job
        CraftingJob job(recipeId, entity, recipe.craftTime);
        crafting->activeJobs.push_back(job);
        
        return true;
    }
    
private:
    std::unordered_map<uint32_t, CraftingRecipe> m_recipes;
    
    void LoadRecipes() {
        // In real implementation, load from JSON/data files
        // Example recipe:
        /*
        CraftingRecipe recipe;
        recipe.recipeId = 1;
        recipe.inputs.push_back({1, 2}); // 2x Iron Ore
        recipe.outputs.push_back({2, 1, 1.0f}); // 1x Iron Ingot
        recipe.craftTime = 5.0f; // 5 seconds
        recipe.requiredSkill = 1;
        m_recipes[1] = recipe;
        */
    }
    
    void CompleteCraftingJob(CraftingJob& job, ECS::Registry& registry, Inventory::InventoryComponent& inventory) {
        auto it = m_recipes.find(job.recipeId);
        if (it == m_recipes.end()) return;
        
        const CraftingRecipe& recipe = it->second;
        
        // Give outputs
        for (const auto& output : recipe.outputs) {
            // Check probability
            if (output.chance >= 1.0f || (rand() / float(RAND_MAX)) < output.chance) {
                Inventory::Item item;
                item.id = output.itemId;
                item.quantity = output.quantity;
                item.stackSize = 64; // Default stack size
                inventory.AddItem(item);
            }
        }
    }
};

} // namespace Crafting
} // namespace Systems
} // namespace Engine


