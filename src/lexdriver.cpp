#include <iostream>
#include <vector>
#include <string>

#include "../include/io.h"
#include "../include/token.h"

 int main(int argc, char* argv[]) {
     std::cout << "DEBUG: Lexical Driver Started." << std::endl;

//     if(argc < 2){
//         std::cerr << "Usage: " << argv[0] << " <source_file>" << std::endl;
//         return 1;
//     }

     std::cout << "Will start lexical analysis..." << std::endl;
     std::string sourceFile = "../My-tests/test1.src";

     std::cout << "Lexical analysis started for file: " << sourceFile << std::endl;

      try {
          auto tokens = tokenizeFile(sourceFile);
          Token token = Token(Token::Type::ID_, "111", 1);
        
          std::string outputFile = "../tokens_output.txt";

    //          std::vector<std::vector<Token>> tokens;
          writeTokensToFile(outputFile, tokens);

          std::cout << "Tokenization completed. Tokens written to " << outputFile << std::endl;
      } catch (const std::exception& e) {
          std::cerr << "Error: " << e.what() << std::endl;
          return 1;
      }

     return 0;
 }