#pragma once

#include <vector>
#include <map>
#include <string>
#include <type_traits>
#include <limits>
#include <glm/glm.hpp>
#include "../remesh/Remeshing.h" // For PointCloudPoint, GridKey

// Forward declarations
struct nk_context;

namespace Engine {
namespace Editors {
namespace CameraSystem {
namespace Plotting {

// Histogram data structure
struct HistogramData {
    std::string title;
    std::string xAxisLabel;
    std::vector<int> bins;
    float minValue;
    float maxValue;
    float meanValue;
    size_t totalCount;
    int numBins;
    
    HistogramData() : minValue(0.0f), maxValue(1.0f), meanValue(0.0f), totalCount(0), numBins(20) {}
};

// Type traits to detect numeric types
template<typename T>
struct is_numeric : std::integral_constant<bool,
    std::is_arithmetic<T>::value && !std::is_same<T, bool>::value &&
    !std::is_same<T, char>::value && !std::is_same<T, unsigned char>::value> {};

// Specialization for glm::vec3 (extract components)
template<>
struct is_numeric<glm::vec3> : std::true_type {};

// Extract numeric values from different data types
namespace Detail {
    // For std::vector<float> or std::vector<int> etc.
    template<typename T>
    typename std::enable_if<is_numeric<T>::value, std::vector<float>>::type
    ExtractNumericValues(const std::vector<T>& data) {
        std::vector<float> result;
        result.reserve(data.size());
        for (const auto& val : data) {
            result.push_back(static_cast<float>(val));
        }
        return result;
    }
    
    // For std::map with numeric keys
    template<typename K, typename V>
    typename std::enable_if<is_numeric<K>::value && !is_numeric<V>::value, std::vector<float>>::type
    ExtractNumericValues(const std::map<K, V>& data) {
        std::vector<float> result;
        result.reserve(data.size());
        for (const auto& pair : data) {
            result.push_back(static_cast<float>(pair.first));
        }
        return result;
    }
    
    // For std::map with numeric values
    template<typename K, typename V>
    typename std::enable_if<!is_numeric<K>::value && is_numeric<V>::value, std::vector<float>>::type
    ExtractNumericValues(const std::map<K, V>& data) {
        std::vector<float> result;
        result.reserve(data.size());
        for (const auto& pair : data) {
            result.push_back(static_cast<float>(pair.second));
        }
        return result;
    }
    
    // For std::map with both numeric keys and values - extract values by default
    template<typename K, typename V>
    typename std::enable_if<is_numeric<K>::value && is_numeric<V>::value, std::vector<float>>::type
    ExtractNumericValues(const std::map<K, V>& data) {
        std::vector<float> result;
        result.reserve(data.size());
        for (const auto& pair : data) {
            result.push_back(static_cast<float>(pair.second));
        }
        return result;
    }
    
    // For PointCloudPoint - extract weight
    std::vector<float> ExtractNumericValues(const std::vector<PointCloudPoint>& data);
    
    // For std::map<GridKey, PointCloudPoint> - extract weights
    std::vector<float> ExtractNumericValues(const std::map<GridKey, PointCloudPoint>& data);
    
    // For glm::vec3 - extract x, y, z components separately
    std::vector<std::pair<std::string, std::vector<float>>> ExtractNumericValues(const std::vector<glm::vec3>& data);
    
    // For std::map with PointCloudPoint values - extract weight and position components
    std::vector<std::pair<std::string, std::vector<float>>> ExtractNumericValues(const std::map<GridKey, PointCloudPoint>& data, bool extractComponents);
    
    // For GridKey - extract x, y, z components
    std::vector<std::pair<std::string, std::vector<float>>> ExtractNumericValues(const std::vector<GridKey>& data);
}

// Initialize histogram data from a vector of floats
HistogramData InitializeHistogram(const std::vector<float>& values, 
                                   const std::string& title = "Histogram",
                                   const std::string& xAxisLabel = "Value",
                                   int numBins = 20,
                                   float minValue = std::numeric_limits<float>::quiet_NaN(),
                                   float maxValue = std::numeric_limits<float>::quiet_NaN());

// Generic histogram initialization from any numeric container
template<typename Container>
HistogramData InitializeHistogram(const Container& data,
                                   const std::string& title = "Histogram",
                                   const std::string& xAxisLabel = "Value",
                                   int numBins = 20) {
    auto values = Detail::ExtractNumericValues(data);
    return InitializeHistogram(values, title, xAxisLabel, numBins);
}

// Initialize multiple histograms from structured data (e.g., structs with multiple numeric fields)
// Generic template version - tries to extract as single numeric vector
template<typename Container>
std::vector<std::pair<std::string, HistogramData>> InitializeHistograms(const Container& data,
                                                                          const std::string& baseTitle = "Histogram") {
    std::vector<std::pair<std::string, HistogramData>> result;
    
    // Try to extract as single numeric vector first
    auto singleValues = Detail::ExtractNumericValues(data);
    if (!singleValues.empty()) {
        result.push_back({baseTitle, InitializeHistogram(singleValues, baseTitle, "Value")});
        return result;
    }
    
    // If that fails, return empty (specialized versions will handle structured types)
    return result;
}

// Overloaded functions for specific types that need component extraction
// These are implemented in Plotting.cpp
std::vector<std::pair<std::string, HistogramData>> InitializeHistograms(const std::vector<PointCloudPoint>& data,
                                                                          const std::string& baseTitle);

std::vector<std::pair<std::string, HistogramData>> InitializeHistograms(const std::map<GridKey, PointCloudPoint>& data,
                                                                          const std::string& baseTitle);

// Render a single histogram
void RenderHistogram(struct nk_context* ctx, const HistogramData& data);

// Render multiple histograms (for structured data)
void RenderHistograms(struct nk_context* ctx, const std::vector<std::pair<std::string, HistogramData>>& histograms);

} // namespace Plotting
} // namespace CameraSystem
} // namespace Editors
} // namespace Engine
