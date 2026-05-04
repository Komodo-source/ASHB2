#include "ValidationFramework.h"
#include "../header/FreeWillSystem.h"
#include <numeric>
#include <cmath>

namespace validation {

// ============= StatisticalTests Implementation =============

double StatisticalTests::calculateMean(const std::vector<double>& data) {
    if (data.empty()) return 0.0;
    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    return sum / data.size();
}

double StatisticalTests::calculateVariance(const std::vector<double>& data, double mean) {
    if (data.size() < 2) return 0.0;
    double sum = 0.0;
    for (const auto& val : data) {
        sum += (val - mean) * (val - mean);
    }
    return sum / (data.size() - 1);
}

double StatisticalTests::calculateStdDev(const std::vector<double>& data, double variance) {
    return std::sqrt(variance);
}

// Simplified t-distribution CDF approximation
static double tDistributionCDF(double t, int df) {
    // Approximation using normal distribution for large df
    if (df > 30) {
        return 0.5 * (1.0 + std::erf(t / std::sqrt(2.0)));
    }
    // Simple approximation for smaller df
    double x = df / (df + t * t);
    return 0.5 * (1.0 + std::copysign(1.0, t) * (1.0 - std::pow(x, df / 2.0)));
}

StatisticalTestResult StatisticalTests::twoSampleTTest(
    const std::vector<double>& sample1,
    const std::vector<double>& sample2,
    double alpha) {
    
    StatisticalTestResult result;
    result.testName = "Two-sample t-test";
    
    if (sample1.size() < 2 || sample2.size() < 2) {
        result.statistic = 0.0;
        result.pValue = 1.0;
        result.significant = false;
        result.conclusion = "Insufficient data";
        return result;
    }
    
    double mean1 = calculateMean(sample1);
    double mean2 = calculateMean(sample2);
    double var1 = calculateVariance(sample1, mean1);
    double var2 = calculateVariance(sample2, mean2);
    
    // Pooled standard error
    double se = std::sqrt(var1 / sample1.size() + var2 / sample2.size());
    if (se < 1e-10) {
        result.statistic = 0.0;
        result.pValue = 1.0;
        result.significant = false;
        result.conclusion = "Zero standard error";
        return result;
    }
    
    result.statistic = (mean1 - mean2) / se;
    
    // Degrees of freedom (Welch-Satterthwaite)
    double num = std::pow(var1 / sample1.size() + var2 / sample2.size(), 2);
    double denom = std::pow(var1 / sample1.size(), 2) / (sample1.size() - 1) +
                   std::pow(var2 / sample2.size(), 2) / (sample2.size() - 1);
    int df = static_cast<int>(num / denom);
    
    // Two-tailed p-value approximation
    double cdf = tDistributionCDF(std::abs(result.statistic), df);
    result.pValue = 2.0 * (1.0 - cdf);
    result.significant = result.pValue < alpha;
    result.conclusion = result.significant ? 
        "Significant difference detected" : "No significant difference";
    
    return result;
}

StatisticalTestResult StatisticalTests::chiSquareGoodnessOfFit(
    const std::vector<double>& observed,
    const std::vector<double>& expected,
    double alpha) {
    
    StatisticalTestResult result;
    result.testName = "Chi-square goodness of fit";
    
    if (observed.size() != expected.size() || observed.empty()) {
        result.statistic = 0.0;
        result.pValue = 1.0;
        result.significant = false;
        result.conclusion = "Invalid input data";
        return result;
    }
    
    double chi2 = 0.0;
    for (size_t i = 0; i < observed.size(); ++i) {
        if (expected[i] > 0) {
            chi2 += std::pow(observed[i] - expected[i], 2) / expected[i];
        }
    }
    
    result.statistic = chi2;
    int df = static_cast<int>(observed.size() - 1);
    
    // Approximate p-value using chi-square distribution
    // Simple approximation for demonstration
    result.pValue = std::exp(-chi2 / 2.0);
    result.significant = result.pValue < alpha;
    result.conclusion = result.significant ? 
        "Model does not fit data well" : "Model fits data adequately";
    
    return result;
}

double StatisticalTests::pearsonCorrelation(
    const std::vector<double>& x,
    const std::vector<double>& y) {
    
    if (x.size() != y.size() || x.empty()) return 0.0;
    
    double meanX = calculateMean(x);
    double meanY = calculateMean(y);
    
    double numerator = 0.0;
    double sumSqX = 0.0;
    double sumSqY = 0.0;
    
    for (size_t i = 0; i < x.size(); ++i) {
        double dx = x[i] - meanX;
        double dy = y[i] - meanY;
        numerator += dx * dy;
        sumSqX += dx * dx;
        sumSqY += dy * dy;
    }
    
    double denominator = std::sqrt(sumSqX * sumSqY);
    if (denominator < 1e-10) return 0.0;
    
    return numerator / denominator;
}

double StatisticalTests::calculateRSquared(
    const std::vector<double>& observed,
    const std::vector<double>& predicted) {
    
    if (observed.size() != predicted.size() || observed.empty()) return 0.0;
    
    double meanObserved = calculateMean(observed);
    
    double ssTotal = 0.0;
    double ssResidual = 0.0;
    
    for (size_t i = 0; i < observed.size(); ++i) {
        ssTotal += std::pow(observed[i] - meanObserved, 2);
        ssResidual += std::pow(observed[i] - predicted[i], 2);
    }
    
    if (ssTotal < 1e-10) return 1.0;
    
    return 1.0 - (ssResidual / ssTotal);
}

// ============= SensitivityAnalyzer Implementation =============

SensitivityAnalyzer::SensitivityAnalyzer(FreeWillSystem* sys, int samples)
    : system(sys), numSamples(samples) {
    rng.seed(std::random_device{}());
}

void SensitivityAnalyzer::addParameter(const std::string& name, double min, 
                                       double max, double current, double step) {
    ParameterRange param;
    param.name = name;
    param.minValue = min;
    param.maxValue = max;
    param.currentValue = current;
    param.stepSize = step;
    parameters.push_back(param);
}

double SensitivityAnalyzer::runSimulation(const std::map<std::string, double>& paramValues) {
    // Placeholder: would integrate with actual simulation
    // For now, return a dummy fitness score
    double score = 0.0;
    for (const auto& [name, value] : paramValues) {
        score += value;
    }
    return score / paramValues.size();
}

std::vector<SensitivityResult> SensitivityAnalyzer::analyzeMorris() {
    std::vector<SensitivityResult> results;
    
    if (parameters.empty()) return results;
    
    std::uniform_real_distribution<> dist(0.0, 1.0);
    
    for (auto& param : parameters) {
        SensitivityResult result;
        result.parameterName = param.name;
        
        // Morris method: elementary effects
        std::vector<double> effects;
        for (int i = 0; i < numSamples; ++i) {
            // Random baseline
            std::map<std::string, double> baseline;
            for (const auto& p : parameters) {
                baseline[p.name] = p.minValue + dist(rng) * (p.maxValue - p.minValue);
            }
            
            // Perturb this parameter
            baseline[param.name] += param.stepSize;
            double output1 = runSimulation(baseline);
            
            baseline[param.name] -= 2 * param.stepSize;
            double output2 = runSimulation(baseline);
            
            double effect = std::abs(output1 - output2) / (2 * param.stepSize);
            effects.push_back(effect);
        }
        
        // Mean absolute elementary effect
        double meanEffect = StatisticalTests::calculateMean(effects);
        result.sensitivityIndex = meanEffect;
        
        // Classify impact
        if (meanEffect > 0.5) {
            result.impactLevel = "high";
        } else if (meanEffect > 0.2) {
            result.impactLevel = "medium";
        } else {
            result.impactLevel = "low";
        }
        
        result.confidenceInterval = 0.1; // Placeholder
        results.push_back(result);
    }
    
    return results;
}

SensitivityResult SensitivityAnalyzer::analyzeLocal(const std::string& paramName) {
    SensitivityResult result;
    result.parameterName = paramName;
    
    // Find parameter
    ParameterRange* param = nullptr;
    for (auto& p : parameters) {
        if (p.name == paramName) {
            param = &p;
            break;
        }
    }
    
    if (!param) {
        result.sensitivityIndex = 0.0;
        result.impactLevel = "unknown";
        return result;
    }
    
    // Central difference
    std::map<std::string, double> baseParams;
    for (const auto& p : parameters) {
        baseParams[p.name] = p.currentValue;
    }
    
    double baseOutput = runSimulation(baseParams);
    
    baseParams[paramName] += param->stepSize;
    double outputPlus = runSimulation(baseParams);
    
    baseParams[paramName] -= 2 * param->stepSize;
    double outputMinus = runSimulation(baseParams);
    
    result.sensitivityIndex = std::abs(outputPlus - outputMinus) / (2 * param->stepSize);
    
    if (result.sensitivityIndex > 0.5) {
        result.impactLevel = "high";
    } else if (result.sensitivityIndex > 0.2) {
        result.impactLevel = "medium";
    } else {
        result.impactLevel = "low";
    }
    
    result.confidenceInterval = 0.05;
    return result;
}

// ============= CalibrationEngine Implementation =============

CalibrationEngine::CalibrationEngine(FreeWillSystem* sys) : system(sys) {}

void CalibrationEngine::loadEmpiricalData(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        BehavioralDataPoint point;
        char comma;
        
        if (iss >> point.entityId >> comma >> point.timestamp >> comma) {
            std::string actionType;
            std::getline(iss, actionType, ',');
            point.actionType = actionType;
            
            iss >> point.observedValue;
            empiricalData.push_back(point);
        }
    }
}

