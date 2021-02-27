#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

void GetReadWriteBytes(uint64_t& read_bytes, uint64_t& write_bytes) {
  pid_t pid = getpid();
  std::string proc_io = "/proc/" + std::to_string(pid) + "/io";
  std::ifstream ifs(proc_io);
  std::string s;
  while (std::getline(ifs, s)) {
    // std::cout << s << std::endl;
    if (s.find("read_bytes: ") == 0) {
      auto pos = s.find(":") + 1;
      read_bytes = std::stoul(s.substr(pos));
    } else if (s.find("write_bytes: ") == 0) {
      auto pos = s.find(":") + 1;
      write_bytes = std::stoul(s.substr(pos));
    }
  }
}

int Write(int fd, const char* buf, size_t len) {
  ssize_t ret, nr;
  while (len != 0 && (ret = write(fd, buf, len)) != 0) {
    if (ret == -1) {
      if (errno == EINTR) {
        continue;
      }
      perror("write");
      return -1;
    }
    len -= ret;
    buf += ret;
  }
  return 0;
}

int WriteFile(int fd, int unit, uint64_t fsize) {
  std::vector<char> buf(unit);
  for (auto& c : buf) {
    c = rand() % 256;
  }
  for (off_t offset = 0; offset < fsize; offset += unit) {
    size_t n = std::min((size_t)(fsize - offset), (size_t)unit);
    int ret = Write(fd, buf.data(), n);
    if (ret != 0) {
      return -1;
    }
  }
  return 0;
}

int InitFile(int fd, uint32_t fsize) {
  int ret = posix_fallocate(fd, 0, fsize);
  if (ret != 0) {
    perror("posix_fallocate");
    return -1;
  }
  return WriteFile(fd, 32 * 4096, fsize);
}

uint64_t GetCurrUS() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return tv.tv_sec * 1000000 + tv.tv_usec;
}

struct File {
  int fd;
  std::string fname;
};

struct FileCleaner {
  void operator()(File* f) const {
    if (close(f->fd) != 0) {
      perror("close");
    }
    if (remove(f->fname.c_str()) != 0) {
      perror("remove");
    }
  }
};

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "Usage: " << argv[0] << " write_unit" << std::endl;
    return -1;
  }

  int unit = std::stoi(argv[1]);

  std::string fname = "/tmp/" + std::to_string(GetCurrUS()) + ".tmp";
  int fd = open(fname.c_str(), O_WRONLY | O_CREAT, 0666);
  if (fd < 0) {
    perror("open");
    return -1;
  }
  File f{fd, fname};
  std::unique_ptr<File, FileCleaner> defer_close(&f);

  constexpr uint64_t fsize = 128 * 1024 * 1024;  // 128MB
  int ret = InitFile(fd, fsize);
  if (ret != 0) {
    return -1;
  }
  off_t offset = lseek(fd, 0, SEEK_SET);
  assert(offset == 0);

  std::string evict_cache = "vmtouch -e " + fname;
  system(evict_cache.data());

  uint64_t read_bytes0 = 0;
  uint64_t write_bytes0 = 0;
  GetReadWriteBytes(read_bytes0, write_bytes0);

  ret = WriteFile(fd, unit, fsize);
  if (ret != 0) {
    return -1;
  }

  uint64_t read_bytes1 = 0;
  uint64_t write_bytes1 = 0;
  GetReadWriteBytes(read_bytes1, write_bytes1);
  printf("file_size %lu write_unit %d write_bytes %lu read_bytes %lu\n", fsize,
         unit, write_bytes1 - write_bytes0, read_bytes1 - read_bytes0);
  return 0;
}