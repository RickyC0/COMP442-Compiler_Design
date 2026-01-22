#include <string>
#include <fstream>
#include <vector>

#include "token.h"

std::vector<std::vector<Token>> tokenizeFile(const std::string& filename);

void writeTokensToFile(const std::string& filename, const std::vector<std::vector<Token>>& tokens);

