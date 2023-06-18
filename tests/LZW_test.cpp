#include <catch2/catch.hpp>
#include <cstdlib>
#include <iostream> // TODO: remove
#include <memory>
#include <sstream>
#include <string>

#include "compressor_base.hpp"
#include "LZW.hpp"

TEST_CASE("Empty LZW compress and decompress")
{
    unsigned dict_size = GENERATE(9U, 10, 12, 13, 16, 17, 23, 24, 27);
    std::string str{};
    std::istringstream iss(str);

    std::ostringstream oss;
    //LZWCompressor<16> lzc(oss);
    std::unique_ptr<Compressor> lzcm = makeLZWCompressor(dict_size, oss);
    Compressor &lzc = *lzcm;
    lzc(iss, str.size());
    lzc.finish();

    //std::cout << oss.str().size() << '\n';
    //std::cout << oss.str() << '\n';

    std::istringstream iss2(oss.str());

    std::ostringstream oss2;
    //LZWDecompressor<16> lzd(oss2);
    std::unique_ptr<Decompressor> lzdm = makeLZWDecompressor(dict_size, oss2);
    Decompressor &lzd = *lzdm;
    lzd(iss2, oss.str().size());
    //std::cout << oss2.str() << '\n';

    //std::cout << "Compression ratio: ";
    //std::cout << static_cast<float>(oss.str().size())/(str.size())*100.0f << "%\n";

    CHECK(str == oss2.str());
}

TEST_CASE("One char LZW compress and decompress")
{
    unsigned dict_size = GENERATE(9U, 10, 12, 13, 16, 17, 23, 24, 27);
    std::string str = "P";
    std::istringstream iss(str);

    std::ostringstream oss;
    //LZWCompressor<16> lzc(oss);
    std::unique_ptr<Compressor> lzcm = makeLZWCompressor(dict_size, oss);
    Compressor &lzc = *lzcm;
    lzc(iss, str.size());
    lzc.finish();

    //std::cout << oss.str().size() << '\n';
    //std::cout << oss.str() << '\n';

    std::istringstream iss2(oss.str());

    std::ostringstream oss2;
    //LZWDecompressor<16> lzd(oss2);
    std::unique_ptr<Decompressor> lzdm = makeLZWDecompressor(dict_size, oss2);
    Decompressor &lzd = *lzdm;
    lzd(iss2, oss.str().size());
    //std::cout << oss2.str() << '\n';

    //std::cout << "Compression ratio: ";
    //std::cout << static_cast<float>(oss.str().size())/(str.size())*100.0f << "%\n";

    CHECK(str == oss2.str());
}

TEST_CASE("Same char LZW compress and decompress")
{
    unsigned dict_size = GENERATE(9U, 10, 12, 13, 16, 17, 23, 24, 27);
    std::string str="PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP";
    std::istringstream iss(str);

    std::ostringstream oss;
    //LZWCompressor<16> lzc(oss);
    std::unique_ptr<Compressor> lzcm = makeLZWCompressor(dict_size, oss);
    Compressor &lzc = *lzcm;
    lzc(iss, str.size());
    lzc.finish();

    //std::cout << oss.str().size() << '\n';
    //std::cout << oss.str() << '\n';

    std::istringstream iss2(oss.str());

    std::ostringstream oss2;
    //LZWDecompressor<16> lzd(oss2);
    std::unique_ptr<Decompressor> lzdm = makeLZWDecompressor(dict_size, oss2);
    Decompressor &lzd = *lzdm;
    lzd(iss2, oss.str().size());
    //std::cout << oss2.str() << '\n';

    //std::cout << "Compression ratio: ";
    //std::cout << static_cast<float>(oss.str().size())/(str.size())*100.0f << "%\n";

    CHECK(str == oss2.str());
}

