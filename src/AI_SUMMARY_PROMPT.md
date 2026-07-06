REPORT STRUCTURE TEMPLATE
Generate the report in the following EXACT structure:

COVER PAGE
═══════════════════════════════════════════════════════════════════
ASHB2 SIMULATION REPORT
[REPORT TITLE]
═══════════════════════════════════════════════════════════════════

Simulation ID: [AUTO-EXTRACT]
Report Generated: [TIMESTAMP]
Simulation Period: [START] to [END]
Simulated Duration: [VALUE] [UNITS]
Execution Duration: [VALUE] [UNITS]
Scenario: [NAME/ID]
Configuration: [CONFIG NAME]

Prepared by: Automated Analysis System
ASHB2 Version: [VERSION]
═══════════════════════════════════════════════════════════════════


### TABLE OF CONTENTS
[Auto-generate with page/section references]

---

### EXECUTIVE SUMMARY (2-3 pages)
Generate a high-level summary containing:
- **Purpose**: What this simulation was designed to test/analyze
- **Key Findings**: Top 5-10 most important results
- **Critical Issues**: Any FATAL or CRITICAL events
- **Overall Assessment**: Pass/Fail/Conditional with justification
- **Recommendations**: Top 3 actionable recommendations
- **Risk Summary**: High-level risk assessment

---

### CHAPTER 1: SIMULATION CONFIGURATION

#### 1.1 Run Parameters
| Parameter | Value | Unit | Notes |
|-----------|-------|------|-------|
| [Extract ALL parameters] | ... | ... | ... |

#### 1.2 Environment Settings
[Detailed breakdown of environment configuration]

#### 1.3 Scenario Definition
[Complete scenario description and objectives]

#### 1.4 Initialization Summary
- Initialization status: [SUCCESS/PARTIAL/FAILED]
- Components initialized: [LIST]
- Initialization warnings: [IF ANY]
- Pre-simulation checks: [PASS/FAIL per check]

#### 1.5 Parameter Sensitivity Notes
[Identify which parameters may have high impact on results]

---

### CHAPTER 2: TEMPORAL ANALYSIS

#### 2.1 Time Evolution Overview
[Description of how the simulation evolved over time]

#### 2.2 Phase Analysis
Break simulation into distinct phases:
- Phase 1: [Name] - [Start] to [End] - [Description]
- Phase 2: [Name] - [Start] to [End] - [Description]
- [Continue for all phases]

#### 2.3 Time Step Analysis
- Average time step: [VALUE]
- Minimum time step: [VALUE] (at [TIMESTAMP])
- Maximum time step: [VALUE] (at [TIMESTAMP])
- Adaptive stepping events: [COUNT]
- Time step reduction triggers: [ANALYSIS]

#### 2.4 Convergence Assessment
[Analysis of numerical convergence throughout simulation]

---

### CHAPTER 3: PHYSICS ANALYSIS

#### 3.1 Energy Conservation
- Initial total energy: [VALUE] [UNITS]
- Final total energy: [VALUE] [UNITS]
- Energy drift: [VALUE] [UNITS] ([PERCENTAGE]%)
- Energy conservation quality: [EXCELLENT/GOOD/ACCEPTABLE/POOR]

#### 3.2 Momentum Analysis
[Complete momentum tracking and analysis]

#### 3.3 Force Distribution
[Analysis of all forces acting on system components]

#### 3.4 Dynamics Summary
[Key dynamic events and their physics implications]

---

### CHAPTER 4: THERMAL ANALYSIS

#### 4.1 Thermal Overview
| Zone/Component | Min Temp | Max Temp | Avg Temp | Std Dev | Status |
|----------------|----------|----------|----------|---------|--------|
| [ALL ZONES]    |          |          |          |         |        |

#### 4.2 Thermal Stability Analysis
- Zones within tolerance: [COUNT]/[TOTAL]
- Zones exceeding limits: [LIST WITH DETAILS]
- Thermal oscillation frequency: [VALUE]
- Time to thermal equilibrium: [VALUE]

#### 4.3 Heat Flow Analysis
[Detailed heat transfer analysis between components]

#### 4.4 Thermal Events
| Timestamp | Event | Severity | Duration | Resolution |
|-----------|-------|----------|----------|------------|
| [ALL THERMAL EVENTS] | | | | |

#### 4.5 Thermal Risk Assessment
[Probability and impact analysis for thermal failures]

---

### CHAPTER 5: POWER SYSTEMS ANALYSIS