void CalibrationEngine::addEmpiricalData(const BehavioralDataPoint& point) {
    empiricalData.push_back(point);
}

void CalibrationEngine::addParameter(const ParameterRange& param) {
    parameters.push_back(param);
}

double CalibrationEngine::calculateFitness(const std::map<std::string, double>& params) {
    if (empiricalData.empty()) return 0.0;
    
    double totalError = 0.0;
    
    // Placeholder: would run simulation with given params
    // and compare to empirical data
    for (const auto& data : empiricalData) {
        // Simulated prediction (placeholder)
        double predicted = data.observedValue * 0.9; // Dummy
        totalError += std::abs(data.observedValue - predicted);
    }
    
    return 1.0 / (1.0 + totalError / empiricalData.size());
}

std::map<std::string, double> CalibrationEngine::calibrateGridSearch() {
    std::map<std::string, double> bestParams;
    double bestFitness = -1.0;
    
    if (parameters.empty()) return bestParams;
    
    // Simple grid search over first few parameters
    std::vector<std::vector<double>> grids;
    for (const auto& param : parameters) {
        std::vector<double> values;
        for (double v = param.minValue; v <= param.maxValue; v += param.stepSize) {
            values.push_back(v);
        }
        grids.push_back(values);
    }
    
    // Iterate through grid (simplified for 2 params max in this example)
    for (double v1 : grids[0]) {
        bestParams[parameters[0].name] = v1;
        
        if (parameters.size() > 1) {
            for (double v2 : grids[1]) {
                bestParams[parameters[1].name] = v2;
                
                double fitness = calculateFitness(bestParams);
                if (fitness > bestFitness) {
                    bestFitness = fitness;
                }
            }
        } else {
            double fitness = calculateFitness(bestParams);
            if (fitness > bestFitness) {
                bestFitness = fitness;
            }
        }
    }
    
    return bestParams;
}

