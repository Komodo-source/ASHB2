#ifndef VALIDATION_FRAMEWORK_H
#define VALIDATION_FRAMEWORK_H

#include <vector>
#include <map>
#include <string>
#include <memory>
#include <functional>
#include <random>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>

// Forward declarations
class Entity;
class FreeWillSystem;
struct Action;

namespace validation {

// Statistical test results
struct StatisticalTestResult {
    std::string testName;
    double statistic;
    double pValue;
    bool significant;
    std::string conclusion;
};

// Calibration parameter range
struct ParameterRange {
    std::string name;
    double minValue;
    double maxValue;
    double currentValue;
    double stepSize;
};

// Sensitivity analysis result
struct SensitivityResult {
    std::string parameterName;
    double sensitivityIndex;  // Sobol index or similar
    double confidenceInterval;
    std::string impactLevel;  // "low", "medium", "high"
};

// Behavioral data point for validation
struct BehavioralDataPoint {
    int entityId;
    int timestamp;
    std::string actionType;
    double observedValue;
    double predictedValue;
    std::map<std::string, double> context;
};

// Validation metrics
struct ValidationMetrics {
    double meanAbsoluteError;
    double rootMeanSquareError;
    double rSquared;
    double correlationCoefficient;
    double calibrationScore;
    std::map<std::string, double> perCategoryMetrics;
};

// T-test implementation for comparing means
class StatisticalTests {
public:
    static double calculateMean(const std::vector<double>& data);
    static double calculateVariance(const std::vector<double>& data, double mean);
    static double calculateStdDev(const std::vector<double>& data, double variance);
    
    // Two-sample t-test
    static StatisticalTestResult twoSampleTTest(
        const std::vector<double>& sample1,
        const std::vector<double>& sample2,
        double alpha = 0.05);
    
    // Chi-square goodness of fit
    static StatisticalTestResult chiSquareGoodnessOfFit(
        const std::vector<double>& observed,
        const std::vector<double>& expected,
        double alpha = 0.05);
    
    // Pearson correlation
    static double pearsonCorrelation(
        const std::vector<double>& x,
        const std::vector<double>& y);
    
    // Calculate R-squared
    static double calculateRSquared(
        const std::vector<double>& observed,
        const std::vector<double>& predicted);
};

// Parameter sensitivity analyzer
class SensitivityAnalyzer {
private:
    FreeWillSystem* system;
    std::vector<ParameterRange> parameters;
    int numSamples;
    std::mt19937 rng;
    
public:
    SensitivityAnalyzer(FreeWillSystem* sys, int samples = 100);
    
    void addParameter(const std::string& name, double min, double max, 
                     double current, double step);
    
    // Morris method for screening
    std::vector<SensitivityResult> analyzeMorris();
    
    // Local sensitivity analysis
    SensitivityResult analyzeLocal(const std::string& paramName);
    
    // Run simulation with given parameters
    double runSimulation(const std::map<std::string, double>& paramValues);
};

// Calibration engine
class CalibrationEngine {
private:
    FreeWillSystem* system;
    std::vector<BehavioralDataPoint> empiricalData;
    std::vector<ParameterRange> parameters;
    
public:
    CalibrationEngine(FreeWillSystem* sys);
    
    void loadEmpiricalData(const std::string& filename);
    void addEmpiricalData(const BehavioralDataPoint& point);
    void addParameter(const ParameterRange& param);
    
    // Grid search calibration
    std::map<std::string, double> calibrateGridSearch();
    
    // Bayesian optimization (simplified)
    std::map<std::string, double> calibrateBayesian(int iterations = 50);
    
    // Genetic algorithm calibration
    std::map<std::string, double> calibrateGenetic(int generations = 100, 
                                                    int populationSize = 50);
    
    // Calculate fitness score
    double calculateFitness(const std::map<std::string, double>& params);
    
    // Export calibration report
    void exportReport(const std::string& filename);
};

// Validation manager
class ValidationManager {
private:
    FreeWillSystem* system;
    std::vector<BehavioralDataPoint> testData;
    ValidationMetrics currentMetrics;
    
public:
    ValidationManager(FreeWillSystem* sys);
    
    void loadTestData(const std::string& filename);
    void addTestData(const BehavioralDataPoint& point);
    
    // Run full validation suite
    ValidationMetrics runValidation();
    
    // Cross-validation
    ValidationMetrics crossValidate(int k = 5);
    
    // Compare model variants
    StatisticalTestResult compareModels(
        FreeWillSystem* model1, 
        FreeWillSystem* model2);
    
    // Export validation report
    void exportReport(const std::string& filename, 
                     const ValidationMetrics& metrics);
    
    // Get current metrics
    const ValidationMetrics& getMetrics() const { return currentMetrics; }
};

} // namespace validation

#endif // VALIDATION_FRAMEWORK_H
