#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

// Best hash that I could find so far:
#include "meow_hash/meow_hash_x64_aesni.h"

// Cross-platform memory mapping:
#include "portable-memory-mapping/MemoryMapped.h"

#define REPORT_ERROR_AND_EXIT(user_description)                                \
  do {                                                                         \
    std::cerr << user_description << "\n";                                     \
    exit(EXIT_FAILURE);                                                        \
  } while (0);

#if ASSERTIONS
#define Assert(Expression)                                                     \
  do {                                                                         \
    if (!(Expression)) {                                                       \
      *(volatile int *)0 = 0;                                                  \
    }                                                                          \
  } while (0);
#else
#define Assert(Expression)
#endif

namespace vsign {

struct Settings {
  int verbose = 0;
  int verify = 0;
  unsigned long long block_size = 1024 * 1024;
  unsigned long long threads = std::thread::hardware_concurrency();
  const char *input = nullptr;
  const char *output = nullptr;
};

const char *USAGE_TEXT = "\nUsage: vsign [OPTIONS] INPUT_FILE [OUTPUT_FILE]\n";
const char *HELP_TEXT =
    "\n"
    "Creates binary signature of contents of INPUT_FILE and writes to "
    "OUTPUT_FILE\n"
    "(by default will write to 'INPUT_FILE.signature')\n\n"
    "Options:\n"
    " -b\t\tBlock size (bytes), default is 1 048 576 bytes\n"
    " -h\t\tPrint help text\n"
    " -t\t\tThreads count, equals to number of logical cores by default \n"
    " -v\t\tVerbose output\n"
    " -y\t\tVerify that OUTPUT_FILE contains correct signature of INPUT_FILE\n";

void print_help_and_exit() {
  std::cout << USAGE_TEXT << HELP_TEXT;
  exit(0);
}

Settings parse_arguments(int argc, char **argv) {
  vsign::Settings settings{};
  for (int count = 1; count < argc; ++count) {
    const char *current_arg = argv[count];
    if (current_arg[0] == '-') {
      if (!strcmp(current_arg, "-v"))
        settings.verbose = 1;
      else if (!strcmp(current_arg, "-y"))
        settings.verify = 1;
      else if (!strcmp(current_arg, "-b"))
        settings.block_size = std::strtoull(argv[++count], nullptr, 0);
      else if (!strcmp(current_arg, "-t"))
        settings.threads = std::strtoull(argv[++count], nullptr, 0);
      else if (!strcmp(current_arg, "-h"))
        print_help_and_exit();
      else
        REPORT_ERROR_AND_EXIT("Wrong argument: " << current_arg << USAGE_TEXT);
    } else {
      if (settings.input == nullptr) {
        settings.input = current_arg;
      } else if (settings.output == nullptr) {
        settings.output = current_arg;
      } else {
        REPORT_ERROR_AND_EXIT(
            "What do you mean by this argument?\n"
            << current_arg
            << "\ninput file already defined as: " << settings.input
            << "\nand output file already defined as: " << settings.output
            << USAGE_TEXT);
      }
    }
  }

  // Verify that settings are correct:
  if (settings.input == nullptr) {
    REPORT_ERROR_AND_EXIT("Missing required argument: input file name\n"
                          << USAGE_TEXT);
  } else if (settings.output == nullptr) {
    static std::string output_name{settings.input};
    output_name += ".signature";
    settings.output = output_name.c_str();
  }

  constexpr size_t MIN_BLOCK_SIZE = sizeof(meow_u128);
  if (settings.block_size < MIN_BLOCK_SIZE) {
    REPORT_ERROR_AND_EXIT("You've set block size (-b) to "
                          << settings.block_size
                          << " bytes but minimal block size is "
                          << MIN_BLOCK_SIZE << " bytes\n"
                          << USAGE_TEXT);
  }
  return settings;
}

void execute_worker(
    const std::shared_ptr<std::atomic<size_t>> &total_blocks_read,
    size_t block_size, const std::shared_ptr<MemoryMapped> &input,
    const std::shared_ptr<MemoryMapped> &output) {
  try {
    // calculate block sizes and last block position
    int last_block_size = input->size() % block_size;
    const size_t last_position =
        input->size() / block_size - (last_block_size ? 0 : 1);
    if (last_block_size == 0) {
      last_block_size = block_size;
    }

    // get raw pointers to data
    uint8_t *in_mem_begin = static_cast<uint8_t *>(input->accessData());
    meow_u128 *out_mem_begin = static_cast<meow_u128 *>(output->accessData());

    for (;;) {
      // interlocked increment, strong memory ordering
      const size_t position = ++(*total_blocks_read) - 1;
      // calculate memory positions where to read/write
      void *input_memory = in_mem_begin + position * block_size;
      meow_u128 *output_memory = out_mem_begin + position;

      if (position < last_position) {
        *output_memory = MeowHash(MeowDefaultSeed, block_size, input_memory);
      } else if (position == last_position) {
        *output_memory =
            MeowHash(MeowDefaultSeed, last_block_size, input_memory);
      } else {
        // end of file reached
        break;
      }
    }
  } catch (const std::exception &error) {
    printf("Sorry, something went wrong: %s\n", error.what());
    exit(EXIT_FAILURE);
  } catch (...) {
    printf("Sorry, something went wrong\n");
    exit(EXIT_FAILURE);
  }
}

void run(const Settings &settings) {
  if (settings.verbose) {
    std::cout << "Running vsign with settings:\n"
              << "verbose: " << settings.verbose << "\n"
              << "verify: " << settings.verify << "\n"
              << "block_size: " << settings.block_size << "\n"
              << "threads: " << settings.threads << "\n"
              << "input: " << settings.input << "\n"
              << "output: " << settings.output << "\n";
  }

  auto total_blocks_read = std::make_shared<std::atomic<size_t>>();

  // Init input
  auto input = std::make_shared<MemoryMapped>();
  bool ok = input->open_read(settings.input);
  if (!ok) {
    REPORT_ERROR_AND_EXIT("Can't map input file " << settings.input
                                                  << " into memory");
  }

  // Init output
  const size_t last_block_size =
      input->size() % settings.block_size > 0 ? sizeof(meow_u128) : 0;
  const size_t output_size =
      sizeof(meow_u128) * (input->size() / settings.block_size) +
      last_block_size;

  auto output = std::make_shared<MemoryMapped>();
  ok = output->open_write(settings.output, output_size);
  if (!ok) {
    REPORT_ERROR_AND_EXIT("Can't map output file " << settings.output
                                                   << " into memory");
  }

  // start workers
  std::vector<std::thread> threads;
  const size_t threads_to_create = settings.threads - 1;
  threads.reserve(threads_to_create);
  size_t failed_threads = 0;
  for (size_t i = 0; i < threads_to_create; ++i) {
    try {
      threads.emplace_back(execute_worker, total_blocks_read,
                           settings.block_size, input, output);
    } catch (const std::system_error &error) {
      if (settings.verbose) {
        std::cout << "Couldn't create thread: " << error.what() << "\n";
      }
      ++failed_threads;
      if (failed_threads < threads_to_create) {
        --i; // retry for some time, but not infinitely
      }
    }
  }

  // main thread already exists, so do some useful work here too:
  execute_worker(total_blocks_read, settings.block_size, input, output);

  // Wait for completion of all threads
  for (std::thread &thread : threads) {
    thread.join();
  }
}
} // namespace vsign

int main(int argc, char **argv) {
  try {
    auto start_time = std::chrono::system_clock::now();

    vsign::Settings settings = vsign::parse_arguments(argc, argv);
    vsign::run(settings);

    auto duration = std::chrono::system_clock::now() - start_time;
    auto duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    if (settings.verbose) {
      std::cout << "Completed in " << duration_ms.count() << " milliseconds\n";
    }
  } catch (const std::exception &error) {
    printf("Sorry, something went wrong: %s\n", error.what());
  } catch (...) {
    printf("Sorry, something went wrong\n");
  }
  return 0;
}
