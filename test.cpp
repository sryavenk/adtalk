#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Configuration for RAW PCM Audio Data ---
#define BUFFER_SECONDS 120
#define RATE 44100
#define CHANNELS 2
#define BYTES_PER_SAMPLE 2 // 16-bit audio

// Calculate buffer size in bytes
const size_t BUFFER_SIZE_BYTES = BUFFER_SECONDS * RATE * CHANNELS * BYTES_PER_SAMPLE;
#define CHUNK_SIZE_BYTES 4096 // Read 4KB at a time

// --- WAV Header Structure ---
// This struct represents the 44-byte header required for a WAV file.
typedef struct {
    // RIFF Chunk
    char     riff_chunk_id[4]; // "RIFF"
    uint32_t riff_chunk_size;
    char     wave_format[4];   // "WAVE"

    // "fmt " sub-chunk
    char     fmt_chunk_id[4]; // "fmt "
    uint32_t fmt_chunk_size;  // 16 for PCM
    uint16_t audio_format;    // 1 for PCM
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;

    // "data" sub-chunk
    char     data_chunk_id[4]; // "data"
    uint32_t data_chunk_size;
} WavHeader;


// Forward declarations
void save_buffer_to_wav_file(const char* filename, const char* buffer, size_t buffer_size, size_t start_index);
void write_wav_header(FILE* file, size_t total_audio_bytes);

/**
 * @brief Reads a raw audio file into a circular buffer.
 * @param filename The path to the raw audio file.
 */
void process_raw_audio_to_buffer(const char *filename) {
    // 1. Setup Circular Buffer in RAM
    char *ring_buffer = (char *)malloc(BUFFER_SIZE_BYTES);
    if (!ring_buffer) {
        perror("Failed to allocate ring buffer");
        return;
    }
    printf("Allocated a %zu MB circular buffer for %d seconds of audio.\n", 
           BUFFER_SIZE_BYTES / (1024 * 1024), BUFFER_SECONDS);

    size_t write_index = 0;
    char read_chunk[CHUNK_SIZE_BYTES];

    // 2. Open the Audio File
    FILE *audio_file = fopen(filename, "rb");
    if (!audio_file) {
        perror("Could not open audio file");
        free(ring_buffer);
        return;
    }

    // 3. File Reading and Buffer Filling Loop
    size_t bytes_read;
    printf("Starting to read file '%s' and fill circular buffer...\n", filename);

    while ((bytes_read = fread(read_chunk, 1, CHUNK_SIZE_BYTES, audio_file)) > 0) {
        if (write_index + bytes_read <= BUFFER_SIZE_BYTES) {
            memcpy(ring_buffer + write_index, read_chunk, bytes_read);
        } else {
            size_t bytes_to_end = BUFFER_SIZE_BYTES - write_index;
            size_t bytes_from_start = bytes_read - bytes_to_end;
            memcpy(ring_buffer + write_index, read_chunk, bytes_to_end);
            memcpy(ring_buffer, read_chunk + bytes_to_end, bytes_from_start);
        }
        write_index = (write_index + bytes_read) % BUFFER_SIZE_BYTES;
    }

    if (ferror(audio_file)) {
        perror("Error reading from file");
    }

    // 4. Final Status and Saving the Buffer
    printf("Finished reading file.\n");
    printf("The oldest data starts at byte index: %zu\n", write_index);

    // Save the contents of the ring buffer to a new .wav file.
    save_buffer_to_wav_file("output.wav", ring_buffer, BUFFER_SIZE_BYTES, write_index);

    // 5. Cleanup
    fclose(audio_file);
    free(ring_buffer);
}

/**
 * @brief Saves the circular buffer to a new, valid .wav file with a proper header.
 * @param filename The name of the file to save (e.g., "output.wav").
 * @param buffer The circular buffer containing the audio data.
 * @param buffer_size The total size of the circular buffer.
 * @param start_index The index where the oldest data begins.
 */
void save_buffer_to_wav_file(const char* filename, const char* buffer, size_t buffer_size, size_t start_index) {
    FILE* out_file = fopen(filename, "wb");
    if (!out_file) {
        perror("Could not open output file for writing");
        return;
    }

    // 1. Write the WAV header first.
    write_wav_header(out_file, buffer_size);

    // 2. Write the audio data from the circular buffer in the correct order.
    // Part A: Write the block from the start_index to the physical end of the buffer.
    size_t first_chunk_size = buffer_size - start_index;
    if (fwrite(buffer + start_index, 1, first_chunk_size, out_file) != first_chunk_size) {
        perror("Failed to write first chunk to output file");
        fclose(out_file);
        return;
    }

    // Part B: Write the block from the beginning of the buffer up to the start_index.
    size_t second_chunk_size = start_index;
    if (fwrite(buffer, 1, second_chunk_size, out_file) != second_chunk_size) {
        perror("Failed to write second chunk to output file");
        fclose(out_file);
        return;
    }

    printf("Successfully saved buffer content to '%s'.\n", filename);
    fclose(out_file);
}

/**
 * @brief Writes a 44-byte WAV file header.
 * @param file A file pointer opened in binary write mode.
 * @param total_audio_bytes The size of the raw PCM data that will follow the header.
 */
void write_wav_header(FILE* file, size_t total_audio_bytes) {
    WavHeader header;
    
    // RIFF Chunk
    memcpy(header.riff_chunk_id, "RIFF", 4);
    header.riff_chunk_size = 36 + total_audio_bytes;
    memcpy(header.wave_format, "WAVE", 4);
    
    // "fmt " sub-chunk
    memcpy(header.fmt_chunk_id, "fmt ", 4);
    header.fmt_chunk_size = 16;
    header.audio_format = 1; // 1 = PCM (uncompressed)
    header.num_channels = CHANNELS;
    header.sample_rate = RATE;
    header.bits_per_sample = BYTES_PER_SAMPLE * 8;
    header.byte_rate = RATE * CHANNELS * BYTES_PER_SAMPLE;
    header.block_align = CHANNELS * BYTES_PER_SAMPLE;
    
    // "data" sub-chunk
    memcpy(header.data_chunk_id, "data", 4);
    header.data_chunk_size = total_audio_bytes;
    
    fwrite(&header, sizeof(WavHeader), 1, file);
}

// Example usage:
int main() {
    // IMPORTANT: Replace "test.raw" with the path to an actual
    // raw PCM audio file that you want to process.
    process_raw_audio_to_buffer("test.raw");
    return 0;
}

