#include <iostream>
#include <fstream>
#include <experimental/filesystem>
#include "args/args.hxx"
#include <leveldb/db.h>
#include <leveldb/filter_policy.h>
#include <leveldb/cache.h>
#include <leveldb/zlib_compressor.h>
#include <nlohmann/json.hpp>
#include "maps.hpp"

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

ldb::WriteOptions mcpe_db_write_options()
{
    ldb::WriteOptions write_options;
    return write_options;
}

class LevelDBStatusError : public std::runtime_error {
public:
    const ldb::Status status;
    LevelDBStatusError(ldb::Status status) :
        std::runtime_error(status.ToString()),
        status(status)
    {}
    bool should_throw() const {
        return !this->status.ok();
    }
    void throw_if() const {
        if (should_throw())
            throw *this;
    }
};

class ConversionError : public std::runtime_error {
public:
    ConversionError(std::string what) :
        std::runtime_error(what)
    {}
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

void json_to_ldb(leveldb::DB& db, const json& input) {
    if (!input.is_array())
        throw ConversionError(
            "Top level json object must be an array");
    auto write_options = mcpe_db_write_options();
    for (const auto& element : input) {
        if (!element.is_array() || element.size() != 2)
            throw ConversionError(
                "Objects in the top level array must "
                "be arrays of size 2 (pairs)");
        json key = element[0];
        json value = element[1];
        if (!key.is_string() || !value.is_string())
            throw ConversionError(
                "Pairs on the top level array must "
                "contain two strings");
        std::string dkey, dvalue;
        try {
            dkey = maps::decode(key.get<std::string>());
            dvalue = maps::decode(value.get<std::string>());
        } catch (maps::DecodingError e) {
            throw ConversionError(std::string(
                "Pairs on top level array must be valid "
                "MAPS encoded strings: ") + e.what());
        }
        try {
            LevelDBStatusError(
                db.Put(write_options, dkey, dvalue)).throw_if();
        } catch (LevelDBStatusError e) {
            throw ConversionError(std::string(
                "Failed to write to database: ") + e.what());
        }
    }
}

json ldb_to_json(/*const */leveldb::DB& db) {
    std::unique_ptr<ldb::Iterator> it(
        db.NewIterator(mcpe_db_read_options()));
    auto root = json::array({});
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto ekey = maps::encode(it->key().ToString());
        auto evalue = maps::encode(it->value().ToString());
        root += json::array({ekey, evalue});
    }
    try {
        LevelDBStatusError(it->status()).throw_if();
    } catch (LevelDBStatusError e) {
        throw ConversionError(std::string(
            "Failed to read from database: ") + e.what());
    }
    return root;
}

int main(int argc, const char **argv)
{
    // Mostly copied from args' README
    args::ArgumentParser parser(
        "Converts data stored in Minecraft Bedrock's "
        "save file format to JSON");
    args::HelpFlag help(
        parser, "help", "Display this help menu", {'h', "help"});
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
            std::cerr << "Failed to open database: "
                      << e.what() << std::endl;
            return 1;
        }
        json json_root;
        try {
            json_root = ldb_to_json(*db);
        } catch (ConversionError e) {
            std::cerr
                << "Failed to convert saves db to json: "
                << e.what() << std::endl;
                return 1;
        }
        out_stream << json_root.dump(4) << std::endl;
    } else if (fs::is_directory(out)) {
        std::ifstream in_json(in);
        if (!in_json.good()) {
            std::cerr << "Failed to open input file "
                      << in << " for reading" << std::endl;
            return 1;
        }
        auto options = mcpe_db_options();
        // kinda redundant
        options.create_if_missing = true;
        options.error_if_exists = true;
        std::unique_ptr<ldb::DB> db;
        try {
            db = db_open(out, options);
        } catch (LevelDBStatusError e) {
            std::cerr << "Failed to open database: "
                      << e.what() << std::endl; 
            return 1;
        }
        json parsed;
        in_json >> parsed;
        try {
            json_to_ldb(*db, parsed);
        } catch (ConversionError e) {
            std::cerr
                << "Failed to convert json to saves db: "
                << e.what() << std::endl;
                return 1;
        }
    } else {
        std::cerr <<
            "Either input path or output path must be a "
            "valid directory while the other argument must "
            "be a path to a file." << std::endl;
        return 1;
    }
    return 0;
}
