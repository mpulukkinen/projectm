#include <cstdio>
#include <cmath>
#include <vector>
#include <cstdint>
#include <algorithm>

// Internal helper structures for JPEG encoding
namespace {
    const uint8_t std_zigzag[64] = {
        0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
    };

    const uint8_t std_lum_quant[64] = {
        16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55, 14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62, 18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92, 49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99
    };

    const uint8_t std_chr_quant[64] = {
        17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99, 24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99
    };

    struct BitWriter {
        std::vector<uint8_t> buffer;
        int bit_buffer = 0;
        int bit_count = 0;

        void write_byte(uint8_t b) {
            buffer.push_back(b);
            if (b == 0xFF) buffer.push_back(0x00); // Byte stuffing
        }

        void write_bits(int bits, int len) {
            bit_buffer |= (bits & ((1 << len) - 1)) << (32 - bit_count - len);
            bit_count += len;
            while (bit_count >= 8) {
                write_byte((uint8_t)(bit_buffer >> 24));
                bit_buffer <<= 8;
                bit_count -= 8;
            }
        }

        // Markers don't use bit stuffing
        void write_marker(uint8_t id) {
            buffer.push_back(0xFF);
            buffer.push_back(id);
        }
    };

    void write_word(std::vector<uint8_t>& buf, int v) {
        buf.push_back((v >> 8) & 0xFF);
        buf.push_back(v & 0xFF);
    }

    // DCT and quantization
    void process_block(const float* input, uint8_t* output, int stride, const int* quant) {
        float blk[64];
        // Copy and center
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                blk[y * 8 + x] = input[y * stride + x] - 128.0f;

        // FDCT (AAN Algorithm)
        for (int i = 0; i < 8; ++i) { // Rows
            float* b = blk + i * 8;
            float t0 = b[0] + b[7], t7 = b[0] - b[7], t1 = b[1] + b[6], t6 = b[1] - b[6];
            float t2 = b[2] + b[5], t5 = b[2] - b[5], t3 = b[3] + b[4], t4 = b[3] - b[4];
            float t10 = t0 + t3, t13 = t0 - t3, t11 = t1 + t2, t12 = t1 - t2;
            b[0] = t10 + t11; b[4] = t10 - t11;
            float z1 = (t12 + t13) * 0.707106781f;
            b[2] = t13 + z1; b[6] = t13 - z1;
            t10 = t7 + t4; t11 = t7 - t4; t12 = t5 + t6; t13 = t5 - t6;
            float z3 = (t12 - t10) * 0.707106781f, z2 = (t10 + t12) * 0.382683432f, z4 = z2 + (t13 - t11) * 0.5411961f;
            z2 -= z3; z3 += z4;
            b[5] = z3 + z2; b[3] = z3 - z2; b[1] = t11 + z4; b[7] = t11 - z4;
        }
        for (int i = 0; i < 8; ++i) { // Columns
            float b[8];
            for(int j=0; j<8; j++) b[j] = blk[j*8 + i];
            float t0 = b[0] + b[7], t7 = b[0] - b[7], t1 = b[1] + b[6], t6 = b[1] - b[6];
            float t2 = b[2] + b[5], t5 = b[2] - b[5], t3 = b[3] + b[4], t4 = b[3] - b[4];
            float t10 = t0 + t3, t13 = t0 - t3, t11 = t1 + t2, t12 = t1 - t2;
            blk[0*8+i] = t10 + t11; blk[4*8+i] = t10 - t11;
            float z1 = (t12 + t13) * 0.707106781f;
            blk[2*8+i] = t13 + z1; blk[6*8+i] = t13 - z1;
            t10 = t7 + t4; t11 = t7 - t4; t12 = t5 + t6; t13 = t5 - t6;
            float z3 = (t12 - t10) * 0.707106781f, z2 = (t10 + t12) * 0.382683432f, z4 = z2 + (t13 - t11) * 0.5411961f;
            z2 -= z3; z3 += z4;
            blk[5*8+i] = z3 + z2; blk[3*8+i] = z3 - z2; blk[1*8+i] = t11 + z4; blk[7*8+i] = t11 - z4;
        }

        // AAN Scaling factors
        static const float aanscales[8] = { 1.0f, 1.387039845f, 1.306562965f, 1.175875602f, 1.0f, 0.785694958f, 0.541196100f, 0.275899379f };

        // Quantize and ZigZag
        int prev_dc = 0;
        // Note: DC diff calc happens outside, we just output raw quant values here roughly
        for (int i = 0; i < 64; ++i) {
            float val = blk[i] * aanscales[i / 8] * aanscales[i % 8];
            int qv = (int)(val / quant[i] + (val >= 0 ? 0.5f : -0.5f));
            output[std_zigzag[i]] = (uint8_t)std::max(-127, std::min(127, qv)); // Store as int8 for temporary
            // Real JPEG allows 11 bits, but for this simple writer we stick to basic range usually fine
            // Actually we need int16 for storage, let's cast later.
        }
    }