TEST_CASE("Simple LZW compress and decompress")
{
    unsigned dict_size = GENERATE(9U, 10, 12, 13, 16, 17, 23, 24, 27);
    std::string str = "ABADABA";
    std::istringstream iss(str);

    std::ostringstream oss;
    //LZWCompressor<16> lzc(oss);
    std::unique_ptr<Compressor> lzcm = makeLZWCompressor(dict_size, oss);
    Compressor &lzc = *lzcm;
    lzc(iss, str.size());
    lzc.finish();

    //std::cout << oss.str().size() << '\n';
    //std::cout << oss.str() << '\n';

    std::istringstream iss2(oss.str());

    std::ostringstream oss2;
    //LZWDecompressor<16> lzd(oss2);
    std::unique_ptr<Decompressor> lzdm = makeLZWDecompressor(dict_size, oss2);
    Decompressor &lzd = *lzdm;
    lzd(iss2, oss.str().size());
    //std::cout << oss2.str() << '\n';

    //std::cout << "Compression ratio: ";
    //std::cout << static_cast<float>(oss.str().size())/(str.size())*100.0f << "%\n";

    CHECK(str == oss2.str());
}

static std::string generate_rnd_str(unsigned seed)
{
    std::srand(seed);
    std::size_t str_size = std::rand()%200 + 150; // NOLINT
    std::string res(str_size, ' ');
    for(std::size_t i=0; i<str_size; i++)
    {
        res[i] = 'A' + std::rand()%8; // NOLINT
    }
    return res;
}

TEST_CASE("Multiple strings")
{
    unsigned dict_size = GENERATE(9U, 10, 12, 13, 16, 17, 23, 24, 27);
    unsigned seed_val = GENERATE(13U, 8, 420, 69, 69420);
    std::string str = generate_rnd_str(seed_val);
    std::istringstream iss(str);

    std::ostringstream oss;
    //LZWCompressor<16> lzc(oss);
    std::unique_ptr<Compressor> lzcm = makeLZWCompressor(dict_size, oss);
    Compressor &lzc = *lzcm;
    lzc(iss, str.size());
    lzc.finish();

    //std::cout << oss.str().size() << '\n';
    //std::cout << oss.str() << '\n';

    std::istringstream iss2(oss.str());

    std::ostringstream oss2;
    //LZWDecompressor<16> lzd(oss2);
    std::unique_ptr<Decompressor> lzdm = makeLZWDecompressor(dict_size, oss2);
    Decompressor &lzd = *lzdm;
    lzd(iss2, oss.str().size());
    //std::cout << oss2.str() << '\n';

    //std::cout << "Compression ratio: ";
    //std::cout << static_cast<float>(oss.str().size())/(str.size())*100.0f << "%\n";

    CHECK(str == oss2.str());
}

static std::string generate_large_rnd_str(unsigned seed)
{
    std::srand(seed);
    std::size_t str_size = std::rand()%20000 + 15000; // NOLINT
    std::string res(str_size, ' ');
    for(std::size_t i=0; i<str_size; i++)
    {
        res[i] = std::rand()%255 + 1; // NOLINT
    }
    return res;
}

TEST_CASE("Large binary strings")
{
    unsigned dict_size = GENERATE(9U, 10, 12, 13, 16, 17, 23, 24, 27);
    unsigned seed_val = GENERATE(13U, 8, 420, 69, 69420);
    std::string str = generate_large_rnd_str(seed_val);
    std::istringstream iss(str);

    std::ostringstream oss;
    //LZWCompressor<16> lzc(oss);
    std::unique_ptr<Compressor> lzcm = makeLZWCompressor(dict_size, oss);
    Compressor &lzc = *lzcm;
    lzc(iss, str.size());
    lzc.finish();

    //std::cout << oss.str().size() << '\n';
    //std::cout << oss.str() << '\n';

    std::istringstream iss2(oss.str());

    std::ostringstream oss2;
    //LZWDecompressor<16> lzd(oss2);
    std::unique_ptr<Decompressor> lzdm = makeLZWDecompressor(dict_size, oss2);
    Decompressor &lzd = *lzdm;
    lzd(iss2, oss.str().size());
    //std::cout << oss2.str() << '\n';

    //std::cout << "Compression ratio: ";
    //std::cout << static_cast<float>(oss.str().size())/(str.size())*100.0f << "%\n";

    CHECK(str == oss2.str());
}
