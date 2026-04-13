#ifndef IO_H
#define IO_H

#include <string>
#include <fstream>
#include <tuple>
#include <vector>

#include "token.h"

struct CompilerOutputPaths {
	std::string baseName;
	std::string outputDir;

	std::string lexerDir;
	std::string parserDir;
	std::string astDir;
	std::string semanticDir;
	std::string codegenDir;

	std::string validTokensFile;
	std::string invalidTokensFile;
	std::string derivationFile;
	std::string syntaxErrorsFile;
	std::string astFile;
	std::string astDotFile;
	std::string astPngFile;
	std::string symbolTablesFile;
	std::string semanticDiagnosticsFile;
	std::string moonOutputFile;
	std::string codegenDiagnosticsFile;
	std::string moonRunLogFile;
};

struct MoonRunResult {
	bool attempted = false;
	bool success = false;
	std::string details;
	std::string moonExecutablePath;
};

// Output path helpers
std::string createOutputDir(const std::string& sourceFile, const std::string& driverExecutablePath);
std::string buildOutputPath(const std::string& outputDir, const std::string& baseName, const std::string& extension);
std::string getBaseName(const std::string& sourceFile);
CompilerOutputPaths prepareCompilerOutputPaths(const std::string& sourceFile, const std::string& driverExecutablePath);

// Lexer I/O
std::tuple<std::vector<std::vector<Token>>, std::vector<std::vector<Token>>> tokenizeFile(const std::string& filename);
void writeTokensToFile(const std::string& filename, const std::vector<std::vector<Token>>& tokens);
void writeErrorsToFile(const std::string& filename, const std::vector<std::vector<Token>>& tokens);
std::vector<std::vector<Token>> lex_file(const std::string& input_file,
										 const std::string valid_out_file,
										 const std::string invalid_out_file,
										 size_t* invalidTokenCount = nullptr);

// Generic text output helpers
bool writeLinesToFile(const std::string& filename, const std::vector<std::string>& lines);
bool writeTextToFile(const std::string& filename, const std::string& text);

// Moon execution helper
MoonRunResult runMoonSimulator(const std::string& driverExecutablePath, const std::string& moonInputFile, const std::string& moonRunLogFile);

// Parser I/O
void writeSyntaxErrorsToFile(const std::string& filename, const std::vector<std::string>& errors);
void writeDerivationToFile(const std::string& filename, const std::vector<std::string>& derivationSteps);

#endif // IO_H

