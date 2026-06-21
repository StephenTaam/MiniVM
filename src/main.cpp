#include "debug/Disassembler.h"
#include "metadata/Loader.h"
#include "metadata/Verifier.h"
#include "vm/VM.h"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string command;
    std::string file;
    std::string entry;
    bool trace = false;
    bool dumpTables = false;
};

void usage(std::ostream& out) {
    out << "Usage:\n"
        << "  stt_vm run <metadata.json> [--trace] [--dump-tables] [--entry name]\n"
        << "  stt_vm dump <metadata.json>\n"
        << "  stt_vm disasm <metadata.json>\n";
}

Options parseArgs(int argc, char** argv) {
    if (argc < 3) {
        usage(std::cerr);
        throw std::runtime_error("missing command or metadata file");
    }

    Options options;
    options.command = argv[1];
    options.file = argv[2];

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--trace") {
            options.trace = true;
        } else if (arg == "--dump-tables") {
            options.dumpTables = true;
        } else if (arg == "--entry") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--entry requires a value");
            }
            options.entry = argv[++i];
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        Options options = parseArgs(argc, argv);

        stt::OpcodeRegistry registry;
        stt::MetadataLoader loader(registry);
        stt::Program program = loader.loadFile(options.file);

        stt::Verifier verifier(registry);
        auto errors = verifier.verify(program);
        if (!errors.empty()) {
            std::cerr << "Metadata verification failed:\n";
            for (const auto& error : errors) {
                std::cerr << "  - " << error << "\n";
            }
            return 1;
        }

        stt::Disassembler disassembler(registry);

        if (options.command == "dump" || options.command == "disasm") {
            disassembler.dumpProgram(program, std::cout);
            return 0;
        }

        if (options.command == "run") {
            if (options.dumpTables) {
                disassembler.dumpProgram(program, std::cout);
                std::cout << "\n";
            }
            stt::VM vm(std::move(program), registry);
            vm.trace = options.trace;
            vm.dumpTables = options.dumpTables;
            (void)vm.run(options.entry);
            return 0;
        }

        usage(std::cerr);
        std::cerr << "Unknown command: " << options.command << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }
}
