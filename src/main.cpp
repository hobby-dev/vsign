#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

// Best hash that I could find so far:
#include "meow_hash/meow_hash_x64_aesni.h"

#define ERROR(user_description)                                                \
  {                                                                            \
    std::cerr << user_description << "\n";                                     \
    exit(EXIT_FAILURE);                                                        \
  }

#if ASSERTIONS
#define Assert(Expression)                                                     \
  {                                                                            \
    if (!(Expression)) {                                                       \
      *(volatile int *)0 = 0;                                                  \
    }                                                                          \
  }
#else
#define Assert(Expression)
#endif

namespace vsign {

// Configure memory consumption:
constexpr unsigned long long MIN_BLOCK_SIZE = sizeof(meow_u128);
constexpr unsigned long long DEFAULT_BLOCK_SIZE = 1024 * 1024;
constexpr unsigned long long MIN_BLOCKS_COUNT = 2;
constexpr unsigned long long DEFAULT_BLOCKS_COUNT = 32;

struct Settings {
  int verbose = 0;
  int verify = 0;
  unsigned long long block_size = DEFAULT_BLOCK_SIZE;
  unsigned long long blocks_count = DEFAULT_BLOCKS_COUNT;
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
    " -c\t\tBlocks count, default is 32\n"
    " -h\t\tPrint help text\n"
    " -v\t\tVerbose output\n"
    " -y\t\tVerify that OUTPUT_FILE contains correct signature of INPUT_FILE\n";

void print_help_and_exit() {
  std::cout << USAGE_TEXT << HELP_TEXT;
  exit(0);
}

Settings parse_arguments(int argc, char **argv) noexcept {
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
      else if (!strcmp(current_arg, "-c"))
        settings.blocks_count = std::strtoull(argv[++count], nullptr, 0);
      else if (!strcmp(current_arg, "-h"))
        print_help_and_exit();
      else
        ERROR("Wrong argument: " << current_arg << USAGE_TEXT);
    } else {
      if (settings.input == nullptr) {
        settings.input = current_arg;
      } else if (settings.output == nullptr) {
        settings.output = current_arg;
      } else {
        ERROR("What do you mean by this argument?\n"
              << current_arg
              << "\ninput file already defined as: " << settings.input
              << "\nand output file already defined as: " << settings.output
              << USAGE_TEXT);
      }
    }
  }

  // Verify that settings are correct:
  if (settings.input == nullptr) {
    ERROR("Missing required argument: input file name\n" << USAGE_TEXT);
  } else if (settings.output == nullptr) {
    static std::string output_name{settings.input};
    output_name += ".signature";
    settings.output = output_name.c_str();
  }

  if (settings.block_size < MIN_BLOCK_SIZE) {
    ERROR("You've set block size (-b) to "
          << settings.block_size << " bytes but minimal block size is "
          << MIN_BLOCK_SIZE << " bytes\n"
          << USAGE_TEXT);
  }
  if (settings.blocks_count < MIN_BLOCKS_COUNT) {
    ERROR("You've set blocks count (-c) to " << settings.blocks_count
                                             << " but minimal blocks count is "
                                             << MIN_BLOCKS_COUNT << "\n");
  }
  return settings;
}

// Managing our files in RAII-way
class File {
  File(File &other) = delete;
  File(File &&other) = delete;

public:
  File(const char *path, const char *mode) {
    fd = fopen(path, mode);
    if (fd == nullptr) {
      ERROR("Can't open file " << path);
    }
  }

  ~File() { fclose(fd); }

  FILE *get_fd() const { return fd; }

private:
  FILE *fd;
};

// Simple single-producer single-consumer FIFO buffer
class Blocks {
  Blocks(Blocks &other) = delete;
  Blocks(Blocks &&other) = delete;

public:
  Blocks(size_t block_size, size_t blocks_count)
      : block_size(block_size), blocks_count(blocks_count) {
    const size_t storage_size = blocks_count * block_size;
    memory = static_cast<uint8_t *>(malloc(storage_size));
    if (memory == nullptr) {
      ERROR("Can't allocate "
            << storage_size << " bytes of memory for " << blocks_count
            << " blocks of " << block_size << " bytes each.\n"
            << "Try decreasing block size (-b) or blocks count (-c)\n");
    }
  }

  ~Blocks() { free(memory); }

  // Access input block for writing
  void *producer_access_block() const {
    return memory + block_to_input.load(std::memory_order_relaxed) * block_size;
  }

