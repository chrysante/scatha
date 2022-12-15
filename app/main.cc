#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <Lexer/Lexer.h>

int main(int argc, char const** argv) {
    if (argc != 2) { return -1; }
    std::filesystem::path const filePath = argv[1];
    std::fstream file(filePath);
    if (!file) {
        std::cerr << "Failed to open file " << filePath << ".\n";
        return -1;
    }
    std::stringstream sstr;
    sstr << file.rdbuf();
    std::string const text = sstr.str();
    
    scatha::issue::LexicalIssueHandler lexIssueHandler;
    auto tokens = scatha::lex::lex(text, lexIssueHandler);
}
