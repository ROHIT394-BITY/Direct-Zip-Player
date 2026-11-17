#include "zip_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ZIP_LOCAL_HEADER_SIG         0x04034B50
#define ZIP_CENTRAL_DIR_HEADER_SIG   0x02014B50
#define ZIP_END_OF_CENTRAL_DIR_SIG   0x06054B50

struct ZipArchive {
    char archive_path[1024];
    uint8_t *raw_buffer;
    size_t file_size;
    bool owns_buffer;
    
    ZipEntry *entries;
    int total_entries;
    
    ZipEntry **mp4_entries;
    int mp4_count;
    
    uint64_t total_uncompressed_bytes;
};

static inline uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool zip_engine_is_mp4_filename(const char *filename) {
    if (!filename) return false;
    size_t len = strlen(filename);
    if (len < 4) return false;
    
    const char *dot = strrchr(filename, '.');
    if (!dot) return false;
    
    char ext[16] = {0};
    size_t ext_len = strlen(dot);
    if (ext_len >= sizeof(ext)) return false;
    
    for (size_t i = 0; i < ext_len; i++) {
        ext[i] = (char)tolower((unsigned char)dot[i]);
    }
    
    return (strcmp(ext, ".mp4") == 0 ||
            strcmp(ext, ".m4v") == 0 ||
            strcmp(ext, ".mov") == 0 ||
            strcmp(ext, ".mkv") == 0);
}

/* Fast CRC32 */
static uint32_t s_crc_table[256];
static int s_crc_ready = 0;

static void init_crc_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320L ^ (c >> 1)) : (c >> 1);
        }
        s_crc_table[i] = c;
    }
    s_crc_ready = 1;
}

