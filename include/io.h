#ifndef IO_H
#define IO_H

#include <string>
#include <fstream>
#include <vector>

#include "token.h"

// Output path helpers
std::string createOutputDir(const std::string& sourceFile);
std::string buildOutputPath(const std::string& outputDir, const std::string& baseName, const std::string& extension);
std::string getBaseName(const std::string& sourceFile);

// Lexer I/O
std::tuple<std::vector<std::vector<Token>>, std::vector<std::vector<Token>>> tokenizeFile(const std::string& filename);
void writeTokensToFile(const std::string& filename, const std::vector<std::vector<Token>>& tokens);
void writeErrorsToFile(const std::string& filename, const std::vector<std::vector<Token>>& tokens);
std::vector<std::vector<Token>> lex_file(const std::string& input_file, const std::string valid_out_file, const std::string invalid_out_file);

// Parser I/O
void writeSyntaxErrorsToFile(const std::string& filename, const std::vector<std::string>& errors);
void writeDerivationToFile(const std::string& filename);

#endif // IO_H

