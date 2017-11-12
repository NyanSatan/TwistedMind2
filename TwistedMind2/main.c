//
//  main.m
//  TwistedMind2
//
//  Created by noname on 23/10/17.
//  Copyright (c) 2017 noname. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <zlib.h>
#include <sys/stat.h>
#include "structs.h"
#include "partition_schemes_structs.h"

uint64_t hfs_resize(uint64_t size, const char* path, uint32_t blocksize);

int gpt_fd;
uint64_t last_usable_block;

configuration config;
GPTHeader gpt_header;
GPTPartitionArray gpt_partition_array;

void usage() {
    
    printf("usage: TwistedMind2 -d1 <new_data_partition_size> -s2 <second_system_partition_size> -d2 <second_data_partition_size | max> [--emf]\n");
}

int configure(int argc, char * argv[]) {
    
    int fail_count = 0;
    
    for (int i = 1; i < argc; i++) {
        
        if (strcmp(argv[i], "-d1") == 0) {
            config.partition_config[0].size_in_bytes = strtoll(argv[i+1], NULL, 10);
            if (config.partition_config[0].size_in_bytes == 0) {
                
                fail_count++;
            }
        }
        
        if (strcmp(argv[i], "-s2") == 0) {
            config.partition_config[1].size_in_bytes = strtoll(argv[i+1], NULL, 10);
            if (config.partition_config[1].size_in_bytes == 0) {
                
                fail_count++;
            }
        }
        
        if (strcmp(argv[i], "-d2") == 0) {
            config.partition_config[2].size_in_bytes = strtoll(argv[i+1], NULL, 10);
            if (config.partition_config[2].size_in_bytes == 0) {
                
                if (strcmp(argv[i+1], "max") == 0) {
                    
                    config.partition_config[2].use_max = 1;
                    
                } else {
                
                    fail_count++;
                }
            }
        }
        
        
        if (strcmp(argv[i], "--emf") == 0) {
            config.partition_config[2].use_emf = 1;
        }
        
    }
    
    if (fail_count > 0) {
        usage();
        return fail_count;
    }
    
    
    const char *incorrect_conf_str = "Incorrect configuration:";
    
    if (config.partition_config[0].size_in_bytes % BLOCKSIZE != 0) {
        printf("%s new Data partition size must be multiple of blocksize (%d bytes)\n", incorrect_conf_str, BLOCKSIZE);
        fail_count++;
    } else {
        config.partition_config[0].size_in_blocks = config.partition_config[0].size_in_bytes/BLOCKSIZE;
    }
    
    if (config.partition_config[1].size_in_bytes % BLOCKSIZE != 0) {
        printf("%s second System partition size must be multiple of blocksize (%d bytes)\n", incorrect_conf_str, BLOCKSIZE);
        fail_count++;
    } else {
        config.partition_config[1].size_in_blocks = config.partition_config[1].size_in_bytes/BLOCKSIZE;
    }
    
    if (config.partition_config[2].size_in_bytes % BLOCKSIZE != 0) {
        printf("%s second Data partition size must be multiple of blocksize (%d bytes)\n", incorrect_conf_str, BLOCKSIZE);
        fail_count++;
    } else {
        config.partition_config[2].size_in_blocks = config.partition_config[2].size_in_bytes/BLOCKSIZE;
    }
    
    if (fail_count > 0) {
        return fail_count;
    }
    
    config.partition_config[0].last_block = gpt_partition_array.partition[1].firstBlock + config.partition_config[0].size_in_blocks;
    
    config.partition_config[1].first_block = config.partition_config[0].last_block + 1;
    config.partition_config[1].last_block = config.partition_config[1].first_block + config.partition_config[1].size_in_blocks;
    
    config.partition_config[2].first_block = config.partition_config[1].last_block + 1;
    
    if (config.partition_config[2].use_max) {
        config.partition_config[2].last_block = last_usable_block;
        config.partition_config[2].size_in_blocks = last_usable_block - config.partition_config[2].first_block;
        config.partition_config[2].size_in_bytes = config.partition_config[2].size_in_blocks*BLOCKSIZE;
    } else {
        config.partition_config[2].last_block = config.partition_config[2].first_block + config.partition_config[2].size_in_blocks;
    }
    
    
    
    if (config.partition_config[2].last_block > last_usable_block) {
        uint64_t difference = config.partition_config[2].last_block - last_usable_block;
        printf("%s your new partitions take %llu more blocks (%llu bytes) than your NAND has\n", incorrect_conf_str, difference, difference*BLOCKSIZE);
        fail_count++;
        return fail_count;
    }
    
    printf("Configuration:\n\nNew size of first Data: %llu bytes (%llu blocks)\nSize of second System: %llu bytes (%llu blocks)\nSize of second Data: %llu bytes (%llu blocks)\n", config.partition_config[0].size_in_bytes, config.partition_config[0].size_in_blocks, config.partition_config[1].size_in_bytes, config.partition_config[1].size_in_blocks, config.partition_config[2].size_in_bytes, config.partition_config[2].size_in_blocks);
    if (config.partition_config[2].use_emf) printf("EMF signature will be used for second Data partition\n");
    
    printf("\n");
    
    return fail_count;
}

