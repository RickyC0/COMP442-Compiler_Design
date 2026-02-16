#include <string>
#include <fstream>
#include <vector>

#include "token.h"

#ifndef IO_H
#define IO_H

std::tuple<std::vector<std::vector<Token>>, std::vector<std::vector<Token>>> tokenizeFile(const std::string& filename);

void writeTokensToFile(const std::string& filename, const std::vector<std::vector<Token>>& tokens);

void writeErrorsToFile(const std::string& filename, const std::vector<std::vector<Token>>& tokens);

std::vector<std::vector<Token>> lex_file(const std::string& input_file,  const std::string valid_out_file, const std::string invalid_out_file);

#endif // IO_H

