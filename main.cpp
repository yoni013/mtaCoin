#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <zlib.h>

#define NUM_OF_MINERS 4
#define DIFFICULTY 24

typedef struct Block {
    int height;
    int timestamp;
    unsigned int hash;
    unsigned int prev_hash;
    int difficulty;
    int nonce;
    int relayed_by;
} Block;

typedef struct BlockDataForHash {
    int height;
    int timestamp;
    unsigned int prev_hash;
    int nonce;
    int relayed_by;
} BlockDataForHash;

typedef struct BlockNode {
    struct BlockNode* next;
    struct Block value;
} BlockNode;

typedef struct BlockChain {
    BlockNode* head;
    BlockNode* tail;
    int size;
} BlockChain;

// Global variables
BlockChain blockchain;
pthread_mutex_t g_lock;
pthread_cond_t g_cv_new_block;
Block current_block;
int block_mined = 0;

BlockChain getEmptyBlockChain();
void addBlockToChain(BlockChain* chain, Block newBlock);
Block getLastBlock(BlockChain* chain);
void createGenesisBlock(BlockChain *chain);
int isValidBlock(BlockChain chain, Block blockToCheck);
void printNewBlock(Block block);
void updateChain(BlockChain* chain, Block block);
Block createBlock(BlockDataForHash blockData, unsigned int hash);
unsigned int calcHash(BlockDataForHash data);
bool isValidHash(unsigned int hash, int difficulty);
BlockDataForHash getDataForHash(Block block);

void *serverThread(void *arg);
void *minerThread(void *arg);

int main() {
    pthread_t server_tid, miner_tids[NUM_OF_MINERS];
    int miner_ids[NUM_OF_MINERS] = {1, 2, 3, 4};

    // Initialize mutex and condition variable
    pthread_mutex_init(&g_lock, NULL);
    pthread_cond_init(&g_cv_new_block, NULL);

    // Initialize blockchain and create the genesis block
    blockchain = getEmptyBlockChain();
    createGenesisBlock(&blockchain);
    current_block = getLastBlock(&blockchain);

    // Create threads
    pthread_create(&server_tid, NULL, serverThread, NULL);
    for (int i = 0; i < NUM_OF_MINERS; i++) {
        pthread_create(&miner_tids[i], NULL, minerThread, &miner_ids[i]);
    }

    // Wait for threads to finish
    pthread_join(server_tid, NULL);
    for (int i = 0; i < NUM_OF_MINERS; i++) {
        pthread_join(miner_tids[i], NULL);
    }

    // Cleanup
    pthread_mutex_destroy(&g_lock);
    pthread_cond_destroy(&g_cv_new_block);

    return 0;
}

void *serverThread(void *arg) {
    while (1) {
        pthread_mutex_lock(&g_lock);
        while (!block_mined) {
            pthread_cond_wait(&g_cv_new_block, &g_lock);
        }

        block_mined = 0;
        if (isValidBlock(blockchain, current_block)) {
            updateChain(&blockchain, current_block);
            Block new_block = getLastBlock(&blockchain);
            new_block.height;
            new_block.prev_hash = new_block.hash;
            new_block.timestamp = time(NULL);
            new_block.nonce = 0;
            new_block.relayed_by = -1;
            current_block = new_block;

        } else {
            printf("Server: Invalid block detected from miner #%d\n", current_block.relayed_by);
        }
        pthread_mutex_unlock(&g_lock);
        pthread_cond_broadcast(&g_cv_new_block);
        sleep(1); // Simulate delay
    }
}

void *minerThread(void *arg) {
    int miner_id = *(int *)arg;
    while (1) {
        pthread_mutex_lock(&g_lock);
        while (block_mined) {
            pthread_cond_wait(&g_cv_new_block, &g_lock);
        }

        BlockDataForHash blockData;
        blockData.height = current_block.height + 1;
        blockData.timestamp = current_block.timestamp;
        blockData.prev_hash = current_block.hash;  // Correctly set prev_hash to current block's hash
        blockData.nonce = current_block.nonce;
        blockData.relayed_by = miner_id;

        unsigned int hash;
        while (1) {
            blockData.nonce++;
            hash = calcHash(blockData);
            if (isValidHash(hash, DIFFICULTY)) {
                current_block = createBlock(blockData, hash);
                block_mined = 1;
                printf("Miner #%d: Mined a new block, hash 0x%04x\n", miner_id, hash);
                pthread_cond_signal(&g_cv_new_block);
                break;
            }
        }
        pthread_mutex_unlock(&g_lock);

        sleep(1); // Simulate delay
    }
}