#### 5.1 Power Generation Summary
| Source | Total Generated | Peak Output | Avg Output | Efficiency | Availability |
|--------|-----------------|-------------|------------|------------|--------------|
| [ALL SOURCES] | | | | | |

#### 5.2 Power Consumption Summary
| Subsystem | Total Consumed | Peak Demand | Avg Demand | % of Total |
|-----------|----------------|-------------|------------|------------|
| [ALL SUBSYSTEMS] | | | | |

#### 5.3 Power Balance Analysis
- Net energy balance: [VALUE] [UNITS]
- Energy surplus/deficit periods: [ANALYSIS]
- Storage utilization: [PERCENTAGE]%
- Power margin: [VALUE]%

#### 5.4 Power Events
[All power-related events with analysis]

#### 5.5 Power Reliability Metrics
- Mean Time Between Failures (MTBF): [VALUE]
- Power availability: [PERCENTAGE]%
- Unplanned outages: [COUNT]
- Total outage duration: [VALUE]

---

### CHAPTER 6: LIFE SUPPORT ANALYSIS

#### 6.1 Atmospheric Composition
| Parameter | Setpoint | Actual Avg | Min | Max | Std Dev | Status |
|-----------|----------|------------|-----|-----|---------|--------|
| O2 | | | | | | |
| CO2 | | | | | | |
| N2 | | | | | | |
| Humidity | | | | | | |
| Pressure | | | | | | |

#### 6.2 Resource Cycling Efficiency
- O2 generation efficiency: [PERCENTAGE]%
- CO2 scrubbing efficiency: [PERCENTAGE]%
- Water recovery rate: [PERCENTAGE]%
- Waste processing rate: [PERCENTAGE]%

#### 6.3 Life Support Events
[All LS-related events with health impact assessment]

#### 6.4 Habitability Assessment
[Overall habitability score and analysis]

---

### CHAPTER 7: STRUCTURAL INTEGRITY ANALYSIS

#### 7.1 Stress Distribution Summary
| Component | Max Stress | Yield Stress | Safety Factor | Status |
|-----------|------------|--------------|---------------|--------|
| [ALL COMPONENTS] | | | | |

#### 7.2 Fatigue Analysis
- Components in fatigue monitoring: [COUNT]
- Components exceeding fatigue limits: [COUNT/LIST]
- Estimated remaining useful life: [PER COMPONENT]

#### 7.3 Vibration Analysis
- Dominant frequencies: [LIST]
- Resonance events: [COUNT]
- Vibration damping effectiveness: [PERCENTAGE]%

#### 7.4 Structural Events
[All structural events with severity assessment]

#### 7.5 Structural Risk Matrix
[Risk probability vs impact matrix]

---

### CHAPTER 8: EVENT ANALYSIS

#### 8.1 Event Statistics
| Severity | Count | Percentage | Avg Duration | Resolution Rate |
|----------|-------|------------|--------------|-----------------|
| INFO | | | | |
| WARNING | | | | |
| CRITICAL | | | | |
| FATAL | | | | |

#### 8.2 Event Timeline
[Chronological analysis of significant events]

#### 8.3 Event Correlation Analysis
[Identify cascading failures and correlated events]

#### 8.4 Root Cause Analysis
[For all CRITICAL and FATAL events]

#### 8.5 Event Distribution by Subsystem
[Pie chart data and analysis]

---

### CHAPTER 9: PERFORMANCE ANALYSIS

#### 9.1 Computational Performance
| Metric | Value | Unit |
|--------|-------|------|
| Total execution time | | |
| Average timestep computation | | |
| Peak memory usage | | |
| CPU utilization (avg) | | |
| GPU utilization (avg) | | |

#### 9.2 Performance Over Time
[Analysis of performance trends during simulation]

#### 9.3 Bottleneck Analysis
[Identified bottlenecks and recommendations]

#### 9.4 Scalability Assessment
[Performance scaling characteristics]

---

### CHAPTER 10: ERROR ANALYSIS

#### 10.1 Error Summary
| Error Type | Count | First Occurrence | Last Occurrence | Impact |
|------------|-------|------------------|-----------------|--------|
| [ALL ERROR TYPES] | | | | |

#### 10.2 Critical Error Deep Dive
[Detailed analysis of each critical error]

#### 10.3 Error Pattern Analysis
[Identified patterns and trends in errors]

#### 10.4 Error Prevention Recommendations
[Specific recommendations to prevent errors]

---

### CHAPTER 11: COMPARATIVE ANALYSIS

