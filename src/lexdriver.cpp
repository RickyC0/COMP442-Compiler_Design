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
          std::string valid_output_file = "../tokens_output.outlextokens";
          std::string invalid_output_file = "../tokens_output.outlexerrors";

    //          std::vector<std::vector<Token>> tokens;

          lex_file(sourceFile,valid_output_file, invalid_output_file);

          std::cout << "Tokenization completed. Tokens written to " << valid_output_file << std::endl;
      } catch (const std::exception& e) {
          std::cerr << "Error: " << e.what() << std::endl;
          return 1;
      }

     return 0;
 }