#ifndef UI_H
#define UI_H

#include <string>
#include <utility>
#include <vector>

struct PhaseSummary {
    std::string name;
    bool success;
    long long durationMs;
    std::string details;
};

namespace UI {

struct PngRenderResult {
    bool dotAvailable;
    bool pngGenerated;
};

void printTitle(const std::string& title);
void printSection(const std::string& title);
void printKV(const std::string& key, const std::string& value);
void printStatusLine(bool success, const std::string& message);
void printWarning(const std::string& message);
void printCrash(const std::string& phaseName, const std::string& errorMessage);
void printSummaryTable(const std::vector<PhaseSummary>& phases);
void printArtifactList(const std::string& title, const std::vector<std::pair<std::string, std::string>>& artifacts);
void printPngGenerationNotes(const PngRenderResult& result);
void printDone();

PngRenderResult renderAstPngFromDot(const std::string& inputDot, const std::string& outputPng);

}  // namespace UI

#endif