static uint32_t compute_crc32(uint32_t crc, const uint8_t *buf, size_t len) {
    if (!s_crc_ready) init_crc_table();
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc = s_crc_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

/* RFC 1951 Deflate Implementation */
typedef struct {
    const uint8_t *src;
    size_t src_len;
    size_t src_pos;
    uint32_t bit_buf;
    int bit_count;
} BitStream;

static inline void init_bitstream(BitStream *bs, const uint8_t *src, size_t len) {
    bs->src = src;
    bs->src_len = len;
    bs->src_pos = 0;
    bs->bit_buf = 0;
    bs->bit_count = 0;
}

static inline uint32_t get_bits(BitStream *bs, int n) {
    while (bs->bit_count < n) {
        if (bs->src_pos >= bs->src_len) return 0;
        bs->bit_buf |= ((uint32_t)bs->src[bs->src_pos++]) << bs->bit_count;
        bs->bit_count += 8;
    }
    uint32_t res = bs->bit_buf & ((1U << n) - 1);
    bs->bit_buf >>= n;
    bs->bit_count -= n;
    return res;
}

#define MAX_BITS 16
#define MAX_L_CODES 288
#define MAX_D_CODES 32

typedef struct {
    uint16_t counts[MAX_BITS + 1];
    uint16_t symbols[MAX_L_CODES];
} HuffmanTree;

static int build_huffman_tree(HuffmanTree *tree, const uint8_t *lens, int num_codes) {
    memset(tree->counts, 0, sizeof(tree->counts));
    for (int i = 0; i < num_codes; i++) {
        if (lens[i] > MAX_BITS) return -1;
        tree->counts[lens[i]]++;
    }
    tree->counts[0] = 0;

    uint16_t offsets[MAX_BITS + 1];
    offsets[1] = 0;
    for (int i = 1; i < MAX_BITS; i++) {
        offsets[i + 1] = offsets[i] + tree->counts[i];
    }

    for (int i = 0; i < num_codes; i++) {
        if (lens[i] != 0) {
            tree->symbols[offsets[lens[i]]++] = (uint16_t)i;
        }
    }
    return 0;
}

static inline int decode_symbol(BitStream *bs, const HuffmanTree *tree) {
    int code = 0;
    int first = 0;
    int index = 0;
    for (int len = 1; len <= MAX_BITS; len++) {
        code |= (int)get_bits(bs, 1);
        int count = tree->counts[len];
        if (code - count < first) {
            return tree->symbols[index + (code - first)];
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

static const uint16_t s_len_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const uint8_t s_len_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
static const uint16_t s_dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const uint8_t s_dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};
static const uint8_t s_cl_order[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static int inflate_raw(const uint8_t *src, size_t src_len,
                       uint8_t *dest, size_t dest_capacity, size_t *actual_out,
                       ZipProgressCallback progress_cb, void *user_data) {
    BitStream bs;
    init_bitstream(&bs, src, src_len);
    size_t out_pos = 0;
    int is_final = 0;

    while (!is_final) {
        is_final = (int)get_bits(&bs, 1);
        int btype = (int)get_bits(&bs, 2);

        if (btype == 0) { // Uncompressed block
            bs.bit_buf = 0;
            bs.bit_count = 0;
            if (bs.src_pos + 4 > bs.src_len) return -1;
            uint16_t len = (uint16_t)(bs.src[bs.src_pos] | (bs.src[bs.src_pos + 1] << 8));
            bs.src_pos += 4;
            if (bs.src_pos + len > bs.src_len) return -2;
            if (out_pos + len > dest_capacity) return -3;
            memcpy(dest + out_pos, bs.src + bs.src_pos, len);
            bs.src_pos += len;
            out_pos += len;
        } else if (btype == 1 || btype == 2) {
            HuffmanTree lt, dt;
            if (btype == 1) { // Fixed Huffman
                uint8_t l_lens[288];
                for (int i = 0; i <= 143; i++) l_lens[i] = 8;
                for (int i = 144; i <= 255; i++) l_lens[i] = 9;
                for (int i = 256; i <= 279; i++) l_lens[i] = 7;
                for (int i = 280; i <= 287; i++) l_lens[i] = 8;
                build_huffman_tree(&lt, l_lens, 288);
                uint8_t d_lens[32];
                for (int i = 0; i < 32; i++) d_lens[i] = 5;
                build_huffman_tree(&dt, d_lens, 32);
            } else { // Dynamic Huffman
                int hlit = (int)get_bits(&bs, 5) + 257;
                int hdist = (int)get_bits(&bs, 5) + 1;
                int hclen = (int)get_bits(&bs, 4) + 4;
                uint8_t cl_lens[19] = {0};
                for (int i = 0; i < hclen; i++) cl_lens[s_cl_order[i]] = (uint8_t)get_bits(&bs, 3);
                HuffmanTree clt;
                build_huffman_tree(&clt, cl_lens, 19);

                uint8_t all_lens[MAX_L_CODES + MAX_D_CODES] = {0};
                int total_codes = hlit + hdist;
                int idx = 0;
                while (idx < total_codes) {
                    int sym = decode_symbol(&bs, &clt);
                    if (sym < 0) return -4;
                    if (sym < 16) {
                        all_lens[idx++] = (uint8_t)sym;
                    } else if (sym == 16) {
                        if (idx == 0) return -5;
                        uint8_t prev = all_lens[idx - 1];
                        int rep = (int)get_bits(&bs, 2) + 3;
                        while (rep-- > 0 && idx < total_codes) all_lens[idx++] = prev;
                    } else if (sym == 17) {
                        int rep = (int)get_bits(&bs, 3) + 3;
                        while (rep-- > 0 && idx < total_codes) all_lens[idx++] = 0;
                    } else if (sym == 18) {
                        int rep = (int)get_bits(&bs, 7) + 11;
                        while (rep-- > 0 && idx < total_codes) all_lens[idx++] = 0;
                    }
                }
                build_huffman_tree(&lt, all_lens, hlit);
                build_huffman_tree(&dt, all_lens + hlit, hdist);
            }

            // Decompress LZ77 symbols
            while (1) {
                int sym = decode_symbol(&bs, &lt);
                if (sym < 0) return -6;
                if (sym == 256) break; // End of block
                if (sym < 256) {
                    if (out_pos >= dest_capacity) return -7;
                    dest[out_pos++] = (uint8_t)sym;
                } else {
                    int l_idx = sym - 257;
                    if (l_idx >= 29) return -8;
                    int length = s_len_base[l_idx] + (int)get_bits(&bs, s_len_extra[l_idx]);
                    int d_sym = decode_symbol(&bs, &dt);
                    if (d_sym < 0 || d_sym >= 30) return -9;
                    int dist = s_dist_base[d_sym] + (int)get_bits(&bs, s_dist_extra[d_sym]);

                    if ((size_t)dist > out_pos || out_pos + length > dest_capacity) return -10;
                    for (int k = 0; k < length; k++) {
                        dest[out_pos] = dest[out_pos - dist];
                        out_pos++;
                    }
                }
            }
        } else {
            return -11; // Reserved / invalid block type
        }

        if (progress_cb && dest_capacity > 0) {
            float p = (float)out_pos / (float)dest_capacity;
            progress_cb(p > 1.0f ? 1.0f : p, "Decompressing...", user_data);
        }
    }

    *actual_out = out_pos;
    return 0;
}

ZipArchive* zip_engine_open(const char *path) {
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (sz < 22) {
        fclose(f);
        return NULL;
    }
    
    uint8_t *raw = (uint8_t*)malloc((size_t)sz);
    if (!raw) {
        fclose(f);
        return NULL;
    }
    
    size_t rd = fread(raw, 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) {
        free(raw);
        return NULL;
    }
    
    // Parse EOCD
    size_t max_search = (size_t)sz > 65557 ? 65557 : (size_t)sz;
    size_t eocd_pos = 0;
    bool found_eocd = false;
    
    for (size_t i = (size_t)sz - 22; i >= (size_t)sz - max_search; i--) {
        if (read_u32(raw + i) == ZIP_END_OF_CENTRAL_DIR_SIG) {
            eocd_pos = i;
            found_eocd = true;
            break;
        }
        if (i == 0) break;
    }
    
    if (!found_eocd) {
        free(raw);
        return NULL;
    }
    
    uint16_t total_entries = read_u16(raw + eocd_pos + 10);
    uint32_t cd_size = read_u32(raw + eocd_pos + 12);
    uint32_t cd_offset = read_u32(raw + eocd_pos + 16);
    
    if (cd_offset + cd_size > (size_t)sz) {
        free(raw);
        return NULL;
    }
    
    ZipArchive *archive = (ZipArchive*)calloc(1, sizeof(ZipArchive));
    if (!archive) {
        free(raw);
        return NULL;
    }
    
    strncpy(archive->archive_path, path, sizeof(archive->archive_path) - 1);
    archive->raw_buffer = raw;
    archive->file_size = (size_t)sz;
    archive->owns_buffer = true;
    archive->entries = (ZipEntry*)calloc(total_entries, sizeof(ZipEntry));
    if (!archive->entries && total_entries > 0) {
        free(raw);
        free(archive);
        return NULL;
    }
    
    size_t cur = cd_offset;
    int entry_count = 0;
    int mp4_count = 0;
    
    while (cur < cd_offset + cd_size && entry_count < total_entries) {
        if (cur + 46 > (size_t)sz) break;
        if (read_u32(raw + cur) != ZIP_CENTRAL_DIR_HEADER_SIG) break;
        
        uint16_t method = read_u16(raw + cur + 10);
        uint32_t comp_sz = read_u32(raw + cur + 20);
        uint32_t uncomp_sz = read_u32(raw + cur + 24);
        uint16_t name_len = read_u16(raw + cur + 28);
        uint16_t extra_len = read_u16(raw + cur + 30);
        uint16_t comment_len = read_u16(raw + cur + 32);
        uint32_t local_hdr_offset = read_u32(raw + cur + 42);
        
        ZipEntry *e = &archive->entries[entry_count];
        e->file_index = (uint32_t)entry_count;
        e->compression_method = method;
        e->compressed_size = comp_sz;
        e->uncompressed_size = uncomp_sz;
        
        size_t copy_len = name_len < (sizeof(e->filename) - 1) ? name_len : (sizeof(e->filename) - 1);
        memcpy(e->filename, raw + cur + 46, copy_len);
        e->filename[copy_len] = '\0';
        
        // Extract display name (basename)
        const char *base = strrchr(e->filename, '/');
        if (!base) base = strrchr(e->filename, '\\');
        base = base ? base + 1 : e->filename;
        strncpy(e->display_name, base, sizeof(e->display_name) - 1);
        
        // Check if directory
        bool is_dir = (copy_len > 0 && (e->filename[copy_len - 1] == '/' || e->filename[copy_len - 1] == '\\'));
        e->is_mp4 = !is_dir && zip_engine_is_mp4_filename(e->display_name);
        e->duration_sec = 0.0;
        
        archive->total_uncompressed_bytes += uncomp_sz;
        if (e->is_mp4) mp4_count++;
        
        cur += 46 + name_len + extra_len + comment_len;
        entry_count++;
    }
    
    archive->total_entries = entry_count;
    archive->mp4_count = mp4_count;
    
    if (mp4_count > 0) {
        archive->mp4_entries = (ZipEntry**)calloc(mp4_count, sizeof(ZipEntry*));
        int m_idx = 0;
        for (int i = 0; i < entry_count; i++) {
            if (archive->entries[i].is_mp4) {
                archive->mp4_entries[m_idx++] = &archive->entries[i];
            }
        }
    }
    
    return archive;
}

void zip_engine_close(ZipArchive *archive) {
    if (!archive) return;
    if (archive->entries) {
        free(archive->entries);
        archive->entries = NULL;
    }
    if (archive->mp4_entries) {
        free(archive->mp4_entries);
        archive->mp4_entries = NULL;
    }
    if (archive->owns_buffer && archive->raw_buffer) {
        free(archive->raw_buffer);
        archive->raw_buffer = NULL;
    }
    free(archive);
}

int zip_engine_get_total_entries(const ZipArchive *archive) {
    return archive ? archive->total_entries : 0;
}

int zip_engine_get_mp4_count(const ZipArchive *archive) {
    return archive ? archive->mp4_count : 0;
}

const ZipEntry* zip_engine_get_mp4_entry(const ZipArchive *archive, int index) {
    if (!archive || index < 0 || index >= archive->mp4_count) return NULL;
    return archive->mp4_entries[index];
}

const char* zip_engine_get_path(const ZipArchive *archive) {
    return archive ? archive->archive_path : "";
}

MemoryBuffer* zip_engine_decompress_entry(ZipArchive *archive, 
                                          const ZipEntry *entry,
                                          ZipProgressCallback progress_cb,
                                          void *user_data) {
    if (!archive || !entry || !archive->raw_buffer) return NULL;
    
    const uint8_t *raw = archive->raw_buffer;
    size_t sz = archive->file_size;
    
    // Locate local file header to find data offset
    // Central directory gives local header offset:
    // Let's locate the entry in archive->entries
    size_t local_hdr_offset = 0;
    bool found_entry = false;
    
    // Find local header offset from central directory
    size_t cd_offset = read_u32(raw + sz - 22 + 16); // quick ref from EOCD or locate
    // Or parse from CD:
    for (size_t cur = 0; cur + 46 < sz; ) {
        if (read_u32(raw + cur) == ZIP_CENTRAL_DIR_HEADER_SIG) {
            uint16_t n_len = read_u16(raw + cur + 28);
            uint16_t ex_len = read_u16(raw + cur + 30);
            uint16_t cm_len = read_u16(raw + cur + 32);
            if (cur + 46 + n_len <= sz && memcmp(raw + cur + 46, entry->filename, n_len) == 0 && strlen(entry->filename) == n_len) {
                local_hdr_offset = read_u32(raw + cur + 42);
                found_entry = true;
                break;
            }
            cur += 46 + n_len + ex_len + cm_len;
        } else {
            cur++;
        }
    }
    
    if (!found_entry || local_hdr_offset + 30 > sz) {
        fprintf(stderr, "[ZipEngine] Failed to locate local header for %s\n", entry->filename);
        return NULL;
    }
    
    if (read_u32(raw + local_hdr_offset) != ZIP_LOCAL_HEADER_SIG) {
        fprintf(stderr, "[ZipEngine] Invalid local header signature\n");
        return NULL;
    }
    
    uint16_t loc_name_len = read_u16(raw + local_hdr_offset + 26);
    uint16_t loc_extra_len = read_u16(raw + local_hdr_offset + 28);
    size_t data_offset = local_hdr_offset + 30 + loc_name_len + loc_extra_len;
    
    if (data_offset + entry->compressed_size > sz) {
        fprintf(stderr, "[ZipEngine] Compressed data extends past file boundary\n");
        return NULL;
    }
    
    // Allocate memory buffer via memory_manager
    MemoryBuffer *mem_buf = memory_buffer_alloc((size_t)entry->uncompressed_size);
    if (!mem_buf) {
        fprintf(stderr, "[ZipEngine] Failed to allocate in-memory buffer of size %llu\n", (unsigned long long)entry->uncompressed_size);
        return NULL;
    }
    
    const uint8_t *src = raw + data_offset;
    size_t src_len = (size_t)entry->compressed_size;
    
    if (entry->compression_method == 0) { // Stored (no compression)
        if (progress_cb) progress_cb(0.1f, "Copying uncompressed stream...", user_data);
        memcpy(mem_buf->data, src, (size_t)entry->uncompressed_size);
        mem_buf->size = (size_t)entry->uncompressed_size;
        if (progress_cb) progress_cb(1.0f, "Decompressed", user_data);
    } else if (entry->compression_method == 8) { // Deflate
        size_t actual_out = 0;
        int res = inflate_raw(src, src_len, mem_buf->data, (size_t)entry->uncompressed_size, &actual_out, progress_cb, user_data);
        if (res != 0) {
            fprintf(stderr, "[ZipEngine] Inflate failed with code %d\n", res);
            memory_buffer_free(mem_buf);
            return NULL;
        }
        mem_buf->size = actual_out;
        if (progress_cb) progress_cb(1.0f, "Decompressed", user_data);
    } else {
        fprintf(stderr, "[ZipEngine] Unsupported compression method: %u\n", entry->compression_method);
        memory_buffer_free(mem_buf);
        return NULL;
    }
    
    return mem_buf;
}