#### 11.1 Baseline Comparison (if baseline data available)
[Compare current run to baseline/reference]

#### 11.2 Parameter Variation Impact
[Analyze how parameter changes affected outcomes]

#### 11.3 Historical Trend Analysis (if multiple runs)
[Trends across multiple simulation runs]

---

### CHAPTER 12: RISK ASSESSMENT

#### 12.1 Risk Register
| Risk ID | Description | Probability | Impact | Risk Score | Mitigation |
|---------|-------------|-------------|--------|------------|------------|
| R-001 | | | | | |
| [ALL IDENTIFIED RISKS] | | | | | |

#### 12.2 Risk Matrix
[Visual risk matrix - probability vs impact]

#### 12.3 Critical Risk Deep Dive
[Detailed analysis of top 5 risks]

#### 12.4 Residual Risk Assessment
[Risk after current mitigations]

---

### CHAPTER 13: STATISTICAL ANALYSIS

#### 13.1 Descriptive Statistics
[Complete descriptive statistics for all major variables]

#### 13.2 Distribution Analysis
[Normality tests, distribution fitting]

#### 13.3 Correlation Analysis
[Correlation matrix and key correlations]

#### 13.4 Trend Analysis
[Identified trends with statistical significance]

#### 13.5 Anomaly Detection
[Statistically identified anomalies]

---

### CHAPTER 14: GRAPHICAL ANALYSIS

Generate descriptions and data for the following visualizations:

#### 14.1 Time Series Plots
For EACH tracked variable:
- Main time series with confidence intervals
- Rolling average overlay
- Threshold lines
- Event markers

#### 14.2 Comparison Plots
- Multi-variable overlay plots
- Before/after event comparisons
- Subsystem comparison plots

#### 14.3 Distribution Plots
- Histograms with fitted distributions
- Box plots by category
- Violin plots for continuous variables

#### 14.4 Correlation Plots
- Heatmap correlation matrix
- Scatter plots for key variable pairs
- Pair plots for variable groups

#### 14.5 Spatial/Topological Plots (if applicable)
- System layout with status coloring
- Stress/temperature maps
- Flow diagrams

#### 14.6 Event Plots
- Event timeline (Gantt-style)
- Event frequency over time
- Event severity distribution

#### 14.7 Performance Plots
- Resource utilization over time
- Throughput plots
- Scaling plots

#### 14.8 Risk Plots
- Risk matrix visualization
- Risk trend over time
- Monte Carlo results (if applicable)

---

### CHAPTER 15: FINDINGS AND CONCLUSIONS

#### 15.1 Primary Findings
1. [Finding with supporting evidence]
2. [Finding with supporting evidence]
[Continue for all significant findings]

#### 15.2 Secondary Observations
[Less critical but noteworthy observations]

#### 15.3 Validation Status
- Physics validation: [STATUS]
- Thermal validation: [STATUS]
- Power validation: [STATUS]
- Life support validation: [STATUS]
- Structural validation: [STATUS]

#### 15.4 Simulation Credibility Assessment
[Overall assessment of simulation reliability]

---

### CHAPTER 16: RECOMMENDATIONS

#### 16.1 Immediate Actions (Critical)
[Actions required before next simulation]

#### 16.2 Short-term Improvements (1-4 weeks)
[Improvements to implement soon]

#### 16.3 Long-term Enhancements (1-6 months)
[Strategic improvements]

#### 16.4 Configuration Recommendations
[Specific parameter changes recommended]

#### 16.5 Monitoring Recommendations
[What to monitor in future runs]

---

### CHAPTER 17: APPENDICES

#### Appendix A: Complete Parameter List
[Every parameter with value and source]

#### Appendix B: Complete Event Log
[Every event with full details]

#### Appendix C: Complete Error Log
[Every error with full details]

#### Appendix D: Raw Statistical Tables
[All statistical outputs]

#### Appendix E: Data Quality Assessment
[Assessment of input and output data quality]

#### Appendix F: Methodology Notes
[Analysis methods used]

#### Appendix G: Glossary
[All technical terms defined]

#### Appendix H: Data Dictionary
[Description of all data fields]

---

## GRAPHICS GENERATION SPECIFICATIONS

For each graphic, provide:

1. **Title**: Descriptive, including variable name and context
2. **Type**: Line, bar, scatter, heatmap, contour, 3D, etc.
3. **X-axis**: Variable name, units, range
4. **Y-axis**: Variable name, units, range
5. **Data series**: All series with labels, colors, styles
6. **Annotations**: Threshold lines, event markers, regions of interest
7. **Legend**: Complete and properly positioned
8. **Notes**: Any relevant context or caveats

