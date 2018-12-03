#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "miniz.h"

/* テスト用のZIPイメージ（の配列）を読み込む */
#include "test_zip.h"

void *extract_zip_to_heap(
        const char *zip_image, 
        size_t zip_image_size, 
        const char *file_name, 
        size_t *extracted_size
){
    mz_zip_archive zip_archive;
    void *p;

    memset(&zip_archive, 0, sizeof(zip_archive));
    mz_zip_reader_init_mem(&zip_archive, zip_image, zip_image_size, 0);
    p = mz_zip_reader_extract_file_to_heap(&zip_archive, file_name, extracted_size, 0);
    mz_zip_reader_end(&zip_archive);

    return p;
}

void extract_zip_to_memory(
        const char *zip_image,
        size_t zip_image_size,
        const char *file_name,
        size_t *extracted_size,
        void *extract_buffer,
        size_t extract_buffer_size
){
    mz_zip_archive zip_archive;
    mz_zip_archive_file_stat file_stat;
    mz_uint32 file_index;

    memset(&zip_archive, 0, sizeof(zip_archive));
    mz_zip_reader_init_mem(&zip_archive, zip_image, zip_image_size, 0);
    mz_zip_reader_locate_file_v2(&zip_archive, file_name, NULL, 0, &file_index);
    mz_zip_reader_file_stat(&zip_archive, file_index, &file_stat);  /* file_stat.m_uncomp_sizeが展開後のサイズ */
    *extracted_size = file_stat.m_uncomp_size;
    mz_zip_reader_extract_to_mem(&zip_archive, file_index, extract_buffer, extract_buffer_size, 0);
    mz_zip_reader_end(&zip_archive);
}

#define DISP_HEAP_MEMORY

size_t zip_read_func(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n){
#ifdef DISP_HEAP_MEMORY
    printf("zip_read_func  : Free Heap Size = %u\n", esp_get_free_heap_size());
    printf("zip_read_func  : Minimum Free Heap Size = %u\n", esp_get_minimum_free_heap_size());
#endif
    memcpy(pBuf, test_zip + file_ofs, n);
    return n;
}

size_t zip_write_func(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n){
#ifdef DISP_HEAP_MEMORY
    printf("zip_write_func : Free Heap Size = %u\n", esp_get_free_heap_size());
    printf("zip_write_func : Minimum Free Heap Size = %u\n", esp_get_minimum_free_heap_size());
#else
    int i;
    for(i = 0; i < n; i++) putchar(((char *)pBuf)[i]);
#endif
    return n;
}

void extract_zip(
        size_t zip_image_size,
        const char *file_name,
        size_t *extracted_size
){
    mz_zip_archive zip_archive;
    mz_zip_archive_file_stat file_stat;
    mz_uint32 file_index;

    memset(&zip_archive, 0, sizeof(zip_archive));
    zip_archive.m_pRead = zip_read_func;
    mz_zip_reader_init(&zip_archive, zip_image_size, 0);
    mz_zip_reader_locate_file_v2(&zip_archive, file_name, NULL, 0, &file_index);
    mz_zip_reader_file_stat(&zip_archive, file_index, &file_stat);  /* file_stat.m_uncomp_sizeが展開後のサイズ */
    *extracted_size = file_stat.m_uncomp_size;
    mz_zip_reader_extract_to_callback(&zip_archive, file_index, zip_write_func, NULL, 0);
    mz_zip_reader_end(&zip_archive);
}

void app_main(){
    /* TEST1 : ヒープメモリに展開 */
    {
        size_t extracted_size;
        char *p;
        int i;

        p = (char *)extract_zip_to_heap(test_zip, sizeof(test_zip), "test.txt", &extracted_size);
        for(i = 0; i < extracted_size; i++) putchar(p[i]);
        free(p);
    }

    /* TEST2 : 確保済みメモリに展開 */
    {
        size_t extracted_size;
        char *buffer;
        const size_t buffer_size = 24 * 1024;

        buffer = malloc(buffer_size);
        extract_zip_to_memory(test_zip, sizeof(test_zip), "test.txt", &extracted_size, (void *)buffer, buffer_size);
        buffer[extracted_size] = 0;
        printf(buffer);
        free(buffer);
    }

    /* TEST3 : コールバックの利用 */
    {
        size_t extracted_size;

#ifdef DISP_HEAP_MEMORY
        printf("Before  : Free Heap Size = %u\n", esp_get_free_heap_size());
        printf("Before  : Minimum Free Heap Size = %u\n", esp_get_minimum_free_heap_size());
#endif
        extract_zip(sizeof(test_zip), "test.txt", &extracted_size);
        printf("\nExtracted size : %u\n", extracted_size);
#ifdef DISP_HEAP_MEMORY
        printf("After   : Free Heap Size = %u\n", esp_get_free_heap_size());
        printf("After   : Minimum Free Heap Size = %u\n", esp_get_minimum_free_heap_size());
#endif
    }

    while(1){
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