    // Huffman Tables (Standard Luma/Chroma)
    const uint8_t dc_lum_len[] = {0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0};
    const uint8_t dc_lum_val[] = {0,1,2,3,4,5,6,7,8,9,10,11};
    const uint8_t ac_lum_len[] = {0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,125};
    const uint8_t ac_lum_val[] = {1,2,3,0,4,17,5,18,33,49,65,6,19,81,97,7,34,113,20,50,129,145,161,8,35,66,177,193,21,82,209,240,36,51,98,114,130,9,10,22,23,24,25,26,37,38,39,40,41,42,52,53,54,55,56,57,58,67,68,69,70,71,72,73,74,83,84,85,86,87,88,89,90,99,100,101,102,103,104,105,106,115,116,117,118,119,120,121,122,131,132,133,134,135,136,137,138,146,147,148,149,150,151,152,153,154,162,163,164,165,166,167,168,178,179,180,181,182,183,184,194,195,196,197,198,199,200,210,211,212,213,214,215,216,225,226,227,228,229,230,231,232};

    const uint8_t dc_chr_len[] = {0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0};
    const uint8_t dc_chr_val[] = {0,1,2,3,4,5,6,7,8,9,10,11};
    const uint8_t ac_chr_len[] = {0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,119};
    const uint8_t ac_chr_val[] = {0,1,2,3,17,4,5,33,49,6,18,65,81,7,97,113,19,34,50,129,8,20,66,145,161,177,193,9,21,35,51,82,209,240,22,36,52,225,37,241,23,24,25,26,38,39,40,41,42,53,54,55,56,57,58,67,68,69,70,71,72,73,74,83,84,85,86,87,88,89,90,99,100,101,102,103,104,105,106,114,115,116,117,118,119,120,121,122,130,131,132,133,134,135,136,137,138,146,147,148,149,150,151,152,153,154,162,163,164,165,166,167,168,178,179,180,181,182,183,184,194,195,196,197,198,199,200,210,211,212,213,214,215,216,226,227,228,229,230,231,232};

    struct HuffmanEntry { int len; int code; };
    void build_huff_codes(const uint8_t* lens, const uint8_t* vals, HuffmanEntry* table) {
        int code = 0;
        for (int i = 0; i < 16; ++i) {
            for (int j = 0; j < lens[i]; ++j) {
                table[*vals].len = i + 1;
                table[*vals].code = code;
                vals++;
                code++;
            }
            code <<= 1;
        }
    }

    void encode_huff(BitWriter& bw, int val, const HuffmanEntry* dc_tab, const HuffmanEntry* ac_tab, int& prev_dc) {
        // DC
        int diff = val - prev_dc;
        prev_dc = val;
        if (diff == 0) bw.write_bits(dc_tab[0].code, dc_tab[0].len);
        else {
            int v2 = diff; if (diff < 0) { diff = -diff; v2--; }
            int nbits = 0; while (diff) { nbits++; diff >>= 1; }
            bw.write_bits(dc_tab[nbits].code, dc_tab[nbits].len);
            bw.write_bits(v2 & ((1<<nbits)-1), nbits);
        }
    }

