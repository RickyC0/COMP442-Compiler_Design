#include "../include/ui.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {
namespace ANSI {
const std::string Reset = "\033[0m";
const std::string Bold = "\033[1m";
const std::string Cyan = "\033[36m";
const std::string Green = "\033[32m";
const std::string Yellow = "\033[33m";
const std::string Red = "\033[31m";
const std::string Gray = "\033[90m";
}

bool shouldUseColor() {
    return std::getenv("NO_COLOR") == nullptr;
}

std::string colorize(const std::string& text, const std::string& color) {
    if (!shouldUseColor()) {
        return text;
    }
    return color + text + ANSI::Reset;
}

std::string statusTag(bool success) {
    return success ? colorize("[OK]", ANSI::Green) : colorize("[FAIL]", ANSI::Red);
}

void printRule(char c = '=', int width = 90) {
    std::cout << colorize(std::string(width, c), ANSI::Gray) << "\n";
}

bool isGraphvizDotAvailable() {
#ifdef _WIN32
    return std::system("dot -V >NUL 2>&1") == 0;
#else
    return std::system("dot -V >/dev/null 2>&1") == 0;
#endif
}

bool renderDotImage(const std::string& inputDot, const std::string& outputPng) {
    std::string cmd = "dot -Tpng \"" + inputDot + "\" -o \"" + outputPng + "\"";
    return std::system(cmd.c_str()) == 0;
}
}  // namespace

namespace UI {

void printTitle(const std::string& title) {
    std::cout << "\n";
    printRule('=');
    std::cout << colorize(title, ANSI::Bold + ANSI::Cyan) << "\n";
    printRule('=');
}

void printSection(const std::string& title) {
    std::cout << "\n";
    printRule('-');
    std::cout << colorize(title, ANSI::Bold + ANSI::Cyan) << "\n";
    printRule('-');
}

void printKV(const std::string& key, const std::string& value) {
    std::cout << std::left << std::setw(14) << key << value << "\n";
}

void printStatusLine(bool success, const std::string& message) {
    std::cout << statusTag(success) << " " << message << "\n";
}

void printWarning(const std::string& message) {
    std::cout << colorize("[WARN]", ANSI::Yellow) << " " << message << "\n";
}

void printCrash(const std::string& phaseName, const std::string& errorMessage) {
    std::cerr << statusTag(false) << " " << phaseName << " crashed: " << errorMessage << "\n";
}

void printSummaryTable(const std::vector<PhaseSummary>& phases) {
    std::cout << "\n";
    printRule('=');
    std::cout << colorize("RUN SUMMARY", ANSI::Bold + ANSI::Cyan) << "\n";
    printRule('=');

    std::cout << std::left << std::setw(12) << "Phase"
              << std::setw(10) << "Status"
              << std::setw(10) << "Time"
              << "Details\n";
    printRule('-');

    for (const auto& phase : phases) {
        std::cout << std::left << std::setw(12) << phase.name << std::setw(10) << statusTag(phase.success)
                  << std::setw(10) << (std::to_string(phase.durationMs) + "ms") << phase.details << "\n";
    }

    printRule('=');
}

void printArtifactList(const std::string& title, const std::vector<std::pair<std::string, std::string>>& artifacts) {
    std::cout << "\n" << colorize(title, ANSI::Bold + ANSI::Cyan) << "\n";
    for (const auto& artifact : artifacts) {
        std::cout << "  " << std::left << std::setw(14) << artifact.first << artifact.second << "\n";
    }
}

PngRenderResult renderAstPngFromDot(const std::string& inputDot, const std::string& outputPng) {
    PngRenderResult result{};
    result.dotAvailable = isGraphvizDotAvailable();
    result.pngGenerated = false;

    if (result.dotAvailable) {
        result.pngGenerated = renderDotImage(inputDot, outputPng);
    }

    return result;
}

void printPngGenerationNotes(const PngRenderResult& result) {
    if (!result.dotAvailable) {
        printWarning("Graphviz 'dot' not found. Skipped PNG generation.");
    } else if (!result.pngGenerated) {
        printWarning("Failed to generate AST image from DOT.");
    }
}

void printDone() {
    std::cout << "\nDone.\n";
}

}  // namespace UI
