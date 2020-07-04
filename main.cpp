#include <iostream>
#include <fstream>
#include <experimental/filesystem>
#include <memory>
#include "args/args.hxx"
#include <leveldb/db.h>
#include <leveldb/filter_policy.h>
#include <leveldb/cache.h>
#include <leveldb/zlib_compressor.h>
#include <nlohmann/json.hpp>

namespace fs = std::experimental::filesystem;
namespace ldb = leveldb;
using json = nlohmann::json;

ldb::Options mcpe_db_options()
{
    ldb::Options options;
    options.filter_policy = ldb::NewBloomFilterPolicy(10);
    options.write_buffer_size = 4 * 1024 * 1024;
    options.block_cache = leveldb::NewLRUCache(40 * 1024 * 1024);
    options.compressors[0] = new ldb::ZlibCompressor();
    return options;
}

ldb::ReadOptions mcpe_db_read_options()
{
    ldb::ReadOptions read_options;
    return read_options;
}

class LevelDBStatusError : public std::runtime_error {
public:
    const ldb::Status status;
    LevelDBStatusError(ldb::Status status) :
        std::runtime_error(status.ToString()),
        status(status)
    {}
    bool should_throw() {
        return !this->status.ok();
    }
};

auto db_open(fs::path path, ldb::Options options)
{
    ldb::DB* db;
    auto error = LevelDBStatusError(
        ldb::DB::Open(options, path, &db));
    if (error.should_throw())
        throw error;
    return std::unique_ptr<ldb::DB>(db);
}

int main(int argc, const char **argv)
{
    args::ArgumentParser parser(
        "Converts data stored in Minecraft Bedrock's save file format to JSON");
    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
    args::Positional<fs::path> origin(
        parser, "in", "Input path, can be a file or a directory",
        args::Options::Required);
    args::Positional<fs::path> dest(
        parser, "out", "Output path, can be a directory or a file",
        args::Options::Required);
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
    const auto in = args::get(origin);
    const auto out = args::get(dest);

    if (fs::is_directory(in)) {
        std::ofstream out_stream(out);
        if (!out_stream.good()) {
            std::cerr << "Failed to open output file "
                      << out << " for writing" << std::endl;
            return 1;
        }

        std::unique_ptr<ldb::DB> db;
        auto options = mcpe_db_options();
        options.create_if_missing = false;
        try {
            db = db_open(in, options);
        } catch (LevelDBStatusError e) {
            std::cerr << "Failed to open database: " << e.what() << std::endl; 
            return 1;
        }

        std::unique_ptr<ldb::Iterator> it(
            db->NewIterator(mcpe_db_read_options()));
        auto root = json::array({});
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            root += json::array({it->key().ToString(), it->value().ToString()});
        }
        out_stream << root;
    } else if (fs::is_directory(out)) {
        std::ifstream in_json(in);
        if (!in_json.good()) {
            std::cerr << "Failed to open input file "
                      << in << " for reading" << std::endl;
            return 1;
        }
    } else {
        std::cerr << "Either input path or output path must be a valid directory "
                  << "while the other argument must be a path to a file." << std::endl;
        return 1;
    }

    std::cout << in << std::endl;
    std::cout << out << std::endl;
    return 0;
}