int main(int argc, char * argv[]) {
    
    if (argc < 7) {
        usage();
        return 0;
    }
    
    printf("Twisted Mind v0.2\n");

    gpt_fd = open("/dev/rdisk0", O_RDONLY | O_SHLOCK);
    //gpt_fd = open("/Users/noname/Tables/RealGPT62", O_RDONLY | O_SHLOCK);
    if (gpt_fd < 0) {
        printf("Failed to open GPT!\n");
        return 0;
    }
    
    uint32_t blocksize;
    ioctl(gpt_fd, DKIOCGETBLOCKSIZE, &blocksize);
    printf("Blocksize: %u\n", blocksize);
    if (blocksize != BLOCKSIZE) {
        printf("\nThis is only for iOS 4 compatible devices!\n");
        return 0;
    }
    
    uint8_t partition_table_buffer[BLOCKSIZE*4];
    
    printf("Reading GPT...\n");
    if (pread(gpt_fd, partition_table_buffer, BLOCKSIZE*4, 0) == -1) {
        printf("Failed to read GPT!\n");
        return 0;
    }
    
    uint8_t potential_lwvm[BLOCKSIZE];
    pread(gpt_fd, potential_lwvm, BLOCKSIZE, 0);
    if (memcmp(potential_lwvm, LwVMSignature, sizeof(LwVMSignature)) == 0 || memcmp(potential_lwvm, LwVMSignature_noCRC, sizeof(LwVMSignature)) == 0) {
        printf("This is actually LwVM!\n");
        return 0;
    }
    
    uint8_t potential_gpt[BLOCKSIZE];
    potential_gpt[8] = '\0';
    pread(gpt_fd, potential_gpt, BLOCKSIZE, BLOCKSIZE);
    if (memcmp(potential_gpt, GPTSignature, sizeof(GPTSignature)) != 0) {
        printf("This isn't actually GPT!\n");
        return 0;
    }
    
    memmove(&gpt_header, &partition_table_buffer[GPTOFFSET], GPTHEADERSIZE);
    gpt_header.header_crc32 = 0;
    
    if (gpt_header.revision != 0x10000) {
        printf("GPT revision must be 0x10000, read 0x%x\n", gpt_header.revision);
        return 0;
    }
    
    if (gpt_header.num_of_partitions != 128) {
        printf("Number of partitions must be 128, read %x, will change it\n", gpt_header.num_of_partitions);
        gpt_header.num_of_partitions = 128;
    }
    
    printf("%s: revision: 0x%x, header length: 0x%x, ", potential_gpt, gpt_header.revision, gpt_header.header_size);
    
    memmove(&gpt_partition_array, &partition_table_buffer[BLOCKSIZE*2], sizeof(GPTPartitionArray));
    
    uint64_t blockcount;
    ioctl(gpt_fd, DKIOCGETBLOCKCOUNT, &blockcount);
    last_usable_block = blockcount - gpt_partition_array.partition[0].firstBlock;
    printf("last usable block: %llu\n", last_usable_block);
    
    
    printf("Validating configurartion of future partitions...\n");
    if (configure(argc, argv) > 0) {
        return 0;
    }
    
    uint64_t current_data_size = (gpt_partition_array.partition[1].lastBlock - gpt_partition_array.partition[1].firstBlock)*BLOCKSIZE;
    
    if (current_data_size > config.partition_config[0].size_in_bytes) {
        
        uint64_t resize = hfs_resize(config.partition_config[0].size_in_bytes, "/private/var", BLOCKSIZE);
        if (resize == 0) return 0;
    }
    

    gpt_partition_array.partition[1].lastBlock = config.partition_config[0].last_block;
    
    memmove(&gpt_partition_array.partition[2], &gpt_partition_array.partition[0], sizeof(GPTPartition));
    gpt_partition_array.partition[2].firstBlock = config.partition_config[1].first_block;
    gpt_partition_array.partition[2].lastBlock = config.partition_config[1].last_block;
    gpt_partition_array.partition[2].guid[0]++;
    gpt_partition_array.partition[2].guid[1]++;
    memmove(&gpt_partition_array.partition[2].name, gpt_partition_name, sizeof(gpt_partition_name));
    
    memmove(&gpt_partition_array.partition[3], &gpt_partition_array.partition[1], sizeof(GPTPartition));
    gpt_partition_array.partition[3].firstBlock = config.partition_config[2].first_block;
    gpt_partition_array.partition[3].lastBlock = config.partition_config[2].last_block;
    gpt_partition_array.partition[3].guid[0]++;
    gpt_partition_array.partition[3].guid[1]++;
    memmove(&gpt_partition_array.partition[3].name, gpt_partition_name, sizeof(gpt_partition_name));
    if (!config.partition_config[2].use_emf) memmove(&gpt_partition_array.partition[3].signature, GPTHFSSignature, sizeof(GPTEMFSignature));
    if (config.partition_config[2].use_emf) memmove(&gpt_partition_array.partition[3].signature, GPTEMFSignature, sizeof(GPTEMFSignature));
    
    gpt_header.partition_array_crc32 = crc32(0, &gpt_partition_array, sizeof(GPTPartitionArray));
    printf("GPT partition array's CRC32: %X\n", gpt_header.partition_array_crc32);
    
    gpt_header.header_crc32 = crc32(0, &gpt_header, GPTHEADERSIZE);
    printf("GPT header's CRC32: %X\n", gpt_header.header_crc32);
    
    memmove(&partition_table_buffer[GPTOFFSET], &gpt_header, GPTHEADERSIZE);
    memmove(&partition_table_buffer[GPTPARTITIONSOFFSET], &gpt_partition_array, sizeof(GPTPartitionArray));
    
    char *result_template = "/TwistedMind2-";
    
    char *result_name;
    result_name = malloc(strlen(result_template)+10);
    sprintf(result_name, "%s%X", result_template, gpt_header.header_crc32);
    
    int result_fd = open(result_name, O_RDWR | O_CREAT);
    if (result_fd < 0) {
        printf("Failed to create resulting file!\n");
        return 0;
    }
    
    if (pwrite(result_fd, partition_table_buffer, BLOCKSIZE*4, 0x0) == -1) {
        printf("Failed to write to resulting file!\n");
        return 0;
        
    } else {
        chmod(result_name, 00644);
        printf("Saved to %s\n", result_name);
        printf("Done!\n");
    }

    
    return 0;
}