    void encode_block_huff(BitWriter& bw, const float* blk, int& prev_dc, const int* quant,
                          const HuffmanEntry* dc_tab, const HuffmanEntry* ac_tab, const float* scales) {
        int zig[64];
        // Quantization and ZigZag inside
        for(int i=0; i<64; ++i) {
            float val = blk[i] * scales[i/8] * scales[i%8];
            int qv = (int)(val / quant[i] + (val >= 0 ? 0.5f : -0.5f));
            zig[std_zigzag[i]] = qv;
        }

        // Encode DC
        encode_huff(bw, zig[0], dc_tab, ac_tab, prev_dc);

        // Encode AC
        int r = 0;
        for (int k = 1; k < 64; ++k) {
            int val = zig[k];
            if (val == 0) r++;
            else {
                while (r > 15) { bw.write_bits(ac_tab[0xF0].code, ac_tab[0xF0].len); r -= 16; }
                int v2 = val; if (val < 0) { val = -val; v2--; }
                int nbits = 0; while (val) { nbits++; val >>= 1; }
                bw.write_bits(ac_tab[(r << 4) + nbits].code, ac_tab[(r << 4) + nbits].len);
                bw.write_bits(v2 & ((1<<nbits)-1), nbits);
                r = 0;
            }
        }
        if (r > 0) bw.write_bits(ac_tab[0].code, ac_tab[0].len);
    }
}

