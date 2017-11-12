//
//  structs.h
//  TwistedMind2
//
//  Created by noname on 26/10/17.
//  Copyright (c) 2017 noname. All rights reserved.
//

#include <sys/types.h>

typedef struct partition_config {
    
    uint64_t size_in_bytes;
    uint64_t size_in_blocks;
    
    uint64_t first_block;
    uint64_t last_block;
    
    uint8_t use_emf;
    uint8_t use_max;
    
} partition_config;

typedef struct configuration {
    
    partition_config partition_config[3];
    
} configuration;