Block createBlock(BlockDataForHash blockData, unsigned int hash) {
    Block newBlock;
    newBlock.hash = hash;
    newBlock.difficulty = DIFFICULTY;
    newBlock.timestamp = blockData.timestamp;
    newBlock.prev_hash = blockData.prev_hash;
    newBlock.relayed_by = blockData.relayed_by;
    newBlock.height = blockData.height;
    newBlock.nonce = blockData.nonce;
    return newBlock;
}

BlockChain getEmptyBlockChain() {
    BlockChain chain;
    chain.head = chain.tail = NULL;
    chain.size = 0;
    return chain;
}

void addBlockToChain(BlockChain* chain, Block newBlock) {
    BlockNode* node = (BlockNode*)malloc(sizeof(BlockNode));
    node->value = newBlock;
    node->next = NULL;
    if (chain->tail != NULL)
        chain->tail->next = node;
    chain->tail = node;
    if (chain->head == NULL)
        chain->head = node;
    (chain->size)++;
}

Block getLastBlock(BlockChain* chain) {
    return chain->tail->value;
}

void printBlock(Block block, const char* str) {
    printf("%s Block: height = %d, timestamp = %d, prev_hash = %d, nonce = %d, relayed_by = %d, hash = %d\n",
           str, block.height, block.timestamp, block.prev_hash, block.nonce, block.relayed_by, block.hash);
}

unsigned int calcHash(BlockDataForHash data) {
    int len = sizeof(BlockDataForHash);
    char buffer[sizeof(BlockDataForHash) + 1];
    memset(buffer, 0x00, sizeof(BlockDataForHash) + 1);
    memcpy(buffer, &data, sizeof(BlockDataForHash));
    return crc32(0, (const Bytef*)buffer, len);
}

bool isValidHash(unsigned int hash, int difficulty) {
    // Count leading zero bits
    int leadingZeros = 0;
    for (int i = 31; i >= 0; i--) {  // Check each bit from MSB to LSB
        if ((hash & (1 << i)) == 0) {
            leadingZeros++;
        } else {
            break;
        }
    }
//
//    printf("hash: %x\n", hash);
//    printf("number of leading 0 bits: %d\n", leadingZeros);

    // Check if leading zeros meet the difficulty requirement
    return leadingZeros >= difficulty;
}

int isValidBlock(BlockChain chain, Block blockToCheck) {
    BlockDataForHash data;
    int hash;
    Block L_Block = getLastBlock(&chain);
//    printf("last hash: %x, last prev hash: %x, last height: %d\n", L_Block.hash, L_Block.prev_hash, L_Block.height);
//    printf("current hash: %x, current prev hash: %x, current height: %d\n", blockToCheck.hash, blockToCheck.prev_hash, blockToCheck.height);
    if (L_Block.height + 1 == blockToCheck.height) {
        if (L_Block.hash == blockToCheck.prev_hash) {
            if (isValidHash(blockToCheck.hash, DIFFICULTY)) {
                data = getDataForHash(blockToCheck);
                hash = calcHash(data);
                if (hash == blockToCheck.hash) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

void printNewBlock(Block block) {
    printf("Server: New block added by %d, attributes: height (%d), timestamp(%d), hash(0x%04x), prev_hash(0x%04x), difficulty(%d), nonce(%d)\n",
           block.relayed_by, block.height, block.timestamp, block.hash, block.prev_hash, block.difficulty, block.nonce);
}

void updateChain(BlockChain* chain, Block block) {
    printNewBlock(block);
    addBlockToChain(chain, block);
}

void createGenesisBlock(BlockChain *chain) {
    Block genBlock;
    BlockDataForHash data;
    memset(&genBlock, 0x00, sizeof(Block));
    genBlock.timestamp = (int)time(NULL);
    genBlock.nonce = 20;
    data = getDataForHash(genBlock);
    genBlock.hash = calcHash(data);
//    printf("genBlock: New block added by %d, attributes: height (%d), timestamp(%d), hash(0x%04x), prev_hash(0x%04x), difficulty(%d), nonce(%d)\n",
//           genBlock.relayed_by, genBlock.height, genBlock.timestamp, genBlock.hash, genBlock.prev_hash, genBlock.difficulty, genBlock.nonce);
    addBlockToChain(chain, genBlock);
}

BlockDataForHash getDataForHash(Block block) {
    BlockDataForHash data;
    data.height = block.height;
    data.timestamp = block.timestamp;
    data.prev_hash = block.prev_hash;
    data.nonce = block.nonce;
    data.relayed_by = block.relayed_by;
    return data;
}