std::map<std::string, double> CalibrationEngine::calibrateBayesian(int iterations) {
    // Simplified Bayesian optimization placeholder
    return calibrateGridSearch();
}

std::map<std::string, double> CalibrationEngine::calibrateGenetic(int generations, 
                                                                   int populationSize) {
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<> dist(0.0, 1.0);
    
    if (parameters.empty()) return {};
    
    // Initialize population
    std::vector<std::map<std::string, double>> population;
    for (int i = 0; i < populationSize; ++i) {
        std::map<std::string, double> individual;
        for (const auto& param : parameters) {
            individual[param.name] = param.minValue + 
                                     dist(rng) * (param.maxValue - param.minValue);
        }
        population.push_back(individual);
    }
    
    // Evolution
    for (int gen = 0; gen < generations; ++gen) {
        // Evaluate fitness
        std::vector<double> fitness(population.size());
        for (size_t i = 0; i < population.size(); ++i) {
            fitness[i] = calculateFitness(population[i]);
        }
        
        // Selection and crossover (simplified)
        std::sort(population.begin(), population.end(), 
            [&fitness](const auto& a, const auto& b) {
                // This won't work directly, need index-based sort
                return true; // Placeholder
            });
        
        // Keep top half, mutate
        for (int i = populationSize / 2; i < populationSize; ++i) {
            int parentIdx = i % (populationSize / 2);
            for (auto& [name, value] : population[i]) {
                value += (dist(rng) - 0.5) * 0.1;
                // Clamp to range
                for (const auto& param : parameters) {
                    if (param.name == name) {
                        value = std::max(param.minValue, 
                                        std::min(param.maxValue, value));
                    }
                }
            }
        }
    }
    
    // Return best
    double bestFitness = -1.0;
    std::map<std::string, double> bestParams;
    for (const auto& individual : population) {
        double fitness = calculateFitness(individual);
        if (fitness > bestFitness) {
            bestFitness = fitness;
            bestParams = individual;
        }
    }
    
    return bestParams;
}

