// Replay one sector of the earlier explicit endpoint-pairing recurrence.
#define main previous_projective_main
#include "../projective41/seams.cpp"
#undef main

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    int n=std::stoi(argv[1]), h=std::stoi(argv[2]);
    if (n<1 || n>35 || h<0 || h>n || (n-h)%2) return 2;
    auto start=std::chrono::steady_clock::now();
    std::size_t peak=0;
    std::uint64_t work=0;
    auto answer=evaluate(n,h,peak,work);
    rusage usage{};
    getrusage(RUSAGE_SELF,&usage);
    std::cout << "{\"n\":" << n << ",\"h\":" << h << ",\"count\":" << answer
              << ",\"peak_states\":" << peak << ",\"transitions\":" << work
              << ",\"rss_kib\":" << usage.ru_maxrss << ",\"seconds\":"
              << std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()
              << "}\n";
}
