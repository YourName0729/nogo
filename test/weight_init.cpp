#include <fstream>
#include <vector>
#include <iostream>
#include <string>

using namespace std;

int main(int argc, char* argv[]) {
    string cmd = argv[1];
    if (cmd == "new") {
        ofstream fout("./weight.bin");
        std::vector<float> weight;
        weight.reserve(65536);
        for (auto i = 0u; i < 65536; ++i) weight.push_back(0.55);
        uint64_t size = weight.size();
        fout.write(reinterpret_cast<const char*>(&size), sizeof(uint64_t));
        fout.write(reinterpret_cast<const char*>(weight.data()), sizeof(float) * size);
    }
    else if (cmd == "print") {
        ifstream fin("./weight.bin");
        std::vector<float> weight;
        uint64_t size = 0;
        fin.read(reinterpret_cast<char*>(&size), sizeof(uint64_t));
        weight.resize(size);
        fin.read(reinterpret_cast<char*>(weight.data()), sizeof(float) * size);
        
        ofstream fout("./weight.txt");
        for (auto v : weight) fout << v << '\n';
    }


    

    return 0;
}