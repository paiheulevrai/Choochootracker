#include <taktful/wav_io.h>
#include <taktful/mem_io.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace taktful {
namespace {

uint16_t rd_u16(const unsigned char* p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

uint32_t rd_u32(const unsigned char* p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

uint64_t rd_u64(const unsigned char* p) {
    return static_cast<uint64_t>(rd_u32(p))
         | (static_cast<uint64_t>(rd_u32(p + 4)) << 32);
}

int32_t rd_i24(const unsigned char* p) {
    int32_t v = static_cast<int32_t>(p[0])
              | (static_cast<int32_t>(p[1]) << 8)
              | (static_cast<int32_t>(p[2]) << 16);
    if (v & 0x800000) {
        v |= static_cast<int32_t>(0xFF000000);
    }
    return v;
}

WavError decode_pcm(const unsigned char* data_bytes,
                    size_t data_size,
                    uint16_t audio_format,
                    uint16_t channels,
                    uint16_t bits_per_sample,
                    uint32_t sample_rate,
                    WavData& out) {
    if (channels == 0 || sample_rate == 0) {
        return WavError::NotAWav;
    }
    if (audio_format != 1 && audio_format != 3) {
        return WavError::UnsupportedFormat;
    }
    if (audio_format == 3 && bits_per_sample != 32) {
        return WavError::UnsupportedBitDepth;
    }
    if (audio_format == 1 &&
        bits_per_sample != 8 && bits_per_sample != 16 &&
        bits_per_sample != 24 && bits_per_sample != 32) {
        return WavError::UnsupportedBitDepth;
    }
    if (data_size == 0) {
        return WavError::Truncated;
    }

    const uint32_t bytes_per_sample = bits_per_sample / 8;
    const uint32_t frame_bytes = bytes_per_sample * channels;
    if (frame_bytes == 0) {
        return WavError::UnsupportedFormat;
    }
    const uint32_t n_frames = static_cast<uint32_t>(data_size / frame_bytes);
    if (n_frames == 0) {
        return WavError::Truncated;
    }

    out.sample_rate = sample_rate;
    out.channels = channels;
    out.bits_per_sample = bits_per_sample;
    try {
        out.samples.resize(n_frames);
    } catch (...) {
        return WavError::OutOfMemory;
    }

    const unsigned char* p = data_bytes;
    for (uint32_t i = 0; i < n_frames; ++i) {
        double acc = 0.0;
        for (uint16_t c = 0; c < channels; ++c) {
            const unsigned char* s = p + static_cast<size_t>(c) * bytes_per_sample;
            float v = 0.0f;
            if (audio_format == 3) {
                float fv = 0.0f;
                std::memcpy(&fv, s, 4);
                v = fv;
            } else if (bits_per_sample == 8) {
                v = (static_cast<float>(s[0]) - 128.0f) / 128.0f;
            } else if (bits_per_sample == 16) {
                const int16_t iv = static_cast<int16_t>(rd_u16(s));
                v = static_cast<float>(iv) / 32768.0f;
            } else if (bits_per_sample == 24) {
                const int32_t iv = rd_i24(s);
                v = static_cast<float>(iv) / 8388608.0f;
            } else {
                const int32_t iv = static_cast<int32_t>(rd_u32(s));
                v = static_cast<float>(iv) / 2147483648.0f;
            }
            acc += static_cast<double>(v);
        }
        float mixed = static_cast<float>(acc / static_cast<double>(channels));
        if (mixed > 1.0f) mixed = 1.0f;
        if (mixed < -1.0f) mixed = -1.0f;
        out.samples[i] = mixed;
        p += frame_bytes;
    }
    return WavError::OK;
}

WavError parse_wav(const unsigned char* buf, size_t nbytes, WavData& out) {
    out = WavData();
    if (!buf || nbytes < 12) {
        return WavError::Truncated;
    }
    const bool is_rf64 = std::memcmp(buf, "RF64", 4) == 0;
    const bool is_riff = std::memcmp(buf, "RIFF", 4) == 0;
    if ((!is_rf64 && !is_riff) || std::memcmp(buf + 8, "WAVE", 4) != 0) {
        return WavError::NotAWav;
    }

    uint16_t audio_format = 0;
    uint16_t channels = 0;
    uint32_t sample_rate = 0;
    uint16_t bits_per_sample = 0;
    bool have_fmt = false;
    const unsigned char* data_ptr = 0;
    size_t data_size = 0;
    uint64_t ds64_data_size = 0;
    bool have_ds64 = false;

    size_t pos = 12;
    while (pos + 8 <= nbytes) {
        const unsigned char* hdr = buf + pos;
        uint64_t chunk_size = rd_u32(hdr + 4);
        pos += 8;
        if (is_rf64 && chunk_size == 0xFFFFFFFFu && have_ds64 && std::memcmp(hdr, "data", 4) == 0) {
            chunk_size = ds64_data_size;
        }
        if (pos + chunk_size > nbytes) {
            if (std::memcmp(hdr, "data", 4) == 0) {
                data_ptr = buf + pos;
                data_size = nbytes - pos;
                break;
            }
            return WavError::Truncated;
        }

        if (std::memcmp(hdr, "ds64", 4) == 0) {
            if (chunk_size < 28) {
                return WavError::NotAWav;
            }
            ds64_data_size = rd_u64(buf + pos + 8);
            have_ds64 = true;
        } else if (std::memcmp(hdr, "fmt ", 4) == 0) {
            if (chunk_size < 16) {
                return WavError::NotAWav;
            }
            const unsigned char* fmt = buf + pos;
            audio_format = rd_u16(fmt);
            channels = rd_u16(fmt + 2);
            sample_rate = rd_u32(fmt + 4);
            bits_per_sample = rd_u16(fmt + 14);
            if (audio_format == 0xFFFE) {
                if (chunk_size < 40) {
                    return WavError::UnsupportedFormat;
                }
                const unsigned char* guid = fmt + 24;
                static const unsigned char pcm_guid[16] = {
                    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
                    0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
                };
                static const unsigned char flt_guid[16] = {
                    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
                    0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
                };
                if (std::memcmp(guid, pcm_guid, 16) == 0) {
                    audio_format = 1;
                } else if (std::memcmp(guid, flt_guid, 16) == 0) {
                    audio_format = 3;
                } else {
                    return WavError::UnsupportedFormat;
                }
            }
            have_fmt = true;
        } else if (std::memcmp(hdr, "data", 4) == 0) {
            data_ptr = buf + pos;
            data_size = static_cast<size_t>(chunk_size);
        }

        pos += static_cast<size_t>(chunk_size);
        if (chunk_size & 1u) {
            ++pos;
        }
    }

    if (!have_fmt) {
        return WavError::NotAWav;
    }
    if (!data_ptr) {
        return WavError::Truncated;
    }
    return decode_pcm(data_ptr, data_size, audio_format, channels, bits_per_sample, sample_rate, out);
}

} // namespace

WavError load_wav_mem(const void* data, size_t nbytes, WavData& out) {
    return parse_wav(static_cast<const unsigned char*>(data), nbytes, out);
}

WavError load_wav(const char* path, WavData& out) {
    out = WavData();
    if (!path) {
        return WavError::FileNotFound;
    }
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        return WavError::FileNotFound;
    }
    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        return WavError::Truncated;
    }
    const long sz = std::ftell(f);
    if (sz < 0) {
        std::fclose(f);
        return WavError::Truncated;
    }
    std::rewind(f);
    std::vector<unsigned char> buf;
    try {
        buf.resize(static_cast<size_t>(sz));
    } catch (...) {
        std::fclose(f);
        return WavError::OutOfMemory;
    }
    const size_t nread = std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    buf.resize(nread);
    return parse_wav(buf.data(), buf.size(), out);
}

} // namespace taktful
