#include <string>
#include <fstream>
#include <vector>

#include "token.h"

#ifndef IO_H
#define IO_H

std::vector<std::vector<Token>> tokenizeFile(const std::string& filename);

void writeTokensToFile(const std::string& filename, const std::vector<std::vector<Token>>& tokens);

#endif // IO_H

