#include <iostream>
#include <iterator>
#include <experimental/filesystem>
#include <fstream>
#include "args/args.hxx"
#include "maps.hpp"

namespace fs = std::experimental::filesystem;

int main(int argc, const char** argv) {
    args::ArgumentParser parser(
        "Encodes and decodes MAPS strings");
    args::HelpFlag help(parser, "help",
        "Display this help menu", {'h', "help"});
    args::Positional<fs::path> origin(
        parser, "in", "Input file path",
        args::Options::Required);
    args::Positional<fs::path> dest(
        parser, "out", "Output file path",
        args::Options::Required) ;
    args::Flag decode_flag(
        parser, "decode", "Decode input instead of encoding",
        {'d', "decode"});
    try {
        parser.ParseCLI(argc, argv);
    } catch(args::Help) {
        std::cout << parser;
        return 0;
    } catch (args::ParseError e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    } catch (args::ValidationError e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    const auto in_path = args::get(origin);
    const auto out_path = args::get(dest);
    const bool do_decode = args::get(decode_flag);
    std::ofstream out(out_path);
    std::ifstream in(in_path, std::ios::binary);
    if (!in.good()) {
        std::cerr << "Failed to open input file "
                  << in_path << std::endl;
        return 1;
    }
    if (!out.good()) {
        std::cerr << "Failed to open output file "
                  << in_path << std::endl;
        return 1;
    }
    std::istreambuf_iterator<char> in_iter(in);
    std::istreambuf_iterator<char> eof;
    try {
        out << (do_decode ?
                    maps::decode(in_iter, eof) :
                    maps::encode(in_iter, eof)
                    );
    } catch(maps::DecodingError e) {
        std::cerr << "Failed to process file: "
                  << e.what() << std::endl;
        return 1;
    }
    return 0;
}