  // Mark input block as 'ready' for processing
  void producer_commit_block() {
    size_t future = block_to_input.load(std::memory_order_relaxed) + 1;
    if (future == blocks_count)
      future = 0;
    // While buffer is full - sleep in input thread
    // to  give more CPU and disk resources to other threads
    while (block_to_process.load(std::memory_order_acquire) == future)
      std::this_thread::sleep_for(std::chrono::duration<long, std::milli>(1));
    block_to_input.store(future, std::memory_order_release);
  }

  void producer_complete() { exhausted.store(1, std::memory_order_release); }

  // Access oldest ready block
  void *consumer_peek_block() const {
    size_t consumer_block = block_to_process.load(std::memory_order_relaxed);
    // If buffer is empty, return nullptr
    if (block_to_input.load(std::memory_order_acquire) == consumer_block) {
      return nullptr;
    }
    return memory + (consumer_block * block_size);
  }

  void consumer_free_block() {
    size_t future = block_to_process.load(std::memory_order_relaxed) + 1;
    if (future == blocks_count)
      future = 0;
    block_to_process.store(future, std::memory_order_release);
  }

  bool consumer_exhausted() const {
    return block_to_input.load(std::memory_order_acquire) ==
               block_to_process.load(std::memory_order_relaxed) &&
           exhausted.load(std::memory_order_acquire);
  }

  const size_t block_size{0};

private:
  const size_t blocks_count{0};
  uint8_t *memory{nullptr};
  std::atomic<size_t> block_to_process{0};
  std::atomic<size_t> block_to_input{0};
  std::atomic<size_t> exhausted{0};
};

void execute_worker(Blocks *blocks, const char *output_file_name,
                    size_t *total_blocks_written) {
  File output_file{output_file_name, "wb"};
  size_t blocks_written{0};
  const size_t block_size = blocks->block_size;
  for (;;) {
    void *block = blocks->consumer_peek_block();
    if (block != nullptr) {
      const meow_u128 hash = MeowHash(MeowDefaultSeed, block_size, block);
      const size_t written =
          fwrite(&hash, sizeof(meow_u128), 1, output_file.get_fd());
      if (written != 1) {
        const auto error = ferror(output_file.get_fd());
        ERROR("Error writing to file: " << strerror(error));
      }
      blocks->consumer_free_block();
      blocks_written += written;
    } else if (blocks->consumer_exhausted()) {
      // exit when there's no more work to do
      break;
    }
  }
  *total_blocks_written = blocks_written;
}

void run(const Settings &settings) {
  if (settings.verbose) {
    std::cout << "Running vsign with settings:\n"
              << "verbose: " << settings.verbose << "\n"
              << "verify: " << settings.verify << "\n"
              << "block_size: " << settings.block_size << "\n"
              << "blocks_count: " << settings.blocks_count << "\n"
              << "input: " << settings.input << "\n"
              << "output: " << settings.output << "\n";
  }

  // 1. Initialization
  const size_t block_size = settings.block_size;
  Blocks blocks{block_size, settings.blocks_count};
  size_t total_blocks_written{0};

  // 2. Start worker thread
  std::thread worker{execute_worker, &blocks, settings.output,
                     &total_blocks_written};

  // 3. Read input file in main thread
  File input_file{settings.input, "rb"};
  size_t total_blocks_read{0};
  for (;;) {
    const size_t blocks_read = fread(blocks.producer_access_block(), block_size,
                                     1, input_file.get_fd());
    if (blocks_read != 1) {
      const auto error = ferror(input_file.get_fd());
      if (error) {
        ERROR("Error reading input file: " << strerror(error));
      } else if (blocks_read == 0) {
        break; // End of file
      } else {
        ERROR("Unknown error reading input file");
      }
    }
    blocks.producer_commit_block();
    total_blocks_read += blocks_read;
  }

  // 5. Wait for completion
  blocks.producer_complete();
  worker.join();

  // 6. Report results
  if (settings.verbose) {
    std::cout << "Blocks read: " << total_blocks_read << "\n"
              << "Blocks written: " << total_blocks_written << "\n";
  }

  if (total_blocks_written != total_blocks_read)
    ERROR("Blocks read: " << total_blocks_read
                          << " blocks written: " << total_blocks_written);
}
} // namespace vsign

int main(int argc, char **argv) {
  auto start_time = std::chrono::system_clock::now();

  vsign::Settings settings = vsign::parse_arguments(argc, argv);
  vsign::run(settings);

  auto duration = std::chrono::system_clock::now() - start_time;
  auto duration_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(duration);
  if (settings.verbose)
    std::cout << "Completed in " << duration_ms.count() << " milliseconds\n";
  return 0;
}