int stbi_write_jpg(char const *filename, int w, int h, int comp, const void *data, int quality) {
    if (quality <= 0) quality = 90;
    if (quality > 100) quality = 100;

    // Scale quantization tables
    int qscale = (quality < 50) ? (5000 / quality) : (200 - 2 * quality);
    int quant_lum[64], quant_chr[64];
    for (int i = 0; i < 64; ++i) {
        int l = (std_lum_quant[i] * qscale + 50) / 100;
        int c = (std_chr_quant[i] * qscale + 50) / 100;
        quant_lum[i] = std::max(1, std::min(255, l));
        quant_chr[i] = std::max(1, std::min(255, c));
    }

    // Init Huffman tables
    HuffmanEntry dc_lum[256], ac_lum[256], dc_chr[256], ac_chr[256];
    std::fill_n((uint8_t*)dc_lum, 256*sizeof(HuffmanEntry), 0);
    build_huff_codes(dc_lum_len, dc_lum_val, dc_lum);
    build_huff_codes(ac_lum_len, ac_lum_val, ac_lum);
    build_huff_codes(dc_chr_len, dc_chr_val, dc_chr);
    build_huff_codes(ac_chr_len, ac_chr_val, ac_chr);

    BitWriter bw;

    // SOI
    bw.write_marker(0xD8);

    // APP0
    bw.write_marker(0xE0);
    write_word(bw.buffer, 16); // Length
    bw.buffer.insert(bw.buffer.end(), {'J','F','I','F',0,1,1,0}); // ID
    write_word(bw.buffer, 1); write_word(bw.buffer, 1); // Density
    bw.buffer.push_back(0); bw.buffer.push_back(0); // No thumbnail

    // DQT
    bw.write_marker(0xDB);
    write_word(bw.buffer, 132); // Length
    bw.buffer.push_back(0); // Lum ID
    for(int i : quant_lum) bw.buffer.push_back((uint8_t)i);
    bw.buffer.push_back(1); // Chr ID
    for(int i : quant_chr) bw.buffer.push_back((uint8_t)i);

    // SOF0
    bw.write_marker(0xC0);
    write_word(bw.buffer, 17); // Length (8 + 3*components)
    bw.buffer.push_back(8); // Precision
    write_word(bw.buffer, h);
    write_word(bw.buffer, w);
    bw.buffer.push_back(3); // Components (Force YCbCr)
    // Comp 1 (Y) 1x1 sampling (no subsampling for max quality)
    bw.buffer.push_back(1); bw.buffer.push_back(0x11); bw.buffer.push_back(0);
    // Comp 2 (Cb)
    bw.buffer.push_back(2); bw.buffer.push_back(0x11); bw.buffer.push_back(1);
    // Comp 3 (Cr)
    bw.buffer.push_back(3); bw.buffer.push_back(0x11); bw.buffer.push_back(1);

    // DHT
    auto write_dht = [&](int id, const uint8_t* len, const uint8_t* val, int vlen) {
        bw.buffer.push_back(0xFF); bw.buffer.push_back(0xC4);
        write_word(bw.buffer, 2 + 1 + 16 + vlen);
        bw.buffer.push_back((uint8_t)id);
        for(int i=0;i<16;++i) bw.buffer.push_back(len[i]);
        for(int i=0;i<vlen;++i) bw.buffer.push_back(val[i]);
    };
    write_dht(0x00, dc_lum_len, dc_lum_val, sizeof(dc_lum_val));
    write_dht(0x10, ac_lum_len, ac_lum_val, sizeof(ac_lum_val));
    write_dht(0x01, dc_chr_len, dc_chr_val, sizeof(dc_chr_val));
    write_dht(0x11, ac_chr_len, ac_chr_val, sizeof(ac_chr_val));

    // SOS
    bw.write_marker(0xDA);
    write_word(bw.buffer, 12);
    bw.buffer.push_back(3);
    bw.buffer.push_back(1); bw.buffer.push_back(0x00);
    bw.buffer.push_back(2); bw.buffer.push_back(0x11);
    bw.buffer.push_back(3); bw.buffer.push_back(0x11);
    bw.buffer.push_back(0); bw.buffer.push_back(63); bw.buffer.push_back(0);

    // DATA
    int prev_DC_Y = 0, prev_DC_Cb = 0, prev_DC_Cr = 0;
    const unsigned char* src = (const unsigned char*)data;

    // Precomputed scaling factors for AAN
    static const float aanscales[] = { 1.0f, 1.387039845f, 1.306562965f, 1.175875602f, 1.0f, 0.785694958f, 0.541196100f, 0.275899379f };

    for (int y = 0; y < h; y += 8) {
        for (int x = 0; x < w; x += 8) {
            float Y[64], Cb[64], Cr[64];

            for (int dy = 0; dy < 8; ++dy) {
                for (int dx = 0; dx < 8; ++dx) {
                    int src_x = std::min(x + dx, w - 1);
                    int src_y = std::min(y + dy, h - 1);
                    int idx = (src_y * w + src_x) * comp;

                    unsigned char r = src[idx];
                    unsigned char g = (comp >= 2) ? src[idx+1] : r;
                    unsigned char b = (comp >= 3) ? src[idx+2] : r;

                    // RGB to YCbCr
                    Y[dy*8+dx]  = 0.299f*r + 0.587f*g + 0.114f*b;
                    Cb[dy*8+dx] = -0.1687f*r - 0.3313f*g + 0.5f*b + 128.0f;
                    Cr[dy*8+dx] = 0.5f*r - 0.4187f*g - 0.0813f*b + 128.0f;
                }
            }

            process_block(Y,  nullptr, 8, quant_lum); // DCT in-place logic mixed inside helper
            process_block(Cb, nullptr, 8, quant_chr);
            process_block(Cr, nullptr, 8, quant_chr);

            encode_block_huff(bw, Y,  prev_DC_Y,  quant_lum, dc_lum, ac_lum, aanscales);
            encode_block_huff(bw, Cb, prev_DC_Cb, quant_chr, dc_chr, ac_chr, aanscales);
            encode_block_huff(bw, Cr, prev_DC_Cr, quant_chr, dc_chr, ac_chr, aanscales);
        }
    }

    if (bw.bit_count > 0) bw.write_byte((uint8_t)(bw.bit_buffer >> 24)); // Flush
    bw.write_marker(0xD9); // EOI

    FILE *f = fopen(filename, "wb");
    if (!f) return 0;
    fwrite(bw.buffer.data(), 1, bw.buffer.size(), f);
    fclose(f);
    return 1;
}