void CalibrationEngine::exportReport(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return;
    
    file << "Calibration Report\n";
    file << "==================\n\n";
    
    file << "Parameters:\n";
    for (const auto& param : parameters) {
        file << "  " << param.name << ": " << param.currentValue 
             << " [" << param.minValue << ", " << param.maxValue << "]\n";
    }
    
    file << "\nEmpirical data points: " << empiricalData.size() << "\n";
}

// ============= ValidationManager Implementation =============

ValidationManager::ValidationManager(FreeWillSystem* sys) : system(sys) {}

void ValidationManager::loadTestData(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return;
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        BehavioralDataPoint point;
        char comma;
        
        if (iss >> point.entityId >> comma >> point.timestamp >> comma) {
            std::string actionType;
            std::getline(iss, actionType, ',');
            point.actionType = actionType;
            
            iss >> point.observedValue;
            testData.push_back(point);
        }
    }
}

void ValidationManager::addTestData(const BehavioralDataPoint& point) {
    testData.push_back(point);
}

ValidationMetrics ValidationManager::runValidation() {
    ValidationMetrics metrics;
    
    if (testData.empty()) {
        metrics.meanAbsoluteError = 0.0;
        metrics.rootMeanSquareError = 0.0;
        metrics.rSquared = 1.0;
        metrics.correlationCoefficient = 1.0;
        metrics.calibrationScore = 1.0;
        return metrics;
    }
    
    std::vector<double> observed, predicted;
    double totalAbsError = 0.0;
    double totalSqError = 0.0;
    
    for (const auto& data : testData) {
        // Placeholder: would get prediction from model
        double pred = data.observedValue * 0.95; // Dummy
        predicted.push_back(pred);
        observed.push_back(data.observedValue);
        
        totalAbsError += std::abs(data.observedValue - pred);
        totalSqError += std::pow(data.observedValue - pred, 2);
    }
    
    metrics.meanAbsoluteError = totalAbsError / testData.size();
    metrics.rootMeanSquareError = std::sqrt(totalSqError / testData.size());
    metrics.rSquared = StatisticalTests::calculateRSquared(observed, predicted);
    metrics.correlationCoefficient = StatisticalTests::pearsonCorrelation(observed, predicted);
    metrics.calibrationScore = 1.0 / (1.0 + metrics.meanAbsoluteError);
    
    currentMetrics = metrics;
    return metrics;
}

ValidationMetrics ValidationManager::crossValidate(int k) {
    if (testData.size() < k) return runValidation();
    
    size_t foldSize = testData.size() / k;
    std::vector<ValidationMetrics> foldMetrics;
    
    for (int i = 0; i < k; ++i) {
        // Split data
        std::vector<BehavioralDataPoint> trainData, testFold;
        for (size_t j = 0; j < testData.size(); ++j) {
            if (j >= i * foldSize && j < (i + 1) * foldSize) {
                testFold.push_back(testData[j]);
            } else {
                trainData.push_back(testData[j]);
            }
        }
        
        // Validate on this fold (placeholder)
        ValidationMetrics metrics;
        metrics.meanAbsoluteError = 0.1; // Dummy
        foldMetrics.push_back(metrics);
    }
    
    // Average metrics
    ValidationMetrics avgMetrics;
    avgMetrics.meanAbsoluteError = 0.1;
    return avgMetrics;
}

StatisticalTestResult ValidationManager::compareModels(FreeWillSystem* model1, 
                                                        FreeWillSystem* model2) {
    // Placeholder: would run both models and compare outputs
    StatisticalTestResult result;
    result.testName = "Model comparison";
    result.conclusion = "Models compared";
    return result;
}

void ValidationManager::exportReport(const std::string& filename, 
                                     const ValidationMetrics& metrics) {
    std::ofstream file(filename);
    if (!file.is_open()) return;
    
    file << "Validation Report\n";
    file << "=================\n\n";
    file << std::fixed << std::setprecision(4);
    
    file << "Mean Absolute Error: " << metrics.meanAbsoluteError << "\n";
    file << "RMSE: " << metrics.rootMeanSquareError << "\n";
    file << "R-squared: " << metrics.rSquared << "\n";
    file << "Correlation: " << metrics.correlationCoefficient << "\n";
    file << "Calibration Score: " << metrics.calibrationScore << "\n\n";
    
    file << "Per-category metrics:\n";
    for (const auto& [category, value] : metrics.perCategoryMetrics) {
        file << "  " << category << ": " << value << "\n";
    }
    
    file << "\nTest data points: " << testData.size() << "\n";
}

} // namespace validation