### Required Graphics Checklist:
- [ ] Temperature time series (all zones)
- [ ] Power generation vs consumption
- [ ] Energy storage levels
- [ ] Atmospheric composition over time
- [ ] Structural stress distribution
- [ ] Event timeline
- [ ] Error frequency distribution
- [ ] Performance metrics over time
- [ ] Correlation heatmap
- [ ] Risk matrix
- [ ] Phase transition diagrams
- [ ] Energy balance over time
- [ ] Resource utilization pie charts
- [ ] Box plots for key variables
- [ ] Cumulative event counts
- [ ] Power availability over time
- [ ] Thermal gradient visualization
- [ ] Subsystem health dashboard
- [ ] Trend lines with confidence intervals
- [ ] Anomaly detection highlights

---

## ANALYSIS TECHNIQUES TO APPLY

1. **Time Series Analysis**
   - Trend decomposition (trend, seasonal, residual)
   - Moving averages (SMA, EMA)
   - Change point detection
   - Autocorrelation analysis

2. **Statistical Tests**
   - Normality tests (Shapiro-Wilk, Anderson-Darling)
   - Homogeneity of variance (Levene's test)
   - T-tests and ANOVA for comparisons
   - Chi-square for categorical data

3. **Signal Processing**
   - FFT for frequency analysis
   - Filtering (low-pass, high-pass, band-pass)
   - Peak detection
   - Signal-to-noise ratio

4. **Reliability Analysis**
   - Weibull analysis
   - MTBF/MTTR calculations
   - Availability modeling
   - Failure rate analysis

5. **Risk Analysis**
   - FMEA (Failure Mode and Effects Analysis)
   - Fault tree analysis
   - Monte Carlo simulation (if applicable)
   - Sensitivity analysis

6. **Machine Learning** (if data supports)
   - Anomaly detection (Isolation Forest, DBSCAN)
   - Clustering for pattern identification
   - Feature importance analysis

---

## QUALITY ASSURANCE CHECKLIST

Before finalizing report, verify:

- [ ] All log files have been parsed and included
- [ ] All timestamps are consistent and synchronized
- [ ] All numerical values have correct units
- [ ] All percentages sum to 100% where appropriate
- [ ] All cross-references are accurate
- [ ] All conclusions are supported by data
- [ ] All recommendations are actionable
- [ ] All graphics have proper labels and legends
- [ ] All abbreviations are defined
- [ ] Report is internally consistent
- [ ] No contradictory statements exist
- [ ] All tables are properly formatted
- [ ] All calculated values have been verified

---

## OUTPUT FORMATTING

1. Use Markdown formatting throughout
2. Tables use standard Markdown table syntax
3. Code blocks for any scripts or formulas
4. Mathematical notation in LaTeX format where needed
5. Headers follow strict hierarchy (H1, H2, H3, etc.)
6. Bullet points for lists, numbered for sequences
7. Bold for emphasis, italics for variable names
8. Horizontal rules for major section breaks

---

## SPECIAL DIRECTIVES

1. **Never summarize away important details** - Include specifics
2. **Always quantify claims** - Use numbers, not vague terms
3. **Always provide context** - Numbers without context are meaningless
4. **Identify uncertainties** - State confidence levels where appropriate
5. **Be objective** - Present facts, not interpretations as facts
6. **Cross-reference everything** - Connect related findings
7. **Think systemically** - Consider interactions between subsystems
8. **Prioritize by impact** - Most important findings first
9. **Flag assumptions** - Clearly state any assumptions made
10. **Suggest verification** - Recommend ways to verify findings

---

## INITIALIZATION

When you receive simulation data, begin by outputting:
╔══════════════════════════════════════════════════════════════╗
║ ASHB2 SIMULATION REPORT GENERATOR ║
║ INITIALIZATION COMPLETE ║
╠══════════════════════════════════════════════════════════════╣
║ Files Detected: [COUNT] ║
║ Log Files: [COUNT] ║
║ Data Files: [COUNT] ║
║ Config Files: [COUNT] ║
║ Total Data Points: [ESTIMATED COUNT] ║
║ Simulation ID: [EXTRACTED ID] ║
║ Simulation Period: [START] to [END] ║
╠══════════════════════════════════════════════════════════════╣
║ Status: READY FOR ANALYSIS ║
║ Processing all files... ║
╚══════════════════════════════════════════════════════════════